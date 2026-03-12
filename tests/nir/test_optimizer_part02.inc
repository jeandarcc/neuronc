  nir::Block *entry = fn->createBlock("entry");

  auto allocaInst = std::make_unique<nir::Instruction>(
      nir::InstKind::Alloca, NType::makePointer(NType::makeInt()), "x_ptr");
  nir::Instruction *allocaPtr = allocaInst.get();
  entry->addInstruction(std::move(allocaInst));

  auto store = std::make_unique<nir::Instruction>(nir::InstKind::Store,
                                                  NType::makeVoid(), "");
  store->addOperand(new nir::ConstantInt(42));
  store->addOperand(allocaPtr);
  entry->addInstruction(std::move(store));

  auto load = std::make_unique<nir::Instruction>(nir::InstKind::Load,
                                                 NType::makeInt(), "x_val");
  load->addOperand(allocaPtr);
  nir::Instruction *loadPtr = load.get();
  entry->addInstruction(std::move(load));

  auto addZero = std::make_unique<nir::Instruction>(nir::InstKind::Add,
                                                    NType::makeInt(), "x_plus_0");
  addZero->addOperand(loadPtr);
  addZero->addOperand(new nir::ConstantInt(0));
  nir::Instruction *addPtr = addZero.get();
  entry->addInstruction(std::move(addZero));

  auto ret = std::make_unique<nir::Instruction>(nir::InstKind::Ret,
                                                NType::makeVoid(), "");
  ret->addOperand(addPtr);
  entry->addInstruction(std::move(ret));

  auto optimizer = nir::Optimizer::createDefaultOptimizer();
  optimizer->run(module.get());

  const auto &insts = entry->getInstructions();
  ASSERT_TRUE(!insts.empty());
  ASSERT_EQ(insts.back()->getKind(), nir::InstKind::Ret);
  ASSERT_TRUE(!insts.back()->getOperands().empty());
  auto *retConst = dynamic_cast<nir::ConstantInt *>(insts.back()->getOperand(0));
  ASSERT_TRUE(retConst != nullptr);
  ASSERT_EQ(retConst->getValue(), 42);
  return true;
}

TEST(CopyPropagationPassBecomesStableAfterRewrite) {
  auto module = std::make_unique<nir::Module>("copy_prop_stable");
  nir::Function *fn = module->createFunction("Init", NType::makeInt());
  nir::Block *entry = fn->createBlock("entry");

  auto allocaInst = std::make_unique<nir::Instruction>(
      nir::InstKind::Alloca, NType::makePointer(NType::makeInt()), "x_ptr");
  nir::Instruction *allocaPtr = allocaInst.get();
  entry->addInstruction(std::move(allocaInst));

  auto store = std::make_unique<nir::Instruction>(nir::InstKind::Store,
                                                  NType::makeVoid(), "");
  store->addOperand(new nir::ConstantInt(7));
  store->addOperand(allocaPtr);
  entry->addInstruction(std::move(store));

  auto load = std::make_unique<nir::Instruction>(nir::InstKind::Load,
                                                 NType::makeInt(), "x_val");
  load->addOperand(allocaPtr);
  nir::Instruction *loadPtr = load.get();
  entry->addInstruction(std::move(load));

  auto add = std::make_unique<nir::Instruction>(nir::InstKind::Add,
                                                NType::makeInt(), "x_plus_1");
  add->addOperand(loadPtr);
  add->addOperand(new nir::ConstantInt(1));
  nir::Instruction *addPtr = add.get();
  entry->addInstruction(std::move(add));

  auto ret = std::make_unique<nir::Instruction>(nir::InstKind::Ret,
                                                NType::makeVoid(), "");
  ret->addOperand(addPtr);
  entry->addInstruction(std::move(ret));

  nir::CopyPropagationPass pass;
  ASSERT_TRUE(pass.runOnModule(module.get()));
  ASSERT_FALSE(pass.runOnModule(module.get()));
  return true;
}

TEST(CopyPropagationPassDoesNotReportCrossBlockPseudoChange) {
  auto module = std::make_unique<nir::Module>("copy_prop_cross_block");
  nir::Function *fn = module->createFunction("Init", NType::makeInt());
  nir::Block *entry = fn->createBlock("entry");
  nir::Block *next = fn->createBlock("next");

  auto allocaInst = std::make_unique<nir::Instruction>(
      nir::InstKind::Alloca, NType::makePointer(NType::makeInt()), "x_ptr");
  nir::Instruction *allocaPtr = allocaInst.get();
  entry->addInstruction(std::move(allocaInst));

  auto store = std::make_unique<nir::Instruction>(nir::InstKind::Store,
                                                  NType::makeVoid(), "");
  store->addOperand(new nir::ConstantInt(11));
  store->addOperand(allocaPtr);
  entry->addInstruction(std::move(store));

  auto load = std::make_unique<nir::Instruction>(nir::InstKind::Load,
                                                 NType::makeInt(), "x_val");
  load->addOperand(allocaPtr);
  nir::Instruction *loadPtr = load.get();
  entry->addInstruction(std::move(load));

  auto br =
      std::make_unique<nir::Instruction>(nir::InstKind::Br, NType::makeVoid(), "");
  br->addOperand(new nir::BlockRef(next));
  entry->addInstruction(std::move(br));

  auto ret = std::make_unique<nir::Instruction>(nir::InstKind::Ret,
                                                NType::makeVoid(), "");
  ret->addOperand(loadPtr);
  next->addInstruction(std::move(ret));

  nir::CopyPropagationPass pass;
  ASSERT_FALSE(pass.runOnModule(module.get()));
  return true;
}

