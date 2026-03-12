// Parser tests - included from tests/test_main.cpp
#include "neuronc/lexer/Lexer.h"
#include "neuronc/parser/Parser.h"

#include <memory>
#include <string>
#include <vector>

using namespace neuron;

static std::unique_ptr<ProgramNode> parseProgramForParserTests(
    const std::string &source, std::vector<std::string> *outLexErrors,
    std::vector<std::string> *outParseErrors) {
  Lexer lexer(source, "<parser_test>");
  auto tokens = lexer.tokenize();
  *outLexErrors = lexer.errors();

  Parser parser(std::move(tokens), "<parser_test>");
  auto program = parser.parse();
  *outParseErrors = parser.errors();
  return program;
}

TEST(ParseModuleAndBinding) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "module Demo;\n"
      "x is 10 as int;\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 2u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::ModuleDecl);
  ASSERT_EQ(program->declarations[1]->type, ASTNodeType::BindingDecl);

  auto *binding = static_cast<BindingDeclNode *>(program->declarations[1].get());
  ASSERT_EQ(binding->name, "x");
  ASSERT_TRUE(binding->typeAnnotation != nullptr);
  ASSERT_EQ(binding->kind, BindingKind::Value);
  return true;
}

TEST(ParseModuleCppDeclaration) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "modulecpp Tensorflow;\n"
      "Init is method() { return; }\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_FALSE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  ASSERT_TRUE(parseErrors[0].find("modulecpp") != std::string::npos);
  return true;
}

TEST(ParseMethodDeclaration) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "sum is method(a as int, b as int) as int {\n"
      "  return a + b;\n"
      "};\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::MethodDecl);

  auto *method = static_cast<MethodDeclNode *>(program->declarations[0].get());
  ASSERT_EQ(method->name, "sum");
  ASSERT_EQ(method->parameters.size(), 2u);
  ASSERT_TRUE(method->returnType != nullptr);
  ASSERT_TRUE(method->body != nullptr);
  ASSERT_EQ(method->body->type, ASTNodeType::Block);

  auto *returnType = dynamic_cast<TypeSpecNode *>(method->returnType.get());
  ASSERT_TRUE(returnType != nullptr);
  ASSERT_EQ(returnType->typeName, "int");
  return true;
}

TEST(ParseMethodDeclarationRejectsTypeFirstParameters) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "sum is method(int a, int b) as int {\n"
      "  return a + b;\n"
      "};\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_FALSE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  bool sawAsError = false;
  for (const auto &err : parseErrors) {
    if (err.find("Expected 'as' after parameter name") != std::string::npos) {
      sawAsError = true;
      break;
    }
  }
  ASSERT_TRUE(sawAsError);
  return true;
}

TEST(ParseClassDeclaration) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Vector is class inherits Shape {\n"
      "  x is 0;\n"
      "  area is method() as int { return x; };\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::ClassDecl);

  auto *classDecl = static_cast<ClassDeclNode *>(program->declarations[0].get());
  ASSERT_EQ(classDecl->name, "Vector");
  ASSERT_EQ(classDecl->baseClasses.size(), 1u);
  ASSERT_EQ(classDecl->baseClasses[0], "Shape");
  ASSERT_EQ(classDecl->members.size(), 2u);
  ASSERT_EQ(classDecl->members[0]->type, ASTNodeType::BindingDecl);
  ASSERT_EQ(classDecl->members[1]->type, ASTNodeType::MethodDecl);
  return true;
}

TEST(ParseExpressionPrecedence) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests("result is 1 + 2 * 3;\n", &lexErrors,
                                            &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::BindingDecl);

  auto *binding = static_cast<BindingDeclNode *>(program->declarations[0].get());
  auto *top = dynamic_cast<BinaryExprNode *>(binding->value.get());
  ASSERT_TRUE(top != nullptr);
  ASSERT_EQ(top->op, TokenType::Plus);

  auto *rhs = dynamic_cast<BinaryExprNode *>(top->right.get());
  ASSERT_TRUE(rhs != nullptr);
  ASSERT_EQ(rhs->op, TokenType::Star);
  return true;
}

TEST(ParseErrorRecoveryMissingSemicolon) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "x is 1\n"
      "y is 2;\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_FALSE(parseErrors.empty());
  ASSERT_TRUE(program->declarations.size() >= 1u);
  return true;
}

TEST(ParseConstBinding) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests("const answer is 42 as int;\n",
                                            &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::BindingDecl);
  auto *binding = static_cast<BindingDeclNode *>(program->declarations[0].get());
  ASSERT_TRUE(binding->isConst);
  ASSERT_EQ(binding->name, "answer");
  return true;
}

