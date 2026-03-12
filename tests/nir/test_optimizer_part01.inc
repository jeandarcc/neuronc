// NIR optimizer tests - included from tests/test_main.cpp
#include "neuronc/nir/Optimizer.h"

using namespace neuron;

TEST(TensorFusionPassFusesMulAdd) {
  auto module = std::make_unique<nir::Module>("fusion_test");
  auto tensorTy = NType::makeTensor(NType::makeFloat());

  nir::Function *fn = module->createFunction("Init", NType::makeVoid());
  nir::Argument *a = fn->addArgument(tensorTy, "a");
  nir::Argument *b = fn->addArgument(tensorTy, "b");
  nir::Argument *c = fn->addArgument(tensorTy, "c");
  nir::Block *entry = fn->createBlock("entry");

  auto mul = std::make_unique<nir::Instruction>(nir::InstKind::TensorMul, tensorTy,
                                                 "mul_ab");
  mul->addOperand(a);
  mul->addOperand(b);
  nir::Instruction *mulPtr = mul.get();
  entry->addInstruction(std::move(mul));

  auto add = std::make_unique<nir::Instruction>(nir::InstKind::TensorAdd, tensorTy,
                                                 "add_mul_c");
  add->addOperand(mulPtr);
  add->addOperand(c);
  entry->addInstruction(std::move(add));

  nir::TensorFusionPass pass;
  ASSERT_TRUE(pass.runOnModule(module.get()));

  const auto &insts = entry->getInstructions();
  ASSERT_EQ(insts.size(), 1u);
  ASSERT_EQ(insts[0]->getKind(), nir::InstKind::TensorFMA);
  ASSERT_EQ(insts[0]->getOperands().size(), 3u);
  ASSERT_EQ(insts[0]->getOperand(0), a);
  ASSERT_EQ(insts[0]->getOperand(1), b);
  ASSERT_EQ(insts[0]->getOperand(2), c);
  return true;
}

TEST(TensorFusionPassPreservesGpuPreferHint) {
  auto module = std::make_unique<nir::Module>("fusion_hint_preserve");
  auto tensorTy = NType::makeTensor(NType::makeFloat());

  nir::Function *fn = module->createFunction("Init", NType::makeVoid());
  nir::Argument *a = fn->addArgument(tensorTy, "a");
  nir::Argument *b = fn->addArgument(tensorTy, "b");
  nir::Argument *c = fn->addArgument(tensorTy, "c");
  nir::Block *entry = fn->createBlock("entry");

  auto mul =
      std::make_unique<nir::Instruction>(nir::InstKind::TensorMul, tensorTy, "mul_ab");
  mul->setExecutionHint(nir::ExecutionHint::GpuPrefer);
  mul->addOperand(a);
  mul->addOperand(b);
  nir::Instruction *mulPtr = mul.get();
  entry->addInstruction(std::move(mul));

  auto add = std::make_unique<nir::Instruction>(nir::InstKind::TensorAdd, tensorTy,
                                                "add_mul_c");
  add->setExecutionHint(nir::ExecutionHint::GpuPrefer);
  add->addOperand(mulPtr);
  add->addOperand(c);
  entry->addInstruction(std::move(add));

  nir::TensorFusionPass pass;
  ASSERT_TRUE(pass.runOnModule(module.get()));

  const auto &insts = entry->getInstructions();
  ASSERT_EQ(insts.size(), 1u);
  ASSERT_EQ(insts[0]->getKind(), nir::InstKind::TensorFMA);
  ASSERT_EQ(insts[0]->getExecutionHint(), nir::ExecutionHint::GpuPrefer);
  return true;
}

TEST(TensorFusionPassSkipsDifferentExecutionHints) {
  auto module = std::make_unique<nir::Module>("fusion_hint_mismatch");
  auto tensorTy = NType::makeTensor(NType::makeFloat());

  nir::Function *fn = module->createFunction("Init", NType::makeVoid());
  nir::Argument *a = fn->addArgument(tensorTy, "a");
  nir::Argument *b = fn->addArgument(tensorTy, "b");
  nir::Argument *c = fn->addArgument(tensorTy, "c");
  nir::Block *entry = fn->createBlock("entry");

  auto mul =
      std::make_unique<nir::Instruction>(nir::InstKind::TensorMul, tensorTy, "mul_ab");
  mul->setExecutionHint(nir::ExecutionHint::Auto);
  mul->addOperand(a);
  mul->addOperand(b);
  nir::Instruction *mulPtr = mul.get();
  entry->addInstruction(std::move(mul));

  auto add = std::make_unique<nir::Instruction>(nir::InstKind::TensorAdd, tensorTy,
                                                "add_mul_c");
  add->setExecutionHint(nir::ExecutionHint::GpuPrefer);
  add->addOperand(mulPtr);
  add->addOperand(c);
  entry->addInstruction(std::move(add));

  nir::TensorFusionPass pass;
  ASSERT_FALSE(pass.runOnModule(module.get()));

  const auto &insts = entry->getInstructions();
  ASSERT_EQ(insts.size(), 2u);
  ASSERT_EQ(insts[0]->getKind(), nir::InstKind::TensorMul);
  ASSERT_EQ(insts[1]->getKind(), nir::InstKind::TensorAdd);
  return true;
}