TEST(ConstantFoldingPassBecomesStableAfterRewrite) {
  auto module = std::make_unique<nir::Module>("const_fold_stable");
  nir::Function *fn = module->createFunction("Init", NType::makeInt());
  nir::Block *entry = fn->createBlock("entry");

  auto add = std::make_unique<nir::Instruction>(nir::InstKind::Add,
                                                NType::makeInt(), "sum");
  add->addOperand(new nir::ConstantInt(2));
  add->addOperand(new nir::ConstantInt(3));
  nir::Instruction *addPtr = add.get();
  entry->addInstruction(std::move(add));

  auto ret = std::make_unique<nir::Instruction>(nir::InstKind::Ret,
                                                NType::makeVoid(), "");
  ret->addOperand(addPtr);
  entry->addInstruction(std::move(ret));

  nir::ConstantFoldingPass pass;
  ASSERT_TRUE(pass.runOnModule(module.get()));
  ASSERT_FALSE(pass.runOnModule(module.get()));
  return true;
}

TEST(AlgebraicSimplificationPassBecomesStableAfterRewrite) {
  auto module = std::make_unique<nir::Module>("algebraic_stable");
  nir::Function *fn = module->createFunction("Init", NType::makeInt());
  nir::Block *entry = fn->createBlock("entry");
  nir::Argument *x = fn->addArgument(NType::makeInt(), "x");

  auto add = std::make_unique<nir::Instruction>(nir::InstKind::Add,
                                                NType::makeInt(), "x_plus_0");
  add->addOperand(x);
  add->addOperand(new nir::ConstantInt(0));
  nir::Instruction *addPtr = add.get();
  entry->addInstruction(std::move(add));

  auto ret = std::make_unique<nir::Instruction>(nir::InstKind::Ret,
                                                NType::makeVoid(), "");
  ret->addOperand(addPtr);
  entry->addInstruction(std::move(ret));

  nir::AlgebraicSimplificationPass pass;
  ASSERT_TRUE(pass.runOnModule(module.get()));
  ASSERT_FALSE(pass.runOnModule(module.get()));
  return true;
}

TEST(DeadStoreEliminationRemovesShadowedStore) {
  auto module = std::make_unique<nir::Module>("dse");
  nir::Function *fn = module->createFunction("Init", NType::makeVoid());
  nir::Block *entry = fn->createBlock("entry");

  auto allocaInst = std::make_unique<nir::Instruction>(
      nir::InstKind::Alloca, NType::makePointer(NType::makeInt()), "x_ptr");
  nir::Instruction *allocaPtr = allocaInst.get();
  entry->addInstruction(std::move(allocaInst));

  auto store1 = std::make_unique<nir::Instruction>(nir::InstKind::Store,
                                                   NType::makeVoid(), "");
  store1->addOperand(new nir::ConstantInt(1));
  store1->addOperand(allocaPtr);
  entry->addInstruction(std::move(store1));

  auto store2 = std::make_unique<nir::Instruction>(nir::InstKind::Store,
                                                   NType::makeVoid(), "");
  store2->addOperand(new nir::ConstantInt(2));
  store2->addOperand(allocaPtr);
  entry->addInstruction(std::move(store2));

  auto load = std::make_unique<nir::Instruction>(nir::InstKind::Load,
                                                 NType::makeInt(), "x_val");
  load->addOperand(allocaPtr);
  entry->addInstruction(std::move(load));

  auto ret = std::make_unique<nir::Instruction>(nir::InstKind::Ret,
                                                NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  nir::DeadStoreEliminationPass dse;
  ASSERT_TRUE(dse.runOnModule(module.get()));

  int storeCount = 0;
  for (const auto &inst : entry->getInstructions()) {
    if (inst->getKind() == nir::InstKind::Store) {
      storeCount++;
    }
  }
  ASSERT_EQ(storeCount, 1);
  return true;
}

TEST(BranchSimplificationReplacesConstantCondBr) {
  auto module = std::make_unique<nir::Module>("branch_simplify");
  nir::Function *fn = module->createFunction("Init", NType::makeVoid());
  nir::Block *entry = fn->createBlock("entry");
  nir::Block *thenBlock = fn->createBlock("then");
  nir::Block *elseBlock = fn->createBlock("else");