TEST(ParseDecrementStatement) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Init is method() {\n"
      "  i is 2;\n"
      "  i--;\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  auto *method = static_cast<MethodDeclNode *>(program->declarations[0].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 2u);
  ASSERT_EQ(body->statements[1]->type, ASTNodeType::DecrementStmt);
  return true;
}

TEST(ParseSliceExpression) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Init is method() {\n"
      "  t is Tensor<float>.Random(4, 4);\n"
      "  s is t[0..2];\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  auto *method = static_cast<MethodDeclNode *>(program->declarations[0].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 2u);
  auto *sliceBinding = static_cast<BindingDeclNode *>(body->statements[1].get());
  ASSERT_TRUE(sliceBinding->value != nullptr);
  ASSERT_EQ(sliceBinding->value->type, ASTNodeType::SliceExpr);
  return true;
}

TEST(ParseAtomicBindingAndTypeofAndStaticAssert) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "macro DemoMacro;\n"
      "Init is method() {\n"
      "  atomic value is 1 as int;\n"
      "  typeName is typeof(value);\n"
      "  static_assert(true, \"ok\");\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 2u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::MacroDecl);
  ASSERT_EQ(program->declarations[1]->type, ASTNodeType::MethodDecl);

  auto *method = static_cast<MethodDeclNode *>(program->declarations[1].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 3u);
  ASSERT_EQ(body->statements[0]->type, ASTNodeType::BindingDecl);
  ASSERT_EQ(body->statements[1]->type, ASTNodeType::BindingDecl);
  ASSERT_EQ(body->statements[2]->type, ASTNodeType::StaticAssertStmt);
  auto *atomicBinding = static_cast<BindingDeclNode *>(body->statements[0].get());
  ASSERT_TRUE(atomicBinding->isAtomic);
  return true;
}

TEST(ParseMatchStatement) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
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
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 2u);

  auto *method = static_cast<MethodDeclNode *>(program->declarations[1].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 2u);
  ASSERT_EQ(body->statements[1]->type, ASTNodeType::MatchStmt);

  auto *matchStmt = static_cast<MatchStmtNode *>(body->statements[1].get());
  ASSERT_EQ(matchStmt->expressions.size(), 1u);
  ASSERT_EQ(matchStmt->arms.size(), 2u);
  auto *firstArm = static_cast<MatchArmNode *>(matchStmt->arms[0].get());
  ASSERT_FALSE(firstArm->isDefault);
  ASSERT_EQ(firstArm->patternExprs.size(), 1u);
  ASSERT_TRUE(firstArm->body != nullptr);
  auto *defaultArm = static_cast<MatchArmNode *>(matchStmt->arms[1].get());
  ASSERT_TRUE(defaultArm->isDefault);
  return true;
}

TEST(ParseCastPipelineStatement) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Init method() {\n"
      "  value is 10 as int;\n"
      "  value as maybe dynamic then string then float;\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);

  auto *method = static_cast<MethodDeclNode *>(program->declarations[0].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 2u);
  ASSERT_EQ(body->statements[1]->type, ASTNodeType::CastStmt);

  auto *castStmt = static_cast<CastStmtNode *>(body->statements[1].get());
  ASSERT_TRUE(castStmt->pipelineNullable);
  ASSERT_EQ(castStmt->steps.size(), 3u);
  return true;
}

TEST(ParseThreadExpressionAndExternDeclaration) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "extern Worker method() as void;\n"
      "Init is method() {\n"
      "  t is thread(Worker);\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 2u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::ExternDecl);
  ASSERT_EQ(program->declarations[1]->type, ASTNodeType::MethodDecl);

  auto *externDecl =
      static_cast<ExternDeclNode *>(program->declarations[0].get());
  ASSERT_TRUE(externDecl->declaration != nullptr);
  auto *externMethod =
      static_cast<MethodDeclNode *>(externDecl->declaration.get());
  ASSERT_TRUE(externMethod->isExtern);
  ASSERT_TRUE(externMethod->body == nullptr);

  auto *initMethod = static_cast<MethodDeclNode *>(program->declarations[1].get());
  auto *body = static_cast<BlockNode *>(initMethod->body.get());
  ASSERT_EQ(body->statements.size(), 1u);
  auto *threadBinding = static_cast<BindingDeclNode *>(body->statements[0].get());
  ASSERT_TRUE(threadBinding->value != nullptr);
  ASSERT_EQ(threadBinding->value->type, ASTNodeType::CallExpr);

  auto *threadCall = static_cast<CallExprNode *>(threadBinding->value.get());
  auto *callee = static_cast<IdentifierNode *>(threadCall->callee.get());
  ASSERT_EQ(callee->name, "thread");
  ASSERT_EQ(threadCall->arguments.size(), 1u);
  return true;
}

