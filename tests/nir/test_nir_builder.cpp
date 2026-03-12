// NIR builder tests - included from tests/test_main.cpp
#include "neuronc/lexer/Lexer.h"
#include "neuronc/nir/NIRBuilder.h"
#include "neuronc/parser/Parser.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace neuron;

static std::unique_ptr<nir::Module> buildNirForTests(
    const std::string &source, std::vector<std::string> *outErrors,
    const std::unordered_map<std::string, NativeModuleInfo> &moduleCppModules = {}) {
  Lexer lexer(source, "<nir_test>");
  auto tokens = lexer.tokenize();
  outErrors->insert(outErrors->end(), lexer.errors().begin(), lexer.errors().end());

  Parser parser(std::move(tokens), "<nir_test>");
  auto ast = parser.parse();
  outErrors->insert(outErrors->end(), parser.errors().begin(),
                    parser.errors().end());

  nir::NIRBuilder builder;
  builder.setModuleCppModules(moduleCppModules);
  auto module = builder.build(ast.get(), "nir_test_module");
  outErrors->insert(outErrors->end(), builder.errors().begin(), builder.errors().end());
  return module;
}

TEST(NirBuildsModuleQualifiedCalls) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init is method() {\n"
      "  System.Print(\"ok\");\n"
      "  value is Math.Sqrt(9);\n"
      "  now is Time.Now();\n"
      "  x is Random.Int(1, 3);\n"
      "  Logger.Info(\"run\");\n"
      "};\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  std::unordered_set<std::string> callNames;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee != nullptr) {
        callNames.insert(callee->getValue());
      }
    }
  }

  ASSERT_TRUE(callNames.count("System.Print") > 0);
  ASSERT_TRUE(callNames.count("Math.Sqrt") > 0);
  ASSERT_TRUE(callNames.count("Time.Now") > 0);
  ASSERT_TRUE(callNames.count("Random.Int") > 0);
  ASSERT_TRUE(callNames.count("Logger.Info") > 0);
  return true;
}

TEST(NirModuleRendersToString) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init is method() {\n"
      "  return;\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const std::string rendered = module->toString();
  ASSERT_TRUE(rendered.find("; ModuleID = 'nir_test_module'") != std::string::npos);
  ASSERT_TRUE(rendered.find("define void @Init()") != std::string::npos);
  ASSERT_TRUE(rendered.find("ret void") != std::string::npos);
  return true;
}

TEST(NirBuildsTypeQualifiedCalls) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init is method() {\n"
      "  t is Tensor<float>.Random(2, 2);\n"
      "};\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  bool foundTensorRandom = false;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee != nullptr && callee->getValue() == "Tensor.Random") {
        foundTensorRandom = true;
      }
    }
  }

  ASSERT_TRUE(foundTensorRandom);
  return true;
}

TEST(NirLowersInputIntFluentChainIntoBuiltinCall) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init is method() {\n"
      "  age is Input<int>(\"Age: \").Min(18).Max(99).Default(21).TimeoutMs(5000);\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  const nir::Instruction *inputCall = nullptr;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee != nullptr && callee->getValue() == "__neuron_input_int") {
        inputCall = inst.get();
        break;
      }
    }
    if (inputCall != nullptr) {
      break;
    }
  }

  ASSERT_TRUE(inputCall != nullptr);
  ASSERT_EQ(inputCall->getOperands().size(), 9u);
  auto *hasMin = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(2));
  auto *minVal = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(3));
  auto *hasMax = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(4));
  auto *maxVal = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(5));
  auto *hasDefault = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(6));
  auto *defaultVal = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(7));
  auto *timeout = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(8));
  ASSERT_TRUE(hasMin != nullptr && minVal != nullptr && hasMax != nullptr &&
              maxVal != nullptr && hasDefault != nullptr &&
              defaultVal != nullptr && timeout != nullptr);
  ASSERT_EQ(hasMin->getValue(), 1);
  ASSERT_EQ(minVal->getValue(), 18);
  ASSERT_EQ(hasMax->getValue(), 1);
  ASSERT_EQ(maxVal->getValue(), 99);
  ASSERT_EQ(hasDefault->getValue(), 1);
  ASSERT_EQ(defaultVal->getValue(), 21);
  ASSERT_EQ(timeout->getValue(), 5000);
  return true;
}

