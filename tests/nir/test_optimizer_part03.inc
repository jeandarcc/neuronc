  auto cbr = std::make_unique<nir::Instruction>(nir::InstKind::CondBr,
                                                NType::makeVoid(), "");
  cbr->addOperand(new nir::ConstantInt(1));
  cbr->addOperand(new nir::BlockRef(thenBlock));
  cbr->addOperand(new nir::BlockRef(elseBlock));
  entry->addInstruction(std::move(cbr));

  auto thenRet =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  thenBlock->addInstruction(std::move(thenRet));
  auto elseRet =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  elseBlock->addInstruction(std::move(elseRet));

  nir::BranchSimplificationPass pass;
  ASSERT_TRUE(pass.runOnModule(module.get()));

  const auto &insts = entry->getInstructions();
  ASSERT_EQ(insts.size(), 1u);
  ASSERT_EQ(insts[0]->getKind(), nir::InstKind::Br);
  return true;
}

TEST(GpuScopeLiftingPassLiftsSimpleForPattern) {
  auto module = std::make_unique<nir::Module>("gpu_scope_lift");
  nir::Function *fn = module->createFunction("Init", NType::makeVoid());
  nir::Block *entry = fn->createBlock("entry");
  nir::Block *cond = fn->createBlock("cond");
  nir::Block *body = fn->createBlock("body");
  nir::Block *inc = fn->createBlock("inc");
  nir::Block *exit = fn->createBlock("exit");

  auto brEntry =
      std::make_unique<nir::Instruction>(nir::InstKind::Br, NType::makeVoid(), "");
  brEntry->addOperand(new nir::BlockRef(cond));
  entry->addInstruction(std::move(brEntry));

  auto condBr = std::make_unique<nir::Instruction>(nir::InstKind::CondBr,
                                                   NType::makeVoid(), "");
  condBr->addOperand(new nir::ConstantInt(1));
  condBr->addOperand(new nir::BlockRef(body));
  condBr->addOperand(new nir::BlockRef(exit));
  cond->addInstruction(std::move(condBr));

  auto scopeBegin = std::make_unique<nir::Instruction>(nir::InstKind::GpuScopeBegin,
                                                       NType::makeVoid(), "");
  scopeBegin->addOperand(new nir::ConstantInt(0));
  scopeBegin->addOperand(new nir::ConstantInt(0));
  body->addInstruction(std::move(scopeBegin));

  auto scopeEnd = std::make_unique<nir::Instruction>(nir::InstKind::GpuScopeEnd,
                                                     NType::makeVoid(), "");
  body->addInstruction(std::move(scopeEnd));

  auto brBody =
      std::make_unique<nir::Instruction>(nir::InstKind::Br, NType::makeVoid(), "");
  brBody->addOperand(new nir::BlockRef(inc));
  body->addInstruction(std::move(brBody));

  auto brInc =
      std::make_unique<nir::Instruction>(nir::InstKind::Br, NType::makeVoid(), "");
  brInc->addOperand(new nir::BlockRef(cond));
  inc->addInstruction(std::move(brInc));

  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  exit->addInstruction(std::move(ret));

  nir::GpuScopeLiftingPass pass;
  ASSERT_TRUE(pass.runOnModule(module.get()));

  const auto &entryInsts = entry->getInstructions();
  ASSERT_EQ(entryInsts.size(), 2u);
  ASSERT_EQ(entryInsts[0]->getKind(), nir::InstKind::GpuScopeBegin);
  ASSERT_EQ(entryInsts[1]->getKind(), nir::InstKind::Br);

  const auto &bodyInsts = body->getInstructions();
  ASSERT_EQ(bodyInsts.size(), 1u);
  ASSERT_EQ(bodyInsts[0]->getKind(), nir::InstKind::Br);

  const auto &exitInsts = exit->getInstructions();
  ASSERT_EQ(exitInsts.size(), 2u);
  ASSERT_EQ(exitInsts[0]->getKind(), nir::InstKind::GpuScopeEnd);
  ASSERT_EQ(exitInsts[1]->getKind(), nir::InstKind::Ret);
  return true;
}

