#include "neuronc/nir/NIRBuilder.h"

#include "../detail/NIRBuilderShared.h"

#include <iostream>

namespace neuron::nir {

void NIRBuilder::buildWhileStmt(WhileStmtNode *node) {
  Block *condBlock = m_currentFunction->createBlock(nextBlockName() + "_cond");
  Block *bodyBlock = m_currentFunction->createBlock(nextBlockName() + "_body");
  Block *exitBlock = m_currentFunction->createBlock(nextBlockName() + "_exit");

  Instruction *goCond = createInst(InstKind::Br, NType::makeVoid(), "");
  goCond->addOperand(new BlockRef(condBlock));

  m_currentBlock = condBlock;
  Value *cond = buildExpression(node->condition.get());
  Instruction *cbr = createInst(InstKind::CondBr, NType::makeVoid(), "");
  cbr->addOperand(cond);
  cbr->addOperand(new BlockRef(bodyBlock));
  cbr->addOperand(new BlockRef(exitBlock));

  m_currentBlock = bodyBlock;
  m_breakTargetStack.push_back(exitBlock);
  m_continueTargetStack.push_back(condBlock);
  visitBlock(static_cast<BlockNode *>(node->body.get()));
  m_continueTargetStack.pop_back();
  m_breakTargetStack.pop_back();
  if (!detail::blockHasTerminator(m_currentBlock)) {
    Instruction *goBack = createInst(InstKind::Br, NType::makeVoid(), "");
    goBack->addOperand(new BlockRef(condBlock));
  }

  m_currentBlock = exitBlock;
}

void NIRBuilder::buildReturnStmt(ReturnStmtNode *node) {
  Value *rv = nullptr;
  if (node->value) {
    rv = buildExpression(node->value.get());
  }

  unwindGpuScopesForReturn();

  Instruction *ret = createInst(InstKind::Ret, NType::makeVoid(), "");
  if (rv)
    ret->addOperand(rv);
}

void NIRBuilder::buildForStmt(ForStmtNode *node) {
  if (node->init) {
    if (node->init->type == ASTNodeType::BindingDecl) {
      visitBindingDecl(static_cast<BindingDeclNode *>(node->init.get()));
    } else if (node->init->type == ASTNodeType::CastStmt) {
      visitCastStmt(static_cast<CastStmtNode *>(node->init.get()));
    } else {
      buildExpression(node->init.get());
    }
  }

  Block *condBlock = m_currentFunction->createBlock(nextBlockName() + "_for_cond");
  Block *bodyBlock = m_currentFunction->createBlock(nextBlockName() + "_for_body");
  Block *incrementBlock =
      m_currentFunction->createBlock(nextBlockName() + "_for_inc");
  Block *exitBlock = m_currentFunction->createBlock(nextBlockName() + "_for_exit");

  Instruction *goCond = createInst(InstKind::Br, NType::makeVoid(), "");
  goCond->addOperand(new BlockRef(condBlock));

  m_currentBlock = condBlock;
  Value *cond = node->condition ? buildExpression(node->condition.get()) : nullptr;
  if (cond) {
    Instruction *cbr = createInst(InstKind::CondBr, NType::makeVoid(), "");
    cbr->addOperand(cond);
    cbr->addOperand(new BlockRef(bodyBlock));
    cbr->addOperand(new BlockRef(exitBlock));
  } else {
    Instruction *br = createInst(InstKind::Br, NType::makeVoid(), "");
    br->addOperand(new BlockRef(bodyBlock));
  }

  m_currentBlock = bodyBlock;
  m_breakTargetStack.push_back(exitBlock);
  m_continueTargetStack.push_back(incrementBlock);
  if (node->body) {
    visitBlock(static_cast<BlockNode *>(node->body.get()));
  }
  m_continueTargetStack.pop_back();
  m_breakTargetStack.pop_back();

  if (!detail::blockHasTerminator(m_currentBlock)) {
    Instruction *toIncrement = createInst(InstKind::Br, NType::makeVoid(), "");
    toIncrement->addOperand(new BlockRef(incrementBlock));
  }

  m_currentBlock = incrementBlock;
  if (node->increment) {
    if (node->increment->type == ASTNodeType::IncrementStmt) {
      buildIncrementStmt(static_cast<IncrementStmtNode *>(node->increment.get()));
    } else if (node->increment->type == ASTNodeType::DecrementStmt) {
      buildDecrementStmt(static_cast<DecrementStmtNode *>(node->increment.get()));
    } else {
      buildExpression(node->increment.get());
    }
  }

  if (!detail::blockHasTerminator(m_currentBlock)) {
    Instruction *goBack = createInst(InstKind::Br, NType::makeVoid(), "");
    goBack->addOperand(new BlockRef(condBlock));
  }

  m_currentBlock = exitBlock;
}

void NIRBuilder::buildBreakStmt(BreakStmtNode *node) {
  (void)node;
  if (m_breakTargetStack.empty()) {
    std::cerr << "NIRBuilder: break used outside a breakable block" << std::endl;
    return;
  }

  unwindGpuScopesForBreak();

  Instruction *jump = createInst(InstKind::Br, NType::makeVoid(), "");
  jump->addOperand(new BlockRef(m_breakTargetStack.back()));
}

void NIRBuilder::buildContinueStmt(ContinueStmtNode *node) {
  (void)node;
  if (m_continueTargetStack.empty()) {
    std::cerr << "NIRBuilder: continue used outside a loop" << std::endl;
    return;
  }

  unwindGpuScopesForContinue();

  Instruction *jump = createInst(InstKind::Br, NType::makeVoid(), "");
  jump->addOperand(new BlockRef(m_continueTargetStack.back()));
}

void NIRBuilder::buildIncrementStmt(IncrementStmtNode *node) {
  Value *ptr = lookupSymbol(node->variable);
  if (!ptr) {
    std::cerr << "NIRBuilder: Undefined variable for increment: "
              << node->variable << std::endl;
    return;
  }

  Instruction *load =
      createInst(InstKind::Load, NType::makeInt(), node->variable + "_inc_val");
  load->addOperand(ptr);

  Instruction *addOne = createInst(InstKind::Add, NType::makeInt());
  addOne->addOperand(load);
  addOne->addOperand(new ConstantInt(1));

  Instruction *store = createInst(InstKind::Store, NType::makeVoid(), "");
  store->addOperand(addOne);
  store->addOperand(ptr);
}

void NIRBuilder::buildDecrementStmt(DecrementStmtNode *node) {
  Value *ptr = lookupSymbol(node->variable);
  if (!ptr) {
    std::cerr << "NIRBuilder: Undefined variable for decrement: "
              << node->variable << std::endl;
    return;
  }

  Instruction *load =
      createInst(InstKind::Load, NType::makeInt(), node->variable + "_dec_val");
  load->addOperand(ptr);

  Instruction *subOne = createInst(InstKind::Sub, NType::makeInt());
  subOne->addOperand(load);
  subOne->addOperand(new ConstantInt(1));

  Instruction *store = createInst(InstKind::Store, NType::makeVoid(), "");
  store->addOperand(subOne);
  store->addOperand(ptr);
}

void NIRBuilder::buildThrowStmt(ThrowStmtNode *node) {
  Value *errorVal = nullptr;
  if (node && node->value) {
    errorVal = buildExpression(node->value.get());
  }
  if (!errorVal) {
    errorVal = new ConstantString("Unhandled Neuron++ throw");
  }

  Instruction *throwCall = createInst(InstKind::Call, NType::makeVoid(), "");
  throwCall->addOperand(new ConstantString("__neuron_throw"));
  throwCall->addOperand(errorVal);

  if (!m_throwTargetStack.empty()) {
    Instruction *jump = createInst(InstKind::Br, NType::makeVoid(), "");
    jump->addOperand(new BlockRef(m_throwTargetStack.back()));
  }
}

void NIRBuilder::buildTryStmt(TryStmtNode *node) {
  if (node == nullptr || node->tryBlock == nullptr) {
    return;
  }

  Block *tryBlock = m_currentFunction->createBlock(nextBlockName() + "_try");
  Block *checkBlock = m_currentFunction->createBlock(nextBlockName() + "_try_check");
  Block *mergeBlock = m_currentFunction->createBlock(nextBlockName() + "_merge");

  std::vector<Block *> catchBlocks;
  catchBlocks.reserve(node->catchClauses.size());
  for (size_t i = 0; i < node->catchClauses.size(); ++i) {
    catchBlocks.push_back(m_currentFunction->createBlock(nextBlockName() + "_catch"));
  }

  Block *finallyBlock = nullptr;
  if (node->finallyBlock) {
    finallyBlock = m_currentFunction->createBlock(nextBlockName() + "_finally");
  }

  Instruction *toTry = createInst(InstKind::Br, NType::makeVoid(), "");
  toTry->addOperand(new BlockRef(tryBlock));

  m_currentBlock = tryBlock;
  m_throwTargetStack.push_back(checkBlock);
  visitBlock(static_cast<BlockNode *>(node->tryBlock.get()));
  m_throwTargetStack.pop_back();
  if (!detail::blockHasTerminator(m_currentBlock)) {
    Instruction *toCheck = createInst(InstKind::Br, NType::makeVoid(), "");
    toCheck->addOperand(new BlockRef(checkBlock));
  }

  m_currentBlock = checkBlock;
  Instruction *hasErrorCall = createInst(InstKind::Call, NType::makeInt(), nextValName());
  hasErrorCall->addOperand(new ConstantString("__neuron_has_exception"));

  Instruction *hasErrorCond = createInst(InstKind::Neq, NType::makeBool(), nextValName());
  hasErrorCond->addOperand(hasErrorCall);
  hasErrorCond->addOperand(new ConstantInt(0));

  Block *onErrorTarget = catchBlocks.empty()
                             ? (finallyBlock ? finallyBlock : mergeBlock)
                             : catchBlocks.front();
  Block *noErrorTarget = finallyBlock ? finallyBlock : mergeBlock;

  Instruction *branch = createInst(InstKind::CondBr, NType::makeVoid(), "");
  branch->addOperand(hasErrorCond);
  branch->addOperand(new BlockRef(onErrorTarget));
  branch->addOperand(new BlockRef(noErrorTarget));

  for (size_t i = 0; i < catchBlocks.size(); ++i) {
    m_currentBlock = catchBlocks[i];
    auto *catchNode = static_cast<CatchClauseNode *>(node->catchClauses[i].get());
    if (catchNode && catchNode->body) {
      Value *catchErrorValue = nullptr;
      if (!catchNode->errorName.empty()) {
        Instruction *lastErrorCall =
            createInst(InstKind::Call, NType::makeString(), nextValName());
        lastErrorCall->addOperand(new ConstantString("__neuron_last_exception"));
        catchErrorValue = lastErrorCall;
      }

      enterScope();
      if (catchErrorValue && !catchNode->errorName.empty()) {
        defineSymbol(catchNode->errorName, catchErrorValue);
      }
      m_throwTargetStack.push_back(mergeBlock);
      visitBlock(static_cast<BlockNode *>(catchNode->body.get()));
      m_throwTargetStack.pop_back();
      leaveScope();
    }

    if (!detail::blockHasTerminator(m_currentBlock)) {
      Instruction *clearCall = createInst(InstKind::Call, NType::makeVoid(), "");
      clearCall->addOperand(new ConstantString("__neuron_clear_exception"));

      Block *nextTarget = (i + 1 < catchBlocks.size())
                              ? catchBlocks[i + 1]
                              : (finallyBlock ? finallyBlock : mergeBlock);
      Instruction *toNext = createInst(InstKind::Br, NType::makeVoid(), "");
      toNext->addOperand(new BlockRef(nextTarget));
    }
  }

  if (finallyBlock) {
    m_currentBlock = finallyBlock;
    m_throwTargetStack.push_back(mergeBlock);
    visitBlock(static_cast<BlockNode *>(node->finallyBlock.get()));
    m_throwTargetStack.pop_back();
    if (!detail::blockHasTerminator(m_currentBlock)) {
      Instruction *toMerge = createInst(InstKind::Br, NType::makeVoid(), "");
      toMerge->addOperand(new BlockRef(mergeBlock));
    }
  }

  m_currentBlock = mergeBlock;
}

} // namespace neuron::nir