TEST(NirLowersInputStringSecretChainIntoBuiltinCall) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init is method() {\n"
      "  password is Input<string>(\"Password: \").Secret().Default(\"guest\").TimeoutMs(1500);\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  const nir::Instruction *inputCall = nullptr;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee != nullptr && callee->getValue() == "__neuron_input_string") {
        inputCall = inst.get();
        break;
      }
    }
    if (inputCall != nullptr) {
      break;
    }
  }

  ASSERT_TRUE(inputCall != nullptr);
  ASSERT_EQ(inputCall->getOperands().size(), 6u);
  auto *secret = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(2));
  auto *hasDefault = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(3));
  auto *defaultVal =
      dynamic_cast<nir::ConstantString *>(inputCall->getOperand(4));
  auto *timeout = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(5));
  ASSERT_TRUE(secret != nullptr && hasDefault != nullptr &&
              defaultVal != nullptr && timeout != nullptr);
  ASSERT_EQ(secret->getValue(), 1);
  ASSERT_EQ(hasDefault->getValue(), 1);
  ASSERT_EQ(defaultVal->getValue(), "guest");
  ASSERT_EQ(timeout->getValue(), 1500);
  return true;
}

TEST(NirLowersGenericlessInputAsStringBuiltinCall) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init is method() {\n"
      "  value is Input(\"Enter: \").Secret().Default(\"guest\").TimeoutMs(1500);\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  const nir::Instruction *inputCall = nullptr;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee != nullptr && callee->getValue() == "__neuron_input_string") {
        inputCall = inst.get();
        break;
      }
    }
    if (inputCall != nullptr) {
      break;
    }
  }

  ASSERT_TRUE(inputCall != nullptr);
  ASSERT_EQ(inputCall->getOperands().size(), 6u);
  auto *secret = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(2));
  auto *hasDefault = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(3));
  auto *defaultVal =
      dynamic_cast<nir::ConstantString *>(inputCall->getOperand(4));
  auto *timeout = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(5));
  ASSERT_TRUE(secret != nullptr && hasDefault != nullptr &&
              defaultVal != nullptr && timeout != nullptr);
  ASSERT_EQ(secret->getValue(), 1);
  ASSERT_EQ(hasDefault->getValue(), 1);
  ASSERT_EQ(defaultVal->getValue(), "guest");
  ASSERT_EQ(timeout->getValue(), 1500);
  return true;
}

TEST(NirLowersInputEnumChainIntoBuiltinCall) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Color is enum { Red, Green, Blue };\n"
      "Init is method() {\n"
      "  choice is Input<Color>(\"Color: \").Default(Color.Blue).TimeoutMs(1200);\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  const nir::Instruction *inputCall = nullptr;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee != nullptr && callee->getValue() == "__neuron_input_enum") {
        inputCall = inst.get();
        break;
      }
    }
    if (inputCall != nullptr) {
      break;
    }
  }

  ASSERT_TRUE(inputCall != nullptr);
  ASSERT_EQ(inputCall->getOperands().size(), 7u);

  auto *optionsPayload =
      dynamic_cast<nir::ConstantString *>(inputCall->getOperand(2));
  auto *optionCount = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(3));
  auto *hasDefault = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(4));
  auto *defaultValue = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(5));
  auto *timeout = dynamic_cast<nir::ConstantInt *>(inputCall->getOperand(6));
  ASSERT_TRUE(optionsPayload != nullptr && optionCount != nullptr &&
              hasDefault != nullptr && defaultValue != nullptr &&
              timeout != nullptr);
  ASSERT_EQ(optionsPayload->getValue(), "Red\nGreen\nBlue");
  ASSERT_EQ(optionCount->getValue(), 3);
  ASSERT_EQ(hasDefault->getValue(), 1);
  ASSERT_EQ(defaultValue->getValue(), 2);
  ASSERT_EQ(timeout->getValue(), 1200);
  return true;
}