TEST(GpuScopeLiftingPassSkipsWhenLoopHasMultipleGpuScopes) {
  auto module = std::make_unique<nir::Module>("gpu_scope_lift_skip");
  nir::Function *fn = module->createFunction("Init", NType::makeVoid());
  nir::Block *entry = fn->createBlock("entry");
  nir::Block *cond = fn->createBlock("cond");
  nir::Block *body = fn->createBlock("body");
  nir::Block *inc = fn->createBlock("inc");
  nir::Block *exit = fn->createBlock("exit");

  auto brEntry =
      std::make_unique<nir::Instruction>(nir::InstKind::Br, NType::makeVoid(), "");
  brEntry->addOperand(new nir::BlockRef(cond));
  entry->addInstruction(std::move(brEntry));

  auto condBr = std::make_unique<nir::Instruction>(nir::InstKind::CondBr,
                                                   NType::makeVoid(), "");
  condBr->addOperand(new nir::ConstantInt(1));
  condBr->addOperand(new nir::BlockRef(body));
  condBr->addOperand(new nir::BlockRef(exit));
  cond->addInstruction(std::move(condBr));

  auto scopeBeginA = std::make_unique<nir::Instruction>(
      nir::InstKind::GpuScopeBegin, NType::makeVoid(), "");
  scopeBeginA->addOperand(new nir::ConstantInt(0));
  scopeBeginA->addOperand(new nir::ConstantInt(0));
  body->addInstruction(std::move(scopeBeginA));
  body->addInstruction(std::make_unique<nir::Instruction>(
      nir::InstKind::GpuScopeEnd, NType::makeVoid(), ""));

  auto scopeBeginB = std::make_unique<nir::Instruction>(
      nir::InstKind::GpuScopeBegin, NType::makeVoid(), "");
  scopeBeginB->addOperand(new nir::ConstantInt(0));
  scopeBeginB->addOperand(new nir::ConstantInt(0));
  body->addInstruction(std::move(scopeBeginB));
  body->addInstruction(std::make_unique<nir::Instruction>(
      nir::InstKind::GpuScopeEnd, NType::makeVoid(), ""));

  auto brBody =
      std::make_unique<nir::Instruction>(nir::InstKind::Br, NType::makeVoid(), "");
  brBody->addOperand(new nir::BlockRef(inc));
  body->addInstruction(std::move(brBody));

  auto brInc =
      std::make_unique<nir::Instruction>(nir::InstKind::Br, NType::makeVoid(), "");
  brInc->addOperand(new nir::BlockRef(cond));
  inc->addInstruction(std::move(brInc));

  exit->addInstruction(std::make_unique<nir::Instruction>(
      nir::InstKind::Ret, NType::makeVoid(), ""));

  nir::GpuScopeLiftingPass pass;
  ASSERT_FALSE(pass.runOnModule(module.get()));
  return true;
}

TEST(GpuScopeLiftingPassKeepsLoopTensorUpdateInsideLiftedScope) {
  auto module = std::make_unique<nir::Module>("gpu_scope_lift_tensor_update");
  auto tensorTy = NType::makeTensor(NType::makeFloat());
  nir::Function *fn = module->createFunction("Init", NType::makeVoid());
  nir::Argument *a = fn->addArgument(tensorTy, "a");
  nir::Argument *b = fn->addArgument(tensorTy, "b");
  nir::Block *entry = fn->createBlock("entry");
  nir::Block *cond = fn->createBlock("cond");
  nir::Block *body = fn->createBlock("body");
  nir::Block *inc = fn->createBlock("inc");
  nir::Block *exit = fn->createBlock("exit");

  auto brEntry =
      std::make_unique<nir::Instruction>(nir::InstKind::Br, NType::makeVoid(), "");
  brEntry->addOperand(new nir::BlockRef(cond));
  entry->addInstruction(std::move(brEntry));

  auto condBr = std::make_unique<nir::Instruction>(nir::InstKind::CondBr,
                                                   NType::makeVoid(), "");
  condBr->addOperand(new nir::ConstantInt(1));
  condBr->addOperand(new nir::BlockRef(body));
  condBr->addOperand(new nir::BlockRef(exit));
  cond->addInstruction(std::move(condBr));

  auto scopeBegin = std::make_unique<nir::Instruction>(nir::InstKind::GpuScopeBegin,
                                                       NType::makeVoid(), "");
  scopeBegin->addOperand(new nir::ConstantInt(0));
  scopeBegin->addOperand(new nir::ConstantInt(0));
  body->addInstruction(std::move(scopeBegin));

  auto tensorUpdate = std::make_unique<nir::Instruction>(
      nir::InstKind::TensorAdd, tensorTy, "loop_sum");
  tensorUpdate->setExecutionHint(nir::ExecutionHint::GpuPrefer);
  tensorUpdate->addOperand(a);
  tensorUpdate->addOperand(b);
  body->addInstruction(std::move(tensorUpdate));

  auto scopeEnd = std::make_unique<nir::Instruction>(nir::InstKind::GpuScopeEnd,
                                                     NType::makeVoid(), "");
  body->addInstruction(std::move(scopeEnd));

  auto brBody =
      std::make_unique<nir::Instruction>(nir::InstKind::Br, NType::makeVoid(), "");
  brBody->addOperand(new nir::BlockRef(inc));
  body->addInstruction(std::move(brBody));

  auto brInc =
      std::make_unique<nir::Instruction>(nir::InstKind::Br, NType::makeVoid(), "");
  brInc->addOperand(new nir::BlockRef(cond));
  inc->addInstruction(std::move(brInc));

  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  exit->addInstruction(std::move(ret));

  nir::GpuScopeLiftingPass pass;
  ASSERT_TRUE(pass.runOnModule(module.get()));

  const auto &entryInsts = entry->getInstructions();
  ASSERT_EQ(entryInsts.size(), 2u);
  ASSERT_EQ(entryInsts[0]->getKind(), nir::InstKind::GpuScopeBegin);
  ASSERT_EQ(entryInsts[1]->getKind(), nir::InstKind::Br);

  const auto &bodyInsts = body->getInstructions();
  ASSERT_EQ(bodyInsts.size(), 2u);
  ASSERT_EQ(bodyInsts[0]->getKind(), nir::InstKind::TensorAdd);
  ASSERT_EQ(bodyInsts[1]->getKind(), nir::InstKind::Br);

  const auto &exitInsts = exit->getInstructions();
  ASSERT_EQ(exitInsts.size(), 2u);
  ASSERT_EQ(exitInsts[0]->getKind(), nir::InstKind::GpuScopeEnd);
  ASSERT_EQ(exitInsts[1]->getKind(), nir::InstKind::Ret);
  return true;
}
