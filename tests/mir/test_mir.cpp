// MIR tests - included from tests/test_main.cpp
#include "neuronc/lexer/Lexer.h"
#include "neuronc/mir/MIRBuilder.h"
#include "neuronc/mir/MIRPrinter.h"
#include "neuronc/parser/Parser.h"

#include <string>
#include <vector>

using namespace neuron;

namespace {

std::unique_ptr<mir::Module> buildMirForTests(const std::string &source,
                                              std::vector<std::string> *outErrors) {
  Lexer lexer(source, "<mir_test>");
  auto tokens = lexer.tokenize();
  outErrors->insert(outErrors->end(), lexer.errors().begin(), lexer.errors().end());

  Parser parser(std::move(tokens), "<mir_test>");
  auto ast = parser.parse();
  outErrors->insert(outErrors->end(), parser.errors().begin(), parser.errors().end());

  mir::MIRBuilder builder;
  auto module = builder.build(ast.get(), "mir_test_module");
  outErrors->insert(outErrors->end(), builder.errors().begin(), builder.errors().end());
  return module;
}

const mir::Function *findFunction(const mir::Module &module, const std::string &name) {
  for (const auto &function : module.functions) {
    if (function.name == name) {
      return &function;
    }
  }
  return nullptr;
}

bool hasBlockPrefix(const mir::Function &function, const std::string &prefix) {
  for (const auto &block : function.blocks) {
    if (block.name.rfind(prefix, 0) == 0) {
      return true;
    }
  }
  return false;
}

} // namespace

TEST(MirBuildsExplicitControlFlowBlocks) {
  std::vector<std::string> errors;
  auto module = buildMirForTests(
      "Color enum { Red, Yellow };\n"
      "Init method(items as Array<int>) {\n"
      "  flag is true as bool;\n"
      "  total is 0 as int;\n"
      "  color is Color.Red as Color;\n"
      "  if(flag) Print(\"then\"); else Print(\"else\");\n"
      "  while(total < 3) total++;\n"
      "  for(item in items) Print(item);\n"
      "  match(color) {\n"
      "    Color.Red then Print(\"red\");\n"
      "    default then Print(\"other\");\n"
      "  }\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const mir::Function *initFn = findFunction(*module, "Init");
  ASSERT_TRUE(initFn != nullptr);
  ASSERT_TRUE(hasBlockPrefix(*initFn, "if_then_"));
  ASSERT_TRUE(hasBlockPrefix(*initFn, "if_else_"));
  ASSERT_TRUE(hasBlockPrefix(*initFn, "while_cond_"));
  ASSERT_TRUE(hasBlockPrefix(*initFn, "while_body_"));
  ASSERT_TRUE(hasBlockPrefix(*initFn, "for_in_cond_"));
  ASSERT_TRUE(hasBlockPrefix(*initFn, "for_in_body_"));
  ASSERT_TRUE(hasBlockPrefix(*initFn, "match_arm_"));
  ASSERT_TRUE(hasBlockPrefix(*initFn, "match_exit_"));

  bool sawTwoWayBranch = false;
  bool sawIteratorNext = false;
  for (const auto &block : initFn->blocks) {
    if (block.successors.size() == 2) {
      sawTwoWayBranch = true;
    }
    for (const auto &inst : block.instructions) {
      if (inst.kind == mir::InstKind::Call &&
          inst.callee.kind == mir::OperandKind::Label &&
          inst.callee.text == "iter_next") {
        sawIteratorNext = true;
      }
    }
  }

  ASSERT_TRUE(sawTwoWayBranch);
  ASSERT_TRUE(sawIteratorNext);
  return true;
}

TEST(MirLowersExpressionsIntoExplicitTemps) {
  std::vector<std::string> errors;
  auto module = buildMirForTests(
      "Init method() {\n"
      "  value is (1 + 2) * 3;\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const mir::Function *initFn = findFunction(*module, "Init");
  ASSERT_TRUE(initFn != nullptr);

  int constantCount = 0;
  int binaryCount = 0;
  bool sawValueBind = false;
  for (const auto &block : initFn->blocks) {
    for (const auto &inst : block.instructions) {
      if (inst.kind == mir::InstKind::Constant) {
        constantCount++;
        ASSERT_TRUE(inst.result.rfind("%t", 0) == 0);
      }
      if (inst.kind == mir::InstKind::Binary) {
        binaryCount++;
        ASSERT_TRUE(inst.result.rfind("%t", 0) == 0);
        ASSERT_EQ(inst.operands.size(), 2u);
        ASSERT_EQ(inst.operands[0].kind, mir::OperandKind::Temp);
        ASSERT_EQ(inst.operands[1].kind, mir::OperandKind::Temp);
      }
      if (inst.kind == mir::InstKind::Bind && inst.result == "value") {
        sawValueBind = true;
        ASSERT_EQ(inst.operands.size(), 1u);
        ASSERT_EQ(inst.operands[0].kind, mir::OperandKind::Temp);
      }
    }
  }

  ASSERT_EQ(constantCount, 3);
  ASSERT_EQ(binaryCount, 2);
  ASSERT_TRUE(sawValueBind);
  return true;
}

TEST(MirPrinterRendersReadableText) {
  std::vector<std::string> errors;
  auto module = buildMirForTests(
      "Init method() {\n"
      "  return;\n"
      "}\n",
      &errors);

  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(module != nullptr);

  const std::string rendered = mir::printToString(*module);
  ASSERT_TRUE(rendered.find("module mir_test_module") != std::string::npos);
  ASSERT_TRUE(rendered.find("func Init(") != std::string::npos);
  ASSERT_TRUE(rendered.find("bb entry") != std::string::npos);
  ASSERT_TRUE(rendered.find("return") != std::string::npos);
  return true;
}