TEST(ParseMethodClassEnumWithoutIs) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Color enum { Red, Green, Blue };\n"
      "Shape interface {\n"
      "  Draw method() as void;\n"
      "}\n"
      "Point struct {\n"
      "  x is 0 as int;\n"
      "}\n"
      "Run method() as int {\n"
      "  return 1;\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 4u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::EnumDecl);
  ASSERT_EQ(program->declarations[1]->type, ASTNodeType::ClassDecl);
  ASSERT_EQ(program->declarations[2]->type, ASTNodeType::ClassDecl);
  ASSERT_EQ(program->declarations[3]->type, ASTNodeType::MethodDecl);

  auto *enumDecl = static_cast<EnumDeclNode *>(program->declarations[0].get());
  ASSERT_EQ(enumDecl->members.size(), 3u);
  auto *iface = static_cast<ClassDeclNode *>(program->declarations[1].get());
  ASSERT_EQ(iface->kind, ClassKind::Interface);
  auto *strct = static_cast<ClassDeclNode *>(program->declarations[2].get());
  ASSERT_EQ(strct->kind, ClassKind::Struct);
  return true;
}

TEST(ParseDynamicAndShorthandBindings) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Init method() {\n"
      "  a;\n"
      "  b 10;\n"
      "  c as dynamic;\n"
      "  d is 0 as dynamic;\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);

  auto *method = static_cast<MethodDeclNode *>(program->declarations[0].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 4u);
  ASSERT_EQ(body->statements[0]->type, ASTNodeType::BindingDecl);
  ASSERT_EQ(body->statements[1]->type, ASTNodeType::BindingDecl);
  ASSERT_EQ(body->statements[2]->type, ASTNodeType::CastStmt);
  ASSERT_EQ(body->statements[3]->type, ASTNodeType::BindingDecl);

  auto *aDecl = static_cast<BindingDeclNode *>(body->statements[0].get());
  ASSERT_EQ(aDecl->name, "a");
  ASSERT_TRUE(aDecl->value == nullptr);
  ASSERT_TRUE(aDecl->typeAnnotation != nullptr);

  auto *bDecl = static_cast<BindingDeclNode *>(body->statements[1].get());
  ASSERT_EQ(bDecl->name, "b");
  ASSERT_TRUE(bDecl->value != nullptr);

  auto *cDecl = static_cast<CastStmtNode *>(body->statements[2].get());
  ASSERT_TRUE(cDecl->target != nullptr);
  ASSERT_EQ(cDecl->steps.size(), 1u);
  return true;
}

TEST(ParseAllowsRepeatedSemicolons) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      ";;\n"
      "Init method() {\n"
      "  ;;\n"
      "  a 10;;;\n"
      "  ;\n"
      "}\n"
      ";\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);

  auto *method = static_cast<MethodDeclNode *>(program->declarations[0].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 1u);
  ASSERT_EQ(body->statements[0]->type, ASTNodeType::BindingDecl);
  return true;
}

TEST(ParseGpuBlockInsideMethod) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Init method() {\n"
      "  gpu {\n"
      "    x is 1 as int;\n"
      "  }\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::MethodDecl);

  auto *method = static_cast<MethodDeclNode *>(program->declarations[0].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 1u);
  ASSERT_EQ(body->statements[0]->type, ASTNodeType::GpuBlock);

  auto *gpuBlock = static_cast<GpuBlockNode *>(body->statements[0].get());
  ASSERT_TRUE(gpuBlock->body != nullptr);
  ASSERT_EQ(gpuBlock->body->type, ASTNodeType::Block);

  auto *gpuBody = static_cast<BlockNode *>(gpuBlock->body.get());
  ASSERT_EQ(gpuBody->statements.size(), 1u);
  ASSERT_EQ(gpuBody->statements[0]->type, ASTNodeType::BindingDecl);
  return true;
}

TEST(ParseGpuBlockRejectedAtTopLevel) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "gpu {\n"
      "  x is 1 as int;\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_FALSE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::GpuBlock);

  bool sawMethodOnlyError = false;
  for (const auto &err : parseErrors) {
    if (err.find("gpu block is only allowed inside method bodies") !=
        std::string::npos) {
      sawMethodOnlyError = true;
      break;
    }
  }
  ASSERT_TRUE(sawMethodOnlyError);
  return true;
}

