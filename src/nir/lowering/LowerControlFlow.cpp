#include "neuronc/nir/NIRBuilder.h"

#include "../detail/NIRBuilderShared.h"

#include <algorithm>

namespace neuron::nir {

namespace {

Value *buildMatchArmCondition(NIRBuilder *builder,
                              const std::vector<Value *> &matchValues,
                              MatchArmNode *matchArm) {
  if (builder == nullptr || matchArm == nullptr || matchValues.empty()) {
    return nullptr;
  }

  if (matchArm->patternExprs.size() != matchValues.size()) {
    return new ConstantInt(0);
  }

  const std::size_t pairCount =
      std::min(matchValues.size(), matchArm->patternExprs.size());
  if (pairCount == 0) {
    return new ConstantInt(0);
  }

  Value *combined = nullptr;
  for (std::size_t i = 0; i < pairCount; ++i) {
    Value *armValue = builder->buildExpression(matchArm->patternExprs[i].get());
    if (armValue == nullptr) {
      return nullptr;
    }

    Instruction *isMatch = builder->createInst(InstKind::Eq, NType::makeBool());
    isMatch->addOperand(matchValues[i]);
    isMatch->addOperand(armValue);

    if (combined == nullptr) {
      combined = isMatch;
    } else {
      Instruction *allMatch =
          builder->createInst(InstKind::Mul, NType::makeBool());
      allMatch->addOperand(combined);
      allMatch->addOperand(isMatch);
      combined = allMatch;
    }
  }

  return combined != nullptr ? combined : new ConstantInt(0);
}

} // namespace

void NIRBuilder::buildIfStmt(IfStmtNode *node) {
  Value *cond = buildExpression(node->condition.get());

  Block *thenBlock = m_currentFunction->createBlock(nextBlockName() + "_then");
  Block *elseBlock =
      node->elseBlock
          ? m_currentFunction->createBlock(nextBlockName() + "_else")
          : nullptr;
  Block *mergeBlock =
      m_currentFunction->createBlock(nextBlockName() + "_merge");

  Instruction *br = createInst(InstKind::CondBr, NType::makeVoid(), "");
  br->addOperand(cond);
  br->addOperand(new BlockRef(thenBlock));
  br->addOperand(new BlockRef(elseBlock ? elseBlock : mergeBlock));

  m_currentBlock = thenBlock;
  visitBlock(static_cast<BlockNode *>(node->thenBlock.get()));
  if (!detail::blockHasTerminator(m_currentBlock)) {
    Instruction *thenBr = createInst(InstKind::Br, NType::makeVoid(), "");
    thenBr->addOperand(new BlockRef(mergeBlock));
  }

  if (elseBlock) {
    m_currentBlock = elseBlock;
    if (node->elseBlock->type == ASTNodeType::IfStmt) {
      buildIfStmt(static_cast<IfStmtNode *>(node->elseBlock.get()));
    } else {
      visitBlock(static_cast<BlockNode *>(node->elseBlock.get()));
    }
    if (!detail::blockHasTerminator(m_currentBlock)) {
      Instruction *elseBr = createInst(InstKind::Br, NType::makeVoid(), "");
      elseBr->addOperand(new BlockRef(mergeBlock));
    }
  }

  m_currentBlock = mergeBlock;
}

void NIRBuilder::buildMatchStmt(MatchStmtNode *node) {
  if (node == nullptr || node->expressions.empty() || node->arms.empty()) {
    return;
  }

  std::vector<Value *> matchValues;
  matchValues.reserve(node->expressions.size());
  for (const auto &expr : node->expressions) {
    Value *matchValue = buildExpression(expr.get());
    if (matchValue == nullptr) {
      return;
    }
    matchValues.push_back(matchValue);
  }

  Block *exitBlock =
      m_currentFunction->createBlock(nextBlockName() + "_match_exit");

  std::vector<Block *> checkBlocks;
  std::vector<Block *> bodyBlocks;
  Block *defaultBlock = nullptr;

  checkBlocks.reserve(node->arms.size());
  bodyBlocks.reserve(node->arms.size());

  for (const auto &armNode : node->arms) {
    auto *matchArm = static_cast<MatchArmNode *>(armNode.get());
    if (!matchArm->isDefault) {
      checkBlocks.push_back(
          m_currentFunction->createBlock(nextBlockName() + "_match_check"));
    }
    bodyBlocks.push_back(
        m_currentFunction->createBlock(nextBlockName() + "_match_arm"));
    if (matchArm->isDefault) {
      defaultBlock = bodyBlocks.back();
    }
  }

  if (!checkBlocks.empty()) {
    Instruction *toFirstCheck = createInst(InstKind::Br, NType::makeVoid(), "");
    toFirstCheck->addOperand(new BlockRef(checkBlocks.front()));
  } else if (defaultBlock != nullptr) {
    Instruction *toDefault = createInst(InstKind::Br, NType::makeVoid(), "");
    toDefault->addOperand(new BlockRef(defaultBlock));
  } else {
    Instruction *toExit = createInst(InstKind::Br, NType::makeVoid(), "");
    toExit->addOperand(new BlockRef(exitBlock));
  }

  size_t checkIndex = 0;
  for (size_t i = 0; i < node->arms.size(); ++i) {
    auto *matchArm = static_cast<MatchArmNode *>(node->arms[i].get());
    if (matchArm->isDefault) {
      continue;
    }

    m_currentBlock = checkBlocks[checkIndex];
    Value *isMatch = buildMatchArmCondition(this, matchValues, matchArm);
    if (isMatch == nullptr) {
      return;
    }

    Block *nextTarget = nullptr;
    if (checkIndex + 1 < checkBlocks.size()) {
      nextTarget = checkBlocks[checkIndex + 1];
    } else if (defaultBlock != nullptr) {
      nextTarget = defaultBlock;
    } else {
      nextTarget = exitBlock;
    }

    Instruction *branch = createInst(InstKind::CondBr, NType::makeVoid(), "");
    branch->addOperand(isMatch);
    branch->addOperand(new BlockRef(bodyBlocks[i]));
    branch->addOperand(new BlockRef(nextTarget));
    ++checkIndex;
  }

  m_breakTargetStack.push_back(exitBlock);
  for (size_t i = 0; i < node->arms.size(); ++i) {
    auto *matchArm = static_cast<MatchArmNode *>(node->arms[i].get());
    m_currentBlock = bodyBlocks[i];
    if (matchArm->body) {
      visitBlock(static_cast<BlockNode *>(matchArm->body.get()));
    }

    if (!detail::blockHasTerminator(m_currentBlock)) {
      Instruction *jump = createInst(InstKind::Br, NType::makeVoid(), "");
      jump->addOperand(new BlockRef(exitBlock));
    }
  }
  m_breakTargetStack.pop_back();

  m_currentBlock = exitBlock;
}

Value *NIRBuilder::buildMatchExpr(MatchExprNode *node) {
  if (node == nullptr || node->expressions.empty() || node->arms.empty()) {
    return nullptr;
  }

  std::vector<Value *> matchValues;
  matchValues.reserve(node->expressions.size());
  for (const auto &expr : node->expressions) {
    Value *matchValue = buildExpression(expr.get());
    if (matchValue == nullptr) {
      return nullptr;
    }
    matchValues.push_back(matchValue);
  }

  Value *resultPtr = createInst(InstKind::Alloca,
                                NType::makePointer(NType::makeUnknown()),
                                nextValName() + "_match_result_ptr");
  Block *exitBlock =
      m_currentFunction->createBlock(nextBlockName() + "_match_expr_exit");

  std::vector<Block *> checkBlocks;
  std::vector<Block *> bodyBlocks;
  Block *defaultBlock = nullptr;

  checkBlocks.reserve(node->arms.size());
  bodyBlocks.reserve(node->arms.size());

  for (const auto &armNode : node->arms) {
    auto *matchArm = static_cast<MatchArmNode *>(armNode.get());
    if (!matchArm->isDefault) {
      checkBlocks.push_back(
          m_currentFunction->createBlock(nextBlockName() + "_match_expr_check"));
    }
    bodyBlocks.push_back(
        m_currentFunction->createBlock(nextBlockName() + "_match_expr_arm"));
    if (matchArm->isDefault) {
      defaultBlock = bodyBlocks.back();
    }
  }

  if (!checkBlocks.empty()) {
    Instruction *toFirstCheck = createInst(InstKind::Br, NType::makeVoid(), "");
    toFirstCheck->addOperand(new BlockRef(checkBlocks.front()));
  } else if (defaultBlock != nullptr) {
    Instruction *toDefault = createInst(InstKind::Br, NType::makeVoid(), "");
    toDefault->addOperand(new BlockRef(defaultBlock));
  } else {
    Instruction *toExit = createInst(InstKind::Br, NType::makeVoid(), "");
    toExit->addOperand(new BlockRef(exitBlock));
  }

  size_t checkIndex = 0;
  for (size_t i = 0; i < node->arms.size(); ++i) {
    auto *matchArm = static_cast<MatchArmNode *>(node->arms[i].get());
    if (matchArm->isDefault) {
      continue;
    }

    m_currentBlock = checkBlocks[checkIndex];
    Value *isMatch = buildMatchArmCondition(this, matchValues, matchArm);
    if (isMatch == nullptr) {
      return nullptr;
    }

    Block *nextTarget = nullptr;
    if (checkIndex + 1 < checkBlocks.size()) {
      nextTarget = checkBlocks[checkIndex + 1];
    } else if (defaultBlock != nullptr) {
      nextTarget = defaultBlock;
    } else {
      nextTarget = exitBlock;
    }

    Instruction *branch = createInst(InstKind::CondBr, NType::makeVoid(), "");
    branch->addOperand(isMatch);
    branch->addOperand(new BlockRef(bodyBlocks[i]));
    branch->addOperand(new BlockRef(nextTarget));
    ++checkIndex;
  }

  for (size_t i = 0; i < node->arms.size(); ++i) {
    auto *matchArm = static_cast<MatchArmNode *>(node->arms[i].get());
    m_currentBlock = bodyBlocks[i];
    if (matchArm->valueExpr != nullptr) {
      Value *armValue = buildExpression(matchArm->valueExpr.get());
      if (armValue == nullptr) {
        return nullptr;
      }
      Instruction *store = createInst(InstKind::Store, NType::makeVoid(), "");
      store->addOperand(armValue);
      store->addOperand(resultPtr);
    }

    if (!detail::blockHasTerminator(m_currentBlock)) {
      Instruction *jump = createInst(InstKind::Br, NType::makeVoid(), "");
      jump->addOperand(new BlockRef(exitBlock));
    }
  }

  m_currentBlock = exitBlock;
  Instruction *load =
      createInst(InstKind::Load, NType::makeUnknown(),
                 nextValName() + "_match_result");
  load->addOperand(resultPtr);
  return load;
}

} // namespace neuron::nir