TEST(NirBuildsMultiBindingAndMultiSelectorMatchExpression) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Color enum { Red, Yellow, Green };\n"
      "x, y is 0 as int;\n"
      "Init is method() {\n"
      "  firstColor is Color.Red as Color;\n"
      "  secondColor is Color.Yellow as Color;\n"
      "  result is match(firstColor, secondColor) {\n"
      "    Color.Red, Color.Yellow then Color.Green;\n"
      "    default then Color.Red;\n"
      "  };\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);
  ASSERT_EQ(module->getGlobals().size(), 2u);
  ASSERT_EQ(module->getGlobals()[0]->getName(), "x");
  ASSERT_EQ(module->getGlobals()[1]->getName(), "y");

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  int eqCount = 0;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() == nir::InstKind::Eq) {
        ++eqCount;
      }
    }
  }

  ASSERT_TRUE(eqCount >= 2);
  return true;
}

TEST(NirBuildsThreadCallExpression) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Worker is method() {\n"
      "  Print(\"w\");\n"
      "}\n"
      "Init is method() {\n"
      "  h is thread(Worker);\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  bool foundThreadCall = false;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee != nullptr && callee->getValue() == "thread") {
        foundThreadCall = true;
      }
    }
  }

  ASSERT_TRUE(foundThreadCall);
  return true;
}

TEST(NirLowersFusionChainIntoSequentialCalls) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Normalize is method(value as int) as int {\n"
      "  return value;\n"
      "}\n"
      "Relu is method(value as int) as int {\n"
      "  return value;\n"
      "}\n"
      "Softmax is method(value as int) as int {\n"
      "  return value;\n"
      "}\n"
      "Init is method() {\n"
      "  result is Normalize-Relu-Softmax(1);\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  std::vector<std::string> callNames;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee != nullptr) {
        callNames.push_back(callee->getValue());
      }
    }
  }

  ASSERT_EQ(callNames.size(), 3u);
  ASSERT_EQ(callNames[0], "Normalize");
  ASSERT_EQ(callNames[1], "Relu");
  ASSERT_EQ(callNames[2], "Softmax");
  return true;
}

TEST(NirLowersRegisteredFusionChainIntoBuiltinCall) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init is method(input as Tensor<float>, kernel as Tensor<float>,\n"
      "               bias as Tensor<float>, gamma as Tensor<float>,\n"
      "               beta as Tensor<float>, mean as Tensor<float>,\n"
      "               variance as Tensor<float>) {\n"
      "  result is NN.Conv2D-BatchNorm-ReLU(\n"
      "      input, kernel, bias, gamma, beta, mean, variance,\n"
      "      0.001, 1, 1, 0, 0);\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  const nir::Instruction *fusionCall = nullptr;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee != nullptr &&
          callee->getValue() == "__neuron_fused_conv2d_batchnorm_relu") {
        fusionCall = inst.get();
        break;
      }
    }
    if (fusionCall != nullptr) {
      break;
    }
  }

  ASSERT_TRUE(fusionCall != nullptr);
  ASSERT_EQ(fusionCall->getOperands().size(), 14u);
  auto *hint =
      dynamic_cast<nir::ConstantInt *>(fusionCall->getOperand(13));
  ASSERT_TRUE(hint != nullptr);
  ASSERT_EQ(hint->getValue(), 1);
  return true;
}

TEST(NirPreservesExplicitDynamicStorageType) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init method() {\n"
      "  a \"wow\" as dynamic;\n"
      "  a 10;\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  const nir::Instruction *aAlloca = nullptr;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() == nir::InstKind::Alloca &&
          inst->getName() == "a_ptr") {
        aAlloca = inst.get();
        break;
      }
    }
    if (aAlloca != nullptr) {
      break;
    }
  }

  ASSERT_TRUE(aAlloca != nullptr);
  ASSERT_TRUE(aAlloca->getType() != nullptr);
  ASSERT_EQ(aAlloca->getType()->kind, TypeKind::Pointer);
  ASSERT_TRUE(aAlloca->getType()->pointeeType != nullptr);
  ASSERT_EQ(aAlloca->getType()->pointeeType->kind, TypeKind::Dynamic);
  return true;
}