TEST(ParseGpuBlockSelectorWithEnumStyleNamedOptions) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Init method() {\n"
      "  gpu(mode: GPUMode.Integrated, policy: GPUPolicy.Prefer) {\n"
      "    x is 1 as int;\n"
      "  }\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  auto *method = static_cast<MethodDeclNode *>(program->declarations[0].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 1u);
  ASSERT_EQ(body->statements[0]->type, ASTNodeType::GpuBlock);

  auto *gpuBlock = static_cast<GpuBlockNode *>(body->statements[0].get());
  ASSERT_EQ(gpuBlock->preferenceMode, GpuDevicePreferenceMode::Prefer);
  ASSERT_EQ(gpuBlock->preferenceTarget, GpuDevicePreferenceTarget::Integrated);
  return true;
}

TEST(ParseCallAllowsNamedArgumentSyntax) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Init method() {\n"
      "  Print(value: true);\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);

  auto *method = static_cast<MethodDeclNode *>(program->declarations[0].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 1u);
  ASSERT_EQ(body->statements[0]->type, ASTNodeType::CallExpr);

  auto *call = static_cast<CallExprNode *>(body->statements[0].get());
  ASSERT_EQ(call->arguments.size(), 1u);
  ASSERT_EQ(call->argumentLabels.size(), 1u);
  ASSERT_EQ(call->argumentLabels[0], "value");
  ASSERT_EQ(call->arguments[0]->type, ASTNodeType::BoolLiteral);
  return true;
}

TEST(ParseModuleQualifiedFusionChainCallSyntax) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Init method() {\n"
      "  result is NN.Normalize-Relu-Softmax(input);\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);

  auto *method = static_cast<MethodDeclNode *>(program->declarations[0].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 1u);

  auto *binding = static_cast<BindingDeclNode *>(body->statements[0].get());
  ASSERT_TRUE(binding->value != nullptr);
  ASSERT_EQ(binding->value->type, ASTNodeType::CallExpr);

  auto *outerCall = static_cast<CallExprNode *>(binding->value.get());
  ASSERT_TRUE(outerCall->isFusionChain);
  ASSERT_EQ(outerCall->fusionCallNames.size(), 3u);
  ASSERT_EQ(outerCall->fusionCallNames[0], "NN.Normalize");
  ASSERT_EQ(outerCall->fusionCallNames[1], "NN.Relu");
  ASSERT_EQ(outerCall->fusionCallNames[2], "NN.Softmax");
  ASSERT_EQ(outerCall->arguments.size(), 1u);
  ASSERT_EQ(outerCall->callee->type, ASTNodeType::MemberAccessExpr);

  auto *outerCallee = static_cast<MemberAccessNode *>(outerCall->callee.get());
  ASSERT_EQ(outerCallee->member, "Softmax");

  auto *middleCall = static_cast<CallExprNode *>(outerCall->arguments[0].get());
  ASSERT_EQ(middleCall->callee->type, ASTNodeType::MemberAccessExpr);
  auto *middleCallee =
      static_cast<MemberAccessNode *>(middleCall->callee.get());
  ASSERT_EQ(middleCallee->member, "Relu");

  auto *innerCall = static_cast<CallExprNode *>(middleCall->arguments[0].get());
  ASSERT_EQ(innerCall->callee->type, ASTNodeType::MemberAccessExpr);
  auto *innerCallee = static_cast<MemberAccessNode *>(innerCall->callee.get());
  ASSERT_EQ(innerCallee->member, "Normalize");
  ASSERT_EQ(innerCall->arguments.size(), 1u);
  ASSERT_EQ(innerCall->arguments[0]->type, ASTNodeType::Identifier);

  auto *inputArg = static_cast<IdentifierNode *>(innerCall->arguments[0].get());
  ASSERT_EQ(inputArg->name, "input");
  return true;
}

