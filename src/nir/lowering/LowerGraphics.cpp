#include "neuronc/nir/NIRBuilder.h"

#include "../detail/NIRBuilderShared.h"

namespace neuron::nir {

void NIRBuilder::buildCanvasStmt(CanvasStmtNode *node) {
  if (node == nullptr) {
    return;
  }

  Value *windowValue = nullptr;
  if (node->windowExpr != nullptr) {
    windowValue = buildExpression(node->windowExpr.get());
  }

  Instruction *createCanvasCall =
      createInst(InstKind::Call, NType::makeDynamic(), nextValName());
  createCanvasCall->addOperand(new ConstantString("Graphics.CreateCanvas"));
  if (windowValue != nullptr) {
    createCanvasCall->addOperand(windowValue);
  }
  Value *canvasValue = createCanvasCall;

  auto callNoArgMethod = [&](const std::string &methodName) {
    Instruction *callInst = createInst(InstKind::Call, NType::makeVoid(), "");
    callInst->addOperand(new ConstantString(methodName));
  };

  auto callRuntime = [&](const std::string &name, NTypePtr retType) -> Value * {
    Instruction *callInst = createInst(InstKind::Call, retType, nextValName());
    callInst->addOperand(new ConstantString(name));
    if (canvasValue != nullptr) {
      callInst->addOperand(canvasValue);
    }
    return callInst;
  };

  auto runInlineMethod = [&](MethodDeclNode *method) {
    if (method == nullptr || method->body == nullptr ||
        method->body->type != ASTNodeType::Block) {
      return;
    }
    visitBlock(static_cast<BlockNode *>(method->body.get()));
  };

  auto runEvent = [&](CanvasEventKind kind) {
    CanvasEventHandlerNode *inlineHandler = nullptr;
    CanvasEventHandlerNode *externalHandler = nullptr;
    for (const auto &handlerNode : node->handlers) {
      if (handlerNode == nullptr ||
          handlerNode->type != ASTNodeType::CanvasEventHandler) {
        continue;
      }
      auto *handler = static_cast<CanvasEventHandlerNode *>(handlerNode.get());
      if (handler->eventKind != kind) {
        continue;
      }
      if (handler->isExternalBinding) {
        if (externalHandler == nullptr) {
          externalHandler = handler;
        }
      } else if (handler->handlerMethod &&
                 handler->handlerMethod->type == ASTNodeType::MethodDecl) {
        if (inlineHandler == nullptr) {
          inlineHandler = handler;
        }
      }
    }

    if (inlineHandler != nullptr && inlineHandler->handlerMethod != nullptr &&
        inlineHandler->handlerMethod->type == ASTNodeType::MethodDecl) {
      runInlineMethod(
          static_cast<MethodDeclNode *>(inlineHandler->handlerMethod.get()));
      return;
    }

    if (externalHandler != nullptr &&
        !externalHandler->externalMethodName.empty()) {
      callNoArgMethod(externalHandler->externalMethodName);
    }
  };

  auto hasEventHandler = [&](CanvasEventKind kind) {
    for (const auto &handlerNode : node->handlers) {
      if (handlerNode == nullptr ||
          handlerNode->type != ASTNodeType::CanvasEventHandler) {
        continue;
      }
      auto *handler = static_cast<CanvasEventHandlerNode *>(handlerNode.get());
      if (handler->eventKind == kind) {
        return true;
      }
    }
    return false;
  };

  if (hasEventHandler(CanvasEventKind::OnOpen)) {
    runEvent(CanvasEventKind::OnOpen);
  }
  if (detail::blockHasTerminator(m_currentBlock)) {
    return;
  }

  Block *pumpBlock =
      m_currentFunction->createBlock(nextBlockName() + "_canvas_pump");
  Block *resizeCheckBlock =
      m_currentFunction->createBlock(nextBlockName() + "_canvas_resize_check");
  Block *frameBlock =
      m_currentFunction->createBlock(nextBlockName() + "_canvas_frame");
  Block *closeBlock =
      m_currentFunction->createBlock(nextBlockName() + "_canvas_close");
  Block *resizeRunBlock = nullptr;
  if (hasEventHandler(CanvasEventKind::OnResize)) {
    resizeRunBlock =
        m_currentFunction->createBlock(nextBlockName() + "_canvas_resize_run");
  }

  Instruction *toPump = createInst(InstKind::Br, NType::makeVoid(), "");
  toPump->addOperand(new BlockRef(pumpBlock));

  m_currentBlock = pumpBlock;
  callRuntime("__neuron_graphics_canvas_pump", NType::makeInt());
  Value *shouldClose =
      callRuntime("__neuron_graphics_canvas_should_close", NType::makeInt());
  Instruction *closeCond =
      createInst(InstKind::Neq, NType::makeBool(), nextValName());
  closeCond->addOperand(shouldClose);
  closeCond->addOperand(new ConstantInt(0));
  Instruction *branchClose =
      createInst(InstKind::CondBr, NType::makeVoid(), "");
  branchClose->addOperand(closeCond);
  branchClose->addOperand(new BlockRef(closeBlock));
  branchClose->addOperand(new BlockRef(resizeCheckBlock));

  m_currentBlock = resizeCheckBlock;
  Value *resizeChanged =
      callRuntime("__neuron_graphics_canvas_take_resize", NType::makeInt());
  if (resizeRunBlock != nullptr) {
    Instruction *resizeCond =
        createInst(InstKind::Neq, NType::makeBool(), nextValName());
    resizeCond->addOperand(resizeChanged);
    resizeCond->addOperand(new ConstantInt(0));
    Instruction *resizeBranch =
        createInst(InstKind::CondBr, NType::makeVoid(), "");
    resizeBranch->addOperand(resizeCond);
    resizeBranch->addOperand(new BlockRef(resizeRunBlock));
    resizeBranch->addOperand(new BlockRef(frameBlock));
  } else {
    Instruction *toFrame = createInst(InstKind::Br, NType::makeVoid(), "");
    toFrame->addOperand(new BlockRef(frameBlock));
  }

  if (resizeRunBlock != nullptr) {
    m_currentBlock = resizeRunBlock;
    runEvent(CanvasEventKind::OnResize);
    if (!detail::blockHasTerminator(m_currentBlock)) {
      Instruction *toFrame = createInst(InstKind::Br, NType::makeVoid(), "");
      toFrame->addOperand(new BlockRef(frameBlock));
    }
  }

  m_currentBlock = frameBlock;
  callRuntime("__neuron_graphics_canvas_begin_frame", NType::makeVoid());
  if (hasEventHandler(CanvasEventKind::OnFrame)) {
    runEvent(CanvasEventKind::OnFrame);
  }
  if (!detail::blockHasTerminator(m_currentBlock)) {
    Instruction *presentCall =
        createInst(InstKind::Call, NType::makeVoid(), "");
    presentCall->addOperand(new ConstantString("Present"));
    callRuntime("__neuron_graphics_canvas_end_frame", NType::makeVoid());
    Instruction *loopBack = createInst(InstKind::Br, NType::makeVoid(), "");
    loopBack->addOperand(new BlockRef(pumpBlock));
  }

  m_currentBlock = closeBlock;
  if (hasEventHandler(CanvasEventKind::OnClose)) {
    runEvent(CanvasEventKind::OnClose);
  }
  if (!detail::blockHasTerminator(m_currentBlock)) {
    Instruction *freeCanvasCall =
        createInst(InstKind::Call, NType::makeVoid(), "");
    freeCanvasCall->addOperand(
        new ConstantString("__neuron_graphics_canvas_free"));
    if (canvasValue != nullptr) {
      freeCanvasCall->addOperand(canvasValue);
    }
  }
}

} // namespace neuron::nir