TEST(NirResolvesQualifiedBuiltinReturnType) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init is method() {\n"
      "  text is Resource.ReadText(\"test.txt\");\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  const nir::Instruction *builtinCall = nullptr;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee != nullptr && callee->getValue() == "Resource.ReadText") {
        builtinCall = inst.get();
        break;
      }
    }
    if (builtinCall != nullptr) {
      break;
    }
  }

  ASSERT_TRUE(builtinCall != nullptr);
  ASSERT_TRUE(builtinCall->getType() != nullptr);
  ASSERT_EQ(builtinCall->getType()->kind, TypeKind::String);
  return true;
}

TEST(NirLowersMatchIntoBranches) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Color enum { Red, Yellow, Green };\n"
      "Init method() {\n"
      "  color is Color.Red as Color;\n"
      "  match (color) {\n"
      "    Color.Red then {\n"
      "      Print(\"Red\");\n"
      "    }\n"
      "    default then {\n"
      "      Print(\"Other\");\n"
      "    }\n"
      "  }\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  bool foundEq = false;
  bool foundCondBr = false;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() == nir::InstKind::Eq) {
        foundEq = true;
      }
      if (inst->getKind() == nir::InstKind::CondBr) {
        foundCondBr = true;
      }
    }
  }

  ASSERT_TRUE(foundEq);
  ASSERT_TRUE(foundCondBr);
  return true;
}

TEST(NirEmitsCastInstructionForCastPipeline) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init method() {\n"
      "  value is 10 as int;\n"
      "  value as maybe string then float;\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  const nir::Instruction *castInst = nullptr;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() == nir::InstKind::Cast) {
        castInst = inst.get();
        break;
      }
    }
    if (castInst != nullptr) {
      break;
    }
  }

  ASSERT_TRUE(castInst != nullptr);
  ASSERT_TRUE(castInst->getType() != nullptr);
  ASSERT_EQ(castInst->getType()->kind, TypeKind::Nullable);
  ASSERT_TRUE(castInst->getType()->nullableBase() != nullptr);
  ASSERT_EQ(castInst->getType()->nullableBase()->kind, TypeKind::Float);
  return true;
}

TEST(NirLowersStatementsInsideGpuBlock) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init method() {\n"
      "  gpu {\n"
      "    value is 1 as int;\n"
      "  }\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  bool foundGpuInnerAlloca = false;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() == nir::InstKind::Alloca &&
          inst->getName() == "value_ptr") {
        foundGpuInnerAlloca = true;
        break;
      }
    }
    if (foundGpuInnerAlloca) {
      break;
    }
  }

  ASSERT_TRUE(foundGpuInnerAlloca);
  return true;
}

TEST(NirMarksTensorOpsInsideGpuBlockWithGpuPreferHint) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init method() {\n"
      "  a is Tensor<float>.Random(2, 2);\n"
      "  b is Tensor<float>.Random(2, 2);\n"
      "  cpu_sum is a + b;\n"
      "  gpu {\n"
      "    gpu_sum is a + b;\n"
      "  }\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  bool sawAutoTensorAdd = false;
  bool sawGpuPreferTensorAdd = false;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::TensorAdd) {
        continue;
      }
      if (inst->getExecutionHint() == nir::ExecutionHint::Auto) {
        sawAutoTensorAdd = true;
      }
      if (inst->getExecutionHint() == nir::ExecutionHint::GpuPrefer) {
        sawGpuPreferTensorAdd = true;
      }
    }
  }

  ASSERT_TRUE(sawAutoTensorAdd);
  ASSERT_TRUE(sawGpuPreferTensorAdd);
  return true;
}