TEST(ParseInputGenericFluentCallChain) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Init method() {\n"
      "  age is Input<int>(\"Age: \").Min(18).Max(99).Default(21).TimeoutMs(5000);\n"
      "  passwordValue is Input<string>(\"Pass: \").Secret();\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);

  auto *method = static_cast<MethodDeclNode *>(program->declarations[0].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 2u);

  auto *ageBinding = static_cast<BindingDeclNode *>(body->statements[0].get());
  ASSERT_TRUE(ageBinding->value != nullptr);
  ASSERT_EQ(ageBinding->value->type, ASTNodeType::InputExpr);

  auto *ageInput = static_cast<InputExprNode *>(ageBinding->value.get());
  ASSERT_EQ(ageInput->typeArguments.size(), 1u);
  ASSERT_EQ(ageInput->promptArguments.size(), 1u);
  ASSERT_EQ(ageInput->stages.size(), 4u);
  ASSERT_EQ(ageInput->typeArguments[0]->type, ASTNodeType::Identifier);
  auto *genericArg =
      static_cast<IdentifierNode *>(ageInput->typeArguments[0].get());
  ASSERT_EQ(genericArg->name, "int");
  ASSERT_EQ(ageInput->promptArguments[0]->type, ASTNodeType::StringLiteral);
  ASSERT_EQ(ageInput->stages[0].method, "Min");
  ASSERT_EQ(ageInput->stages[1].method, "Max");
  ASSERT_EQ(ageInput->stages[2].method, "Default");
  ASSERT_EQ(ageInput->stages[3].method, "TimeoutMs");
  ASSERT_EQ(ageInput->stages[3].arguments.size(), 1u);
  ASSERT_EQ(ageInput->stages[3].arguments[0]->type, ASTNodeType::IntLiteral);

  auto *passBinding = static_cast<BindingDeclNode *>(body->statements[1].get());
  ASSERT_TRUE(passBinding->value != nullptr);
  ASSERT_EQ(passBinding->value->type, ASTNodeType::InputExpr);
  auto *passInput = static_cast<InputExprNode *>(passBinding->value.get());
  ASSERT_EQ(passInput->typeArguments.size(), 1u);
  ASSERT_EQ(passInput->stages.size(), 1u);
  ASSERT_EQ(passInput->stages[0].method, "Secret");
  ASSERT_EQ(passInput->typeArguments[0]->type, ASTNodeType::Identifier);
  auto *stringGenericArg =
      static_cast<IdentifierNode *>(passInput->typeArguments[0].get());
  ASSERT_EQ(stringGenericArg->name, "string");
  return true;
}

TEST(ParseInputWithoutGenericKeepsTypeOpenForDefaultStringBehavior) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Init method() {\n"
      "  value is Input(\"Enter: \").Secret().Default(\"guest\");\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);

  auto *method = static_cast<MethodDeclNode *>(program->declarations[0].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 1u);

  auto *binding = static_cast<BindingDeclNode *>(body->statements[0].get());
  ASSERT_TRUE(binding->value != nullptr);
  ASSERT_EQ(binding->value->type, ASTNodeType::InputExpr);

  auto *inputExpr = static_cast<InputExprNode *>(binding->value.get());
  ASSERT_TRUE(inputExpr->typeArguments.empty());
  ASSERT_EQ(inputExpr->promptArguments.size(), 1u);
  ASSERT_EQ(inputExpr->promptArguments[0]->type, ASTNodeType::StringLiteral);
  ASSERT_EQ(inputExpr->stages.size(), 2u);
  ASSERT_EQ(inputExpr->stages[0].method, "Secret");
  ASSERT_EQ(inputExpr->stages[1].method, "Default");
  return true;
}

TEST(ParseMultiBindingDeclarationSharesInitializerAcrossBindings) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests("x, y is 0 as int;\n", &lexErrors,
                                            &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 2u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::BindingDecl);
  ASSERT_EQ(program->declarations[1]->type, ASTNodeType::BindingDecl);

  auto *first = static_cast<BindingDeclNode *>(program->declarations[0].get());
  auto *second = static_cast<BindingDeclNode *>(program->declarations[1].get());
  ASSERT_EQ(first->name, "x");
  ASSERT_EQ(second->name, "y");
  ASSERT_TRUE(first->value != nullptr);
  ASSERT_TRUE(second->value != nullptr);
  ASSERT_EQ(first->value->type, ASTNodeType::IntLiteral);
  ASSERT_EQ(second->value->type, ASTNodeType::IntLiteral);
  ASSERT_TRUE(first->typeAnnotation != nullptr);
  ASSERT_TRUE(second->typeAnnotation != nullptr);
  return true;
}