TEST(TensorFusionPassSkipsNonFusionPattern) {
  auto module = std::make_unique<nir::Module>("fusion_test_noop");
  auto tensorTy = NType::makeTensor(NType::makeFloat());

  nir::Function *fn = module->createFunction("Init", NType::makeVoid());
  nir::Argument *a = fn->addArgument(tensorTy, "a");
  nir::Argument *b = fn->addArgument(tensorTy, "b");
  nir::Argument *c = fn->addArgument(tensorTy, "c");
  nir::Block *entry = fn->createBlock("entry");

  auto add1 = std::make_unique<nir::Instruction>(nir::InstKind::TensorAdd, tensorTy,
                                                  "add_ab");
  add1->addOperand(a);
  add1->addOperand(b);
  entry->addInstruction(std::move(add1));

  auto add2 = std::make_unique<nir::Instruction>(nir::InstKind::TensorAdd, tensorTy,
                                                  "add_prev_c");
  add2->addOperand(a);
  add2->addOperand(c);
  entry->addInstruction(std::move(add2));

  nir::TensorFusionPass pass;
  ASSERT_FALSE(pass.runOnModule(module.get()));

  const auto &insts = entry->getInstructions();
  ASSERT_EQ(insts.size(), 2u);
  ASSERT_EQ(insts[0]->getKind(), nir::InstKind::TensorAdd);
  ASSERT_EQ(insts[1]->getKind(), nir::InstKind::TensorAdd);
  return true;
}

TEST(TensorFusionPassFusesMatMulBias) {
  auto module = std::make_unique<nir::Module>("fusion_matmul_bias");
  auto tensorTy = NType::makeTensor(NType::makeFloat());

  nir::Function *fn = module->createFunction("Init", NType::makeVoid());
  nir::Argument *a = fn->addArgument(tensorTy, "a");
  nir::Argument *b = fn->addArgument(tensorTy, "b");
  nir::Argument *bias = fn->addArgument(tensorTy, "bias");
  nir::Block *entry = fn->createBlock("entry");

  auto mm = std::make_unique<nir::Instruction>(nir::InstKind::TensorMatMul,
                                               tensorTy, "mm");
  mm->addOperand(a);
  mm->addOperand(b);
  nir::Instruction *mmPtr = mm.get();
  entry->addInstruction(std::move(mm));

  auto add = std::make_unique<nir::Instruction>(nir::InstKind::TensorAdd, tensorTy,
                                                "mm_plus_bias");
  add->addOperand(mmPtr);
  add->addOperand(bias);
  entry->addInstruction(std::move(add));

  nir::TensorFusionPass pass;
  ASSERT_TRUE(pass.runOnModule(module.get()));

  const auto &insts = entry->getInstructions();
  ASSERT_EQ(insts.size(), 1u);
  ASSERT_EQ(insts[0]->getKind(), nir::InstKind::TensorMatMulAdd);
  ASSERT_EQ(insts[0]->getOperands().size(), 3u);
  return true;
}

TEST(TensorFusionPassFusesLinearResidual) {
  auto module = std::make_unique<nir::Module>("fusion_linear_residual");
  auto tensorTy = NType::makeTensor(NType::makeFloat());

  nir::Function *fn = module->createFunction("Init", NType::makeVoid());
  nir::Argument *a = fn->addArgument(tensorTy, "a");
  nir::Argument *b = fn->addArgument(tensorTy, "b");
  nir::Argument *bias = fn->addArgument(tensorTy, "bias");
  nir::Argument *res = fn->addArgument(tensorTy, "res");
  nir::Block *entry = fn->createBlock("entry");

  auto mm = std::make_unique<nir::Instruction>(nir::InstKind::TensorMatMul,
                                               tensorTy, "mm");
  mm->addOperand(a);
  mm->addOperand(b);
  nir::Instruction *mmPtr = mm.get();
  entry->addInstruction(std::move(mm));

  auto addBias = std::make_unique<nir::Instruction>(
      nir::InstKind::TensorAdd, tensorTy, "mm_plus_bias");
  addBias->addOperand(mmPtr);
  addBias->addOperand(bias);
  nir::Instruction *addBiasPtr = addBias.get();
  entry->addInstruction(std::move(addBias));

  auto addRes = std::make_unique<nir::Instruction>(nir::InstKind::TensorAdd, tensorTy,
                                                   "linear_plus_res");
  addRes->addOperand(addBiasPtr);
  addRes->addOperand(res);
  entry->addInstruction(std::move(addRes));

  nir::TensorFusionPass pass;
  ASSERT_TRUE(pass.runOnModule(module.get()));

  const auto &insts = entry->getInstructions();
  ASSERT_EQ(insts.size(), 1u);
  ASSERT_EQ(insts[0]->getKind(), nir::InstKind::TensorLinearFused);
  ASSERT_EQ(insts[0]->getOperands().size(), 4u);
  return true;
}

TEST(OptimizerPropagatesConstantsToReturn) {
  auto module = std::make_unique<nir::Module>("const_prop");
  nir::Function *fn = module->createFunction("Compute", NType::makeInt());