TEST(NirEmitsGpuScopeMarkersForGpuBlock) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init method() {\n"
      "  gpu {\n"
      "    value is 1 as int;\n"
      "  }\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  bool sawScopeBegin = false;
  bool sawScopeEnd = false;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() == nir::InstKind::GpuScopeBegin) {
        sawScopeBegin = true;
      } else if (inst->getKind() == nir::InstKind::GpuScopeEnd) {
        sawScopeEnd = true;
      }
    }
  }

  ASSERT_TRUE(sawScopeBegin);
  ASSERT_TRUE(sawScopeEnd);
  return true;
}

TEST(NirUnwindsGpuScopeBeforeReturn) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init method() {\n"
      "  gpu {\n"
      "    return;\n"
      "  }\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  std::vector<nir::InstKind> kinds;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      kinds.push_back(inst->getKind());
    }
  }

  int beginIndex = -1;
  int endIndex = -1;
  int retIndex = -1;
  for (size_t i = 0; i < kinds.size(); ++i) {
    if (kinds[i] == nir::InstKind::GpuScopeBegin && beginIndex < 0) {
      beginIndex = static_cast<int>(i);
    }
    if (kinds[i] == nir::InstKind::GpuScopeEnd && endIndex < 0) {
      endIndex = static_cast<int>(i);
    }
    if (kinds[i] == nir::InstKind::Ret && retIndex < 0) {
      retIndex = static_cast<int>(i);
    }
  }

  ASSERT_TRUE(beginIndex >= 0);
  ASSERT_TRUE(endIndex >= 0);
  ASSERT_TRUE(retIndex >= 0);
  ASSERT_TRUE(beginIndex < endIndex);
  ASSERT_TRUE(endIndex < retIndex);
  return true;
}

TEST(NirCanvasLowersLifecycleInDeterministicOrder) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "OpenHandler method() { }\n"
      "FrameHandler method() { }\n"
      "ResizeHandler method() { }\n"
      "CloseHandler method() { }\n"
      "Init method() {\n"
      "  window is 0 as dynamic;\n"
      "  canvas(window, onOpen: OpenHandler, onFrame: FrameHandler, onResize: ResizeHandler, onClose: CloseHandler) {\n"
      "  }\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  std::vector<std::string> callNames;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee != nullptr) {
        callNames.push_back(callee->getValue());
      }
    }
  }

  std::unordered_map<std::string, size_t> firstCallIndex;
  for (size_t i = 0; i < callNames.size(); ++i) {
    if (firstCallIndex.find(callNames[i]) == firstCallIndex.end()) {
      firstCallIndex.emplace(callNames[i], i);
    }
  }

  ASSERT_TRUE(firstCallIndex.count("Graphics.CreateCanvas") > 0);
  ASSERT_TRUE(firstCallIndex.count("OpenHandler") > 0);
  ASSERT_TRUE(firstCallIndex.count("__neuron_graphics_canvas_pump") > 0);
  ASSERT_TRUE(firstCallIndex.count("__neuron_graphics_canvas_should_close") > 0);
  ASSERT_TRUE(firstCallIndex.count("__neuron_graphics_canvas_take_resize") > 0);
  ASSERT_TRUE(firstCallIndex.count("__neuron_graphics_canvas_begin_frame") > 0);
  ASSERT_TRUE(firstCallIndex.count("FrameHandler") > 0);
  ASSERT_TRUE(firstCallIndex.count("Present") > 0);
  ASSERT_TRUE(firstCallIndex.count("__neuron_graphics_canvas_end_frame") > 0);
  ASSERT_TRUE(firstCallIndex.count("CloseHandler") > 0);
  ASSERT_TRUE(firstCallIndex.count("__neuron_graphics_canvas_free") > 0);

  ASSERT_TRUE(firstCallIndex["OpenHandler"] <
              firstCallIndex["__neuron_graphics_canvas_pump"]);
  ASSERT_TRUE(firstCallIndex["__neuron_graphics_canvas_begin_frame"] <
              firstCallIndex["FrameHandler"]);
  ASSERT_TRUE(firstCallIndex["FrameHandler"] < firstCallIndex["Present"]);
  ASSERT_TRUE(firstCallIndex["Present"] <
              firstCallIndex["__neuron_graphics_canvas_end_frame"]);
  ASSERT_TRUE(firstCallIndex["CloseHandler"] <
              firstCallIndex["__neuron_graphics_canvas_free"]);

  int condBrCount = 0;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() == nir::InstKind::CondBr) {
        condBrCount++;
      }
    }
  }
  ASSERT_TRUE(condBrCount >= 2);
  return true;
}