TEST(ParseMatchExpressionSupportsMultipleSelectorsAndPatterns) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Color enum { Red, Yellow, Green };\n"
      "Init method() {\n"
      "  result is match(firstColor, secondColor) {\n"
      "    Color.Red, Color.Yellow then Color.Green;\n"
      "    default then Color.Red;\n"
      "  };\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 2u);

  auto *method = static_cast<MethodDeclNode *>(program->declarations[1].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 1u);

  auto *binding = static_cast<BindingDeclNode *>(body->statements[0].get());
  ASSERT_TRUE(binding->value != nullptr);
  ASSERT_EQ(binding->value->type, ASTNodeType::MatchExpr);

  auto *matchExpr = static_cast<MatchExprNode *>(binding->value.get());
  ASSERT_EQ(matchExpr->expressions.size(), 2u);
  ASSERT_EQ(matchExpr->arms.size(), 2u);

  auto *firstArm = static_cast<MatchArmNode *>(matchExpr->arms[0].get());
  ASSERT_FALSE(firstArm->isDefault);
  ASSERT_EQ(firstArm->patternExprs.size(), 2u);
  ASSERT_TRUE(firstArm->valueExpr != nullptr);

  auto *defaultArm = static_cast<MatchArmNode *>(matchExpr->arms[1].get());
  ASSERT_TRUE(defaultArm->isDefault);
  ASSERT_TRUE(defaultArm->valueExpr != nullptr);
  return true;
}

TEST(ParseImplicitSingleStatementBodiesForMethodsAndControlFlow) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Echo method(value as string)\n"
      "  Print(value);\n"
      "Init method(items as Array<int>) {\n"
      "  if(true) Print(\"if\"); else Print(\"else\");\n"
      "  while(false) Print(\"while\");\n"
      "  for(i is 0; i < 1; i++) Print(i);\n"
      "  for(item in items) Print(item);\n"
      "  match(color) {\n"
      "    default then Print(\"match\");\n"
      "  }\n"
      "  try Print(\"try\"); catch(error) Print(error); finally Print(\"finally\");\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 2u);

  auto *echoMethod = static_cast<MethodDeclNode *>(program->declarations[0].get());
  auto *echoBody = static_cast<BlockNode *>(echoMethod->body.get());
  ASSERT_FALSE(echoBody->hasExplicitBraces);
  ASSERT_EQ(echoBody->statements.size(), 1u);
  ASSERT_EQ(echoBody->statements[0]->type, ASTNodeType::CallExpr);

  auto *initMethod = static_cast<MethodDeclNode *>(program->declarations[1].get());
  auto *initBody = static_cast<BlockNode *>(initMethod->body.get());
  ASSERT_EQ(initBody->statements.size(), 6u);

  auto *ifStmt = static_cast<IfStmtNode *>(initBody->statements[0].get());
  ASSERT_FALSE(static_cast<BlockNode *>(ifStmt->thenBlock.get())->hasExplicitBraces);
  ASSERT_FALSE(static_cast<BlockNode *>(ifStmt->elseBlock.get())->hasExplicitBraces);

  auto *whileStmt = static_cast<WhileStmtNode *>(initBody->statements[1].get());
  ASSERT_FALSE(static_cast<BlockNode *>(whileStmt->body.get())->hasExplicitBraces);

  auto *forStmt = static_cast<ForStmtNode *>(initBody->statements[2].get());
  ASSERT_FALSE(static_cast<BlockNode *>(forStmt->body.get())->hasExplicitBraces);

  auto *forInStmt = static_cast<ForInStmtNode *>(initBody->statements[3].get());
  ASSERT_FALSE(static_cast<BlockNode *>(forInStmt->body.get())->hasExplicitBraces);

  auto *matchStmt = static_cast<MatchStmtNode *>(initBody->statements[4].get());
  auto *defaultArm = static_cast<MatchArmNode *>(matchStmt->arms[0].get());
  ASSERT_TRUE(defaultArm->body != nullptr);
  ASSERT_FALSE(static_cast<BlockNode *>(defaultArm->body.get())->hasExplicitBraces);

  auto *tryStmt = static_cast<TryStmtNode *>(initBody->statements[5].get());
  ASSERT_FALSE(static_cast<BlockNode *>(tryStmt->tryBlock.get())->hasExplicitBraces);
  auto *catchClause =
      static_cast<CatchClauseNode *>(tryStmt->catchClauses[0].get());
  ASSERT_FALSE(static_cast<BlockNode *>(catchClause->body.get())->hasExplicitBraces);
  ASSERT_FALSE(
      static_cast<BlockNode *>(tryStmt->finallyBlock.get())->hasExplicitBraces);
  return true;
}

TEST(ParseMethodShorthandPascalCaseBlock) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Init method() {\n"
      "  OnOpen {\n"
      "    Print(\"open\");\n"
      "  }\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);

  auto *method = static_cast<MethodDeclNode *>(program->declarations[0].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 1u);
  ASSERT_EQ(body->statements[0]->type, ASTNodeType::MethodDecl);
  auto *eventMethod = static_cast<MethodDeclNode *>(body->statements[0].get());
  ASSERT_EQ(eventMethod->name, "OnOpen");
  ASSERT_TRUE(eventMethod->returnType != nullptr);
  return true;
}

