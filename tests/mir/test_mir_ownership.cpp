// MIR ownership tests - included from tests/test_main.cpp
#include "neuronc/lexer/Lexer.h"
#include "neuronc/mir/MIRBuilder.h"
#include "neuronc/mir/MIROwnershipPass.h"
#include "neuronc/parser/Parser.h"

#include <string>
#include <vector>

using namespace neuron;

namespace {

struct OwnershipAnalysisResult {
  std::unique_ptr<mir::Module> module;
  std::vector<std::string> buildErrors;
  std::vector<SemanticError> ownershipErrors;
  std::vector<mir::OwnershipHint> hints;
};

OwnershipAnalysisResult analyzeOwnershipForTests(const std::string &source) {
  OwnershipAnalysisResult result;

  Lexer lexer(source, "<mir_ownership_test>");
  auto tokens = lexer.tokenize();
  result.buildErrors.insert(result.buildErrors.end(), lexer.errors().begin(),
                            lexer.errors().end());

  Parser parser(std::move(tokens), "<mir_ownership_test>");
  auto ast = parser.parse();
  result.buildErrors.insert(result.buildErrors.end(), parser.errors().begin(),
                            parser.errors().end());

  mir::MIRBuilder builder;
  result.module = builder.build(ast.get(), "mir_ownership_test");
  result.buildErrors.insert(result.buildErrors.end(), builder.errors().begin(),
                            builder.errors().end());
  if (result.module == nullptr) {
    return result;
  }

  mir::MIROwnershipPass pass;
  pass.setSourceText(source);
  pass.run(*result.module);
  result.ownershipErrors = pass.errors();
  result.hints = pass.hints();
  return result;
}

bool hasOwnershipMessage(const std::vector<SemanticError> &errors,
                         const std::string &needle) {
  for (const auto &error : errors) {
    if (error.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

mir::Module makeLifetimeEscapeModule() {
  mir::Module module;
  module.name = "mir_ownership_manual";

  mir::Function function;
  function.name = "Init";
  function.locals.push_back(
      {"outerRef", "outerRef", {2, 3, "<mir_ownership_manual>"}, 0, false});
  function.locals.push_back(
      {"inner", "inner", {4, 5, "<mir_ownership_manual>"}, 1, false});

  mir::BasicBlock entry{"entry"};
  entry.instructions.push_back(
      {mir::InstKind::Constant, {2, 3, "<mir_ownership_manual>"}, "%t0", "", {},
       {mir::Operand::literal("0")}});
  entry.instructions.push_back(
      {mir::InstKind::Bind, {2, 3, "<mir_ownership_manual>"}, "outerRef", "",
       {}, {mir::Operand::temp("%t0")}, {}, "binding:value"});
  entry.instructions.push_back(
      {mir::InstKind::Constant, {4, 5, "<mir_ownership_manual>"}, "%t1", "", {},
       {mir::Operand::literal("1")}});
  entry.instructions.push_back(
      {mir::InstKind::Bind, {4, 5, "<mir_ownership_manual>"}, "inner", "", {},
       {mir::Operand::temp("%t1")}, {}, "binding:value"});
  entry.instructions.push_back(
      {mir::InstKind::Borrow, {5, 7, "<mir_ownership_manual>"}, "%t2", "", {},
       {mir::Operand::variable("inner")}});
  entry.instructions.push_back(
      {mir::InstKind::Assign, {5, 3, "<mir_ownership_manual>"}, "outerRef", "",
       {}, {mir::Operand::temp("%t2")}, {}, "binding:address_of"});
  entry.instructions.push_back(
      {mir::InstKind::Return, {6, 1, "<mir_ownership_manual>"}, "", "", {}, {}});
  entry.terminated = true;

  function.blocks.push_back(std::move(entry));
  module.functions.push_back(std::move(function));
  return module;
}

} // namespace

TEST(MirOwnershipDetectsUseAfterMove) {
  const auto result = analyzeOwnershipForTests(
      "Init method() {\n"
      "  value is 1;\n"
      "  moved is move value;\n"
      "  Print(value);\n"
      "}\n");

  ASSERT_TRUE(result.buildErrors.empty());
  ASSERT_TRUE(result.module != nullptr);
  ASSERT_TRUE(
      hasOwnershipMessage(result.ownershipErrors, "used after ownership was moved"));
  return true;
}

TEST(MirOwnershipRejectsSecondOwnerWithoutMove) {
  const auto result = analyzeOwnershipForTests(
      "Init method() {\n"
      "  value is 1;\n"
      "  alias is another value;\n"
      "}\n");

  ASSERT_TRUE(result.buildErrors.empty());
  ASSERT_TRUE(result.module != nullptr);
  ASSERT_TRUE(hasOwnershipMessage(result.ownershipErrors, "second owner"));
  return true;
}

TEST(MirOwnershipDetectsReferenceLifetimeEscape) {
  mir::MIROwnershipPass pass;
  mir::Module module = makeLifetimeEscapeModule();
  pass.run(module);

  ASSERT_TRUE(hasOwnershipMessage(pass.errors(), "may outlive owner"));
  return true;
}

TEST(MirOwnershipPassProducesHints) {
  const auto result = analyzeOwnershipForTests(
      "Init method() {\n"
      "  value is 1;\n"
      "  ref is address of value;\n"
      "}\n");

  ASSERT_TRUE(result.buildErrors.empty());
  ASSERT_TRUE(result.module != nullptr);

  bool sawOwner = false;
  bool sawRef = false;
  for (const auto &hint : result.hints) {
    if (hint.label == " owner") {
      sawOwner = true;
    }
    if (hint.label.find(" ref value") != std::string::npos) {
      sawRef = true;
    }
  }

  ASSERT_TRUE(sawOwner);
  ASSERT_TRUE(sawRef);
  return true;
}