TEST(NirCanvasPrefersInlineHandlerOverExternalBinding) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "FrameHandler method() { }\n"
      "InlineTick method() { }\n"
      "Init method() {\n"
      "  window is 0 as dynamic;\n"
      "  canvas(window, onFrame: FrameHandler) {\n"
      "    OnFrame {\n"
      "      InlineTick();\n"
      "    }\n"
      "  }\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  bool sawInlineCall = false;
  bool sawExternalCall = false;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee == nullptr) {
        continue;
      }
      if (callee->getValue() == "InlineTick") {
        sawInlineCall = true;
      }
      if (callee->getValue() == "FrameHandler") {
        sawExternalCall = true;
      }
    }
  }

  ASSERT_TRUE(sawInlineCall);
  ASSERT_FALSE(sawExternalCall);
  return true;
}

TEST(NirLowersGraphicsV2TypedFrameCommands) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init method() {\n"
      "  mainWindow is Window.Create(1280, 720, \"App\");\n"
      "  mesh is Mesh.Load(\"examples/assets/triangle.obj\");\n"
      "  material is Material<BasicLit>();\n"
      "  canvas(mainWindow) {\n"
      "    OnFrame {\n"
      "      cmd.Clear(Color(0.08, 0.08, 0.10, 1.0));\n"
      "      cmd.Draw(mesh, material);\n"
      "    }\n"
      "  }\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  std::unordered_set<std::string> callNames;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee != nullptr) {
        callNames.insert(callee->getValue());
      }
    }
  }

  ASSERT_TRUE(callNames.count("Window.Create") > 0);
  ASSERT_TRUE(callNames.count("Mesh.Load") > 0);
  ASSERT_TRUE(callNames.count("Material.Create") > 0);
  ASSERT_TRUE(callNames.count("Color") > 0);
  ASSERT_TRUE(callNames.count("cmd.Clear") > 0);
  ASSERT_TRUE(callNames.count("cmd.Draw") > 0);
  ASSERT_TRUE(module->getShaders().empty());
  return true;
}

TEST(NirLowersDescriptorFieldAssignmentsToMaterialSetCalls) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Textured is shader {\n"
      "  tint as Color;\n"
      "  albedo as Texture2D;\n"
      "  linearSampler as Sampler;\n"
      "  Vertex method(position as Vector3, uv as Vector2) {\n"
      "    pass uv;\n"
      "    return position;\n"
      "  }\n"
      "  Fragment method(uv as Vector2) {\n"
      "    return tint;\n"
      "  }\n"
      "}\n"
      "Init method() {\n"
      "  tex is Texture2D.Load(\"examples/assets/checker.png\");\n"
      "  samp is Sampler.Create();\n"
      "  mat is Material<Textured>();\n"
      "  mat.tint is Color(1.0, 1.0, 1.0, 1.0);\n"
      "  mat.albedo is tex;\n"
      "  mat.linearSampler is samp;\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  std::unordered_set<std::string> callNames;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee != nullptr) {
        callNames.insert(callee->getValue());
      }
    }
  }

  ASSERT_TRUE(callNames.count("Material.Create") > 0);
  ASSERT_TRUE(callNames.count("Material.SetVec4") > 0);
  ASSERT_TRUE(callNames.count("Material.SetTexture") > 0);
  ASSERT_TRUE(callNames.count("Material.SetSampler") > 0);
  return true;
}