TEST(ParseCanvasBlockWithInlineAndExplicitHandlers) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Init method() {\n"
      "  canvas(window, onResize: ResizeHandler) {\n"
      "    OnOpen { Print(\"open\"); }\n"
      "    OnFrame { Print(\"frame\"); }\n"
      "    OnClose { Print(\"close\"); }\n"
      "  }\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  auto *method = static_cast<MethodDeclNode *>(program->declarations[0].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  ASSERT_EQ(body->statements.size(), 1u);
  ASSERT_EQ(body->statements[0]->type, ASTNodeType::CanvasStmt);

  auto *canvas = static_cast<CanvasStmtNode *>(body->statements[0].get());
  ASSERT_TRUE(canvas->windowExpr != nullptr);
  ASSERT_EQ(canvas->handlers.size(), 4u);
  return true;
}

TEST(ParseShaderDeclarationWithPassStatement) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "GradientEffect is shader {\n"
      "  speed as float;\n"
      "  Vertex method(position as Vector3, uv as Vector2) {\n"
      "    pass uv;\n"
      "    return position;\n"
      "  }\n"
      "  Fragment method(uv as Vector2) {\n"
      "    return uv;\n"
      "  }\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::ShaderDecl);

  auto *shader = static_cast<ShaderDeclNode *>(program->declarations[0].get());
  ASSERT_EQ(shader->uniforms.size(), 1u);
  ASSERT_EQ(shader->stages.size(), 2u);

  auto *vertexStage = static_cast<ShaderStageNode *>(shader->stages[0].get());
  auto *vertexMethod = static_cast<MethodDeclNode *>(vertexStage->methodDecl.get());
  auto *vertexBody = static_cast<BlockNode *>(vertexMethod->body.get());
  ASSERT_EQ(vertexBody->statements[0]->type, ASTNodeType::ShaderPassStmt);
  return true;
}

TEST(ParseShaderDeclarationCollectsCpuSideDescriptorMethods) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "GradientEffect is shader {\n"
      "  tint as Color;\n"
      "  DefaultTint method() as Color {\n"
      "    return Color(1.0, 1.0, 1.0, 1.0);\n"
      "  }\n"
      "  Vertex method(position as Vector3) {\n"
      "    return position;\n"
      "  }\n"
      "  Fragment method() {\n"
      "    return tint;\n"
      "  }\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::ShaderDecl);

  auto *shader = static_cast<ShaderDeclNode *>(program->declarations[0].get());
  ASSERT_EQ(shader->uniforms.size(), 1u);
  ASSERT_EQ(shader->methods.size(), 1u);
  ASSERT_EQ(shader->stages.size(), 2u);
  ASSERT_EQ(shader->methods[0]->type, ASTNodeType::MethodDecl);
  auto *cpuMethod = static_cast<MethodDeclNode *>(shader->methods[0].get());
  ASSERT_EQ(cpuMethod->name, "DefaultTint");
  return true;
}

TEST(ParseNullLiteralBinding) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests("value is null;\n", &lexErrors,
                                            &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::BindingDecl);

  auto *binding = static_cast<BindingDeclNode *>(program->declarations[0].get());
  ASSERT_TRUE(binding->value != nullptr);
  ASSERT_EQ(binding->value->type, ASTNodeType::NullLiteral);
  return true;
}

TEST(ParseRecoveryKeepsFollowingDeclarationsAfterHalfWrittenMethod) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "Init is method(value as int {\n"
      "  total is value +\n"
      "}\n"
      "Next is method() {\n"
      "  return;\n"
      "}\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_FALSE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 2u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::MethodDecl);
  ASSERT_EQ(program->declarations[1]->type, ASTNodeType::MethodDecl);

  auto *initMethod = static_cast<MethodDeclNode *>(program->declarations[0].get());
  ASSERT_TRUE(initMethod->body != nullptr);
  auto *body = static_cast<BlockNode *>(initMethod->body.get());
  ASSERT_EQ(body->statements.size(), 1u);
  ASSERT_EQ(body->statements[0]->type, ASTNodeType::BindingDecl);

  auto *binding = static_cast<BindingDeclNode *>(body->statements[0].get());
  ASSERT_TRUE(binding->value != nullptr);
  ASSERT_EQ(binding->value->type, ASTNodeType::BinaryExpr);
  auto *binary = static_cast<BinaryExprNode *>(binding->value.get());
  ASSERT_TRUE(binary->right != nullptr);
  ASSERT_EQ(binary->right->type, ASTNodeType::Error);
  return true;
}

