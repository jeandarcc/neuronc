// NIR exception lowering tests - included from tests/test_main.cpp
#include "neuronc/lexer/Lexer.h"
#include "neuronc/nir/NIRBuilder.h"
#include "neuronc/parser/Parser.h"

#include <string>
#include <unordered_set>
#include <vector>

using namespace neuron;

static std::unique_ptr<nir::Module>
buildNirForExceptionTests(const std::string &source,
                          std::vector<std::string> *outErrors) {
  Lexer lexer(source, "<nir_ex_test>");
  auto tokens = lexer.tokenize();
  outErrors->insert(outErrors->end(), lexer.errors().begin(), lexer.errors().end());

  Parser parser(std::move(tokens), "<nir_ex_test>");
  auto ast = parser.parse();
  outErrors->insert(outErrors->end(), parser.errors().begin(),
                    parser.errors().end());

  nir::NIRBuilder builder;
  return builder.build(ast.get(), "nir_exception_test_module");
}

TEST(NirLowersTryCatchThrowToRuntimeCalls) {
  std::vector<std::string> errors;
  auto module = buildNirForExceptionTests(
      "Init is method() {\n"
      "  try {\n"
      "    throw \"x\";\n"
      "  } catch(err) {\n"
      "    Print(err);\n"
      "  } finally {\n"
      "    Print(\"done\");\n"
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

  std::unordered_set<std::string> calls;
  for (const auto &block : initFn->getBlocks()) {
    for (const auto &inst : block->getInstructions()) {
      if (inst->getKind() != nir::InstKind::Call ||
          inst->getOperands().empty()) {
        continue;
      }
      auto *callee = dynamic_cast<nir::ConstantString *>(inst->getOperand(0));
      if (callee != nullptr) {
        calls.insert(callee->getValue());
      }
    }
  }

  ASSERT_TRUE(calls.count("__neuron_throw") > 0);
  ASSERT_TRUE(calls.count("__neuron_has_exception") > 0);
  ASSERT_TRUE(calls.count("__neuron_clear_exception") > 0);
  return true;
}