TEST(NirLowersScene2DCallsToTypedRuntimeSurface) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init method() {\n"
      "  window is Window.Create(1280, 720, \"Scene2D\");\n"
      "  scene is Scene.Create();\n"
      "  entity is scene.CreateEntity(\"Sprite\");\n"
      "  cameraEntity is scene.CreateEntity(\"Camera\");\n"
      "  camera is cameraEntity.AddCamera2D();\n"
      "  transform is entity.GetTransform();\n"
      "  transform.SetPosition(Vector3(0.0, 0.0, 0.0));\n"
      "  sprite is entity.AddSpriteRenderer2D();\n"
      "  sprite.SetSize(Vector2(2.0, 2.0));\n"
      "  renderer is Renderer2D.Create();\n"
      "  renderer.SetCamera(camera);\n"
      "  renderer.SetClearColor(Color(0.1, 0.1, 0.1, 1.0));\n"
      "  canvas(window) {\n"
      "    OnFrame {\n"
      "      renderer.Render(scene);\n"
      "    }\n"
      "  }\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  std::unordered_set<std::string> callNames;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee != nullptr) {
        callNames.insert(callee->getValue());
      }
    }
  }

  ASSERT_TRUE(callNames.count("Scene.Create") > 0);
  ASSERT_TRUE(callNames.count("scene.CreateEntity") > 0);
  ASSERT_TRUE(callNames.count("cameraEntity.AddCamera2D") > 0);
  ASSERT_TRUE(callNames.count("entity.GetTransform") > 0);
  ASSERT_TRUE(callNames.count("transform.SetPosition") > 0);
  ASSERT_TRUE(callNames.count("entity.AddSpriteRenderer2D") > 0);
  ASSERT_TRUE(callNames.count("sprite.SetSize") > 0);
  ASSERT_TRUE(callNames.count("Renderer2D.Create") > 0);
  ASSERT_TRUE(callNames.count("renderer.SetCamera") > 0);
  ASSERT_TRUE(callNames.count("renderer.SetClearColor") > 0);
  ASSERT_TRUE(callNames.count("renderer.Render") > 0);
  return true;
}

TEST(NirBuildsNullLiteralBinding) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init is method() {\n"
      "  value is null;\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  bool sawNullValue = false;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      for (auto *operand : inst->getOperands()) {
        if (operand != nullptr &&
            operand->getValueKind() == nir::ValueKind::ConstantNull) {
          sawNullValue = true;
        }
      }
    }
  }

  ASSERT_TRUE(sawNullValue);
  return true;
}

TEST(NirBuilderReportsUndefinedSymbolErrors) {
  Lexer lexer("Init is method() {\n  x is missing;\n}\n", "<nir_test>");
  auto tokens = lexer.tokenize();
  ASSERT_TRUE(lexer.errors().empty());

  Parser parser(std::move(tokens), "<nir_test>");
  auto ast = parser.parse();
  ASSERT_TRUE(parser.errors().empty());

  nir::NIRBuilder builder;
  auto module = builder.build(ast.get(), "nir_test_module");
  ASSERT_TRUE(module != nullptr);
  ASSERT_TRUE(builder.hasErrors());
  ASSERT_FALSE(builder.errors().empty());
  ASSERT_TRUE(builder.errors().front().find("Undefined symbol: missing") !=
              std::string::npos);
  return true;
}

TEST(NirLowersCaretPowAndRootInstructions) {
  std::vector<std::string> errors;
  auto module = buildNirForTests(
      "Init is method() {\n"
      "  p is 2 ^ 3;\n"
      "  r is 9 ^^ 3;\n"
      "  s is 16 ^^ 2;\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const nir::Function *initFn = nullptr;
  for (const auto &fn : module->getFunctions()) {
    if (fn->getName() == "Init") {
      initFn = fn.get();
      break;
    }
  }
  ASSERT_TRUE(initFn != nullptr);

  bool foundPow = false;
  bool foundNthRoot = false;
  bool foundSqrt = false;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() == nir::InstKind::Pow) {
        foundPow = true;
      }
      if (inst->getKind() == nir::InstKind::NthRoot) {
        foundNthRoot = true;
      }
      if (inst->getKind() == nir::InstKind::Sqrt) {
        foundSqrt = true;
      }
    }
  }

  ASSERT_TRUE(foundPow);
  ASSERT_TRUE(foundNthRoot);
  ASSERT_TRUE(foundSqrt);
  return true;
}