TEST(ParseExpandModuleDeclaration) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "expand module MathCore;\n"
      "module Math;\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 2u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::ExpandModuleDecl);
  ASSERT_EQ(program->declarations[1]->type, ASTNodeType::ModuleDecl);

  auto *expandDecl =
      static_cast<ExpandModuleDeclNode *>(program->declarations[0].get());
  ASSERT_EQ(expandDecl->moduleName, "MathCore");
  return true;
}

TEST(ParseCaretAndCaretCaretExpressionPrecedence) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "result is 1 + 2 ^ 3 ^^ 2 * 4;\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  auto *binding = static_cast<BindingDeclNode *>(program->declarations[0].get());
  auto *top = dynamic_cast<BinaryExprNode *>(binding->value.get());
  ASSERT_TRUE(top != nullptr);
  ASSERT_EQ(top->op, TokenType::Plus);

  auto *mul = dynamic_cast<BinaryExprNode *>(top->right.get());
  ASSERT_TRUE(mul != nullptr);
  ASSERT_EQ(mul->op, TokenType::Star);

  auto *root = dynamic_cast<BinaryExprNode *>(mul->left.get());
  ASSERT_TRUE(root != nullptr);
  ASSERT_EQ(root->op, TokenType::CaretCaret);

  auto *pow = dynamic_cast<BinaryExprNode *>(root->left.get());
  ASSERT_TRUE(pow != nullptr);
  ASSERT_EQ(pow->op, TokenType::Caret);
  return true;
}

TEST(ParseCaretTokensFromLexer) {
  Lexer lexer("value is a ^ b ^^ 2;", "<parser_test>");
  auto tokens = lexer.tokenize();
  ASSERT_TRUE(lexer.errors().empty());

  bool sawCaret = false;
  bool sawCaretCaret = false;
  for (const auto &token : tokens) {
    if (token.type == TokenType::Caret) {
      sawCaret = true;
    }
    if (token.type == TokenType::CaretCaret) {
      sawCaretCaret = true;
    }
  }

  ASSERT_TRUE(sawCaret);
  ASSERT_TRUE(sawCaretCaret);
  return true;
}

TEST(ParsePlainExternMethodDeclaration) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "extern FileExists method(path as string) as bool;\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::ExternDecl);

  auto *externDecl = static_cast<ExternDeclNode *>(program->declarations[0].get());
  ASSERT_FALSE(externDecl->symbolOverride.has_value());
  ASSERT_TRUE(externDecl->declaration != nullptr);
  ASSERT_EQ(externDecl->declaration->type, ASTNodeType::MethodDecl);

  auto *method = static_cast<MethodDeclNode *>(externDecl->declaration.get());
  ASSERT_TRUE(method->isExtern);
  ASSERT_EQ(method->name, "FileExists");
  ASSERT_EQ(method->parameters.size(), 1u);
  ASSERT_TRUE(method->returnType != nullptr);
  ASSERT_FALSE(method->externSymbolOverride.has_value());
  ASSERT_TRUE(method->body == nullptr);
  return true;
}

TEST(ParseInlineSymbolExternMethodDeclaration) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "extern(\"fs_file_exists\") FileExists method(path as string) as bool;\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_TRUE(parseErrors.empty());
  ASSERT_EQ(program->declarations.size(), 1u);
  ASSERT_EQ(program->declarations[0]->type, ASTNodeType::ExternDecl);

  auto *externDecl = static_cast<ExternDeclNode *>(program->declarations[0].get());
  ASSERT_TRUE(externDecl->symbolOverride.has_value());
  ASSERT_EQ(*externDecl->symbolOverride, "fs_file_exists");
  ASSERT_TRUE(externDecl->declaration != nullptr);
  ASSERT_EQ(externDecl->declaration->type, ASTNodeType::MethodDecl);

  auto *method = static_cast<MethodDeclNode *>(externDecl->declaration.get());
  ASSERT_TRUE(method->isExtern);
  ASSERT_TRUE(method->externSymbolOverride.has_value());
  ASSERT_EQ(*method->externSymbolOverride, "fs_file_exists");
  ASSERT_TRUE(method->body == nullptr);
  return true;
}

TEST(ParseRemovedModuleCppDeclarationProducesParseError) {
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;
  auto program = parseProgramForParserTests(
      "modulecpp FileSystemNative;\n",
      &lexErrors, &parseErrors);

  ASSERT_TRUE(lexErrors.empty());
  ASSERT_FALSE(parseErrors.empty());
  ASSERT_TRUE(program->declarations.empty());
  ASSERT_TRUE(parseErrors[0].find("modulecpp") != std::string::npos);
  return true;
}
