// Semantic analyzer tests - included from tests/test_main.cpp
#include "neuronc/frontend/Frontend.h"
#include "neuronc/lexer/Lexer.h"
#include "neuronc/parser/Parser.h"
#include "neuronc/sema/SemanticAnalyzer.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace neuron;

static std::unique_ptr<ProgramNode>
parseForSemanticTests(const std::string &source, std::vector<std::string> *errors,
                      const std::string &filename = "<sema_test>") {
  Lexer lexer(source, filename);
  auto tokens = lexer.tokenize();
  errors->insert(errors->end(), lexer.errors().begin(), lexer.errors().end());

  Parser parser(std::move(tokens), filename);
  auto ast = parser.parse();
  errors->insert(errors->end(), parser.errors().begin(), parser.errors().end());
  return ast;
}

static std::filesystem::path
writeNavigationTestFile(const std::string &filename, const std::string &source) {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / filename;
  std::ofstream output(path, std::ios::binary);
  output << source;
  return path;
}

TEST(SemanticModuleResolutionFailsOnMissingModule) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "module MissingPkg;\n"
      "Init is method() { Print(\"x\"); }\n",
      &errors);
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setAvailableModules(std::unordered_set<std::string>{"system"}, true);
  sema.analyze(ast.get());

  ASSERT_TRUE(sema.hasErrors());
  return true;
}

TEST(SemanticTryCatchThrowBasicFlow) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "module System;\n"
      "Init is method() {\n"
      "  try {\n"
      "    throw \"boom\";\n"
      "  } catch(error) {\n"
      "    Print(error);\n"
      "  } finally {\n"
      "    Print(\"done\");\n"
      "  }\n"
      "}\n",
      &errors);
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setAvailableModules(std::unordered_set<std::string>{"system"}, true);
  sema.analyze(ast.get());

  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticRejectsMultipleClassesWhenRuleEnabled) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "module System;\n"
      "A is class { x is 1 as int; }\n"
      "B is class { y is 2 as int; }\n",
      &errors, "ManyClasses.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setAvailableModules(std::unordered_set<std::string>{"system"}, true);
  sema.setSingleClassPerFile(true);
  sema.analyze(ast.get());

  ASSERT_TRUE(sema.hasErrors());
  bool sawRuleError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Multiple classes defined in module") !=
        std::string::npos) {
      sawRuleError = true;
      break;
    }
  }
  ASSERT_TRUE(sawRuleError);
  return true;
}

TEST(SemanticClassLimitSupportsCustomMax) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "module System;\n"
      "A is class { x is 1 as int; }\n"
      "B is class { y is 2 as int; }\n",
      &errors, "TwoClasses.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setAvailableModules(std::unordered_set<std::string>{"system"}, true);
  sema.setMaxClassesPerFile(2);
  sema.analyze(ast.get());

  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticInheritanceResolvesBaseMembers) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Animal is class {\n"
      "  age is 3 as int;\n"
      "}\n"
      "Dog is class inherits Animal {\n"
      "  GetAge is method() as int {\n"
      "    return this.age;\n"
      "  }\n"
      "}\n",
      &errors, "Inheritance.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setSingleClassPerFile(false);
  sema.analyze(ast.get());

  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticInheritanceRejectsSelfAndDuplicateBases) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Dog is class inherits Dog, Animal, Animal {\n"
      "}\n",
      &errors, "Dog.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setSingleClassPerFile(false);
  sema.analyze(ast.get());

  ASSERT_TRUE(sema.hasErrors());
  bool sawSelfError = false;
  bool sawDuplicateError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("cannot inherit itself") != std::string::npos) {
      sawSelfError = true;
    }
    if (err.message.find("Duplicate base class in inherits list") !=
        std::string::npos) {
      sawDuplicateError = true;
    }
  }
  ASSERT_TRUE(sawSelfError);
  ASSERT_TRUE(sawDuplicateError);
  return true;
}

TEST(SemanticMethodNameUppercaseRuleCanBeToggled) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "foo is method() {\n"
      "  return;\n"
      "}\n",
      &errors, "MethodName.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer strictSema;
  strictSema.setRequireMethodUppercaseStart(true);
  strictSema.analyze(ast.get());
  ASSERT_TRUE(strictSema.hasErrors());

  SemanticAnalyzer relaxedSema;
  relaxedSema.setRequireMethodUppercaseStart(false);
  relaxedSema.analyze(ast.get());
  ASSERT_FALSE(relaxedSema.hasErrors());
  return true;
}

TEST(SemanticRejectsBindingThatUsesExistingMethodName) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests("Print;\n", &errors, "BareMethodName.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());

  ASSERT_TRUE(sema.hasErrors());
  ASSERT_EQ(sema.getErrors().size(), static_cast<std::size_t>(1));
  ASSERT_EQ(
      sema.getErrors()[0].message,
      "Identifier 'Print' refers to a method. Call it like 'Print(...)'; if "
      "you meant to declare a variable, start the name with a lowercase "
      "letter or '_'.");
  return true;
}

TEST(SemanticTreatsInputAsBuiltinMethodName) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests("Input;\n", &errors, "BareInputName.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());

  ASSERT_TRUE(sema.hasErrors());
  ASSERT_EQ(sema.getErrors().size(), static_cast<std::size_t>(1));
  ASSERT_EQ(
      sema.getErrors()[0].message,
      "Identifier 'Input' refers to a method. Call it like 'Input(...)'; if "
      "you meant to declare a variable, start the name with a lowercase "
      "letter or '_'.");
  return true;
}

TEST(SemanticMethodNameRejectsNonAlnum) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Foo_Bar is method() {\n"
      "  return;\n"
      "}\n",
      &errors, "MethodUnderscore.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setRequireMethodUppercaseStart(false);
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());
  return true;
}

TEST(SemanticExposesInferredTypeForHoverStyleQueries) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "module System;\n"
      "Init is method() {\n"
      "  value is 1 as int;\n"
      "  Print(value);\n"
      "}\n",
      &errors);
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  auto *method = static_cast<MethodDeclNode *>(ast->declarations[1].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  auto *binding = static_cast<BindingDeclNode *>(body->statements[0].get());
  auto *call = static_cast<CallExprNode *>(body->statements[1].get());
  auto *identifier = static_cast<IdentifierNode *>(call->arguments[0].get());

  SemanticAnalyzer sema;
  sema.setAvailableModules(std::unordered_set<std::string>{"system"}, true);
  sema.analyze(ast.get());

  ASSERT_FALSE(sema.hasErrors());
  ASSERT_TRUE(sema.getInferredType(binding) != nullptr);
  ASSERT_TRUE(sema.getInferredType(identifier) != nullptr);
  ASSERT_EQ(sema.getInferredType(binding)->toString(), "int");
  ASSERT_EQ(sema.getInferredType(identifier)->toString(), "int");
  return true;
}

TEST(SemanticTracksDefinitionAndReferencesForBindings) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  value is 1 as int;\n"
      "  Print(value);\n"
      "  value 2;\n"
      "}\n",
      &errors, "NavBinding.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());

  const auto definition = sema.getDefinitionLocation({3, 9, "NavBinding.npp"});
  ASSERT_TRUE(definition.has_value());
  ASSERT_EQ(definition->location.line, 2);
  ASSERT_EQ(definition->location.column, 3);

  const auto references = sema.getReferenceLocations({2, 3, "NavBinding.npp"});
  ASSERT_EQ(references.size(), 2u);
  ASSERT_EQ(references[0].location.line, 3);
  ASSERT_EQ(references[1].location.line, 4);
  return true;
}

TEST(SemanticTracksDefinitionForMemberAccessAndParameters) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Box is class {\n"
      "  value is 1 as int;\n"
      "  GetValue is method(input as int) as int {\n"
      "    return this.value + input;\n"
      "  }\n"
      "}\n",
      &errors, "NavMember.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  auto *classDecl = static_cast<ClassDeclNode *>(ast->declarations[0].get());
  auto *method = static_cast<MethodDeclNode *>(classDecl->members[1].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  auto *returnStmt = static_cast<ReturnStmtNode *>(body->statements[0].get());
  auto *binary = static_cast<BinaryExprNode *>(returnStmt->value.get());
  auto *member = static_cast<MemberAccessNode *>(binary->left.get());
  auto *identifier = static_cast<IdentifierNode *>(binary->right.get());

  SemanticAnalyzer sema;
  sema.analyze(ast.get());

  const auto fieldDefinition =
      sema.getDefinitionLocation(member->memberLocation);
  ASSERT_TRUE(fieldDefinition.has_value());
  ASSERT_EQ(fieldDefinition->location.line, 2);
  ASSERT_EQ(fieldDefinition->location.column, 3);

  const auto parameterDefinition =
      sema.getDefinitionLocation(identifier->location);
  ASSERT_TRUE(parameterDefinition.has_value());
  ASSERT_EQ(parameterDefinition->location.line, method->parameters[0].location.line);
  ASSERT_EQ(parameterDefinition->location.column,
            method->parameters[0].location.column);
  return true;
}

TEST(SemanticResolvedSymbolsExposeKindsForDefinitionsAndReferences) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Box is class {\n"
      "  value is 1 as int;\n"
      "  GetValue is method(input as int) as int {\n"
      "    local is value;\n"
      "    return this.value + input + local;\n"
      "  }\n"
      "}\n",
      &errors, "ResolvedKinds.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  auto *classDecl = static_cast<ClassDeclNode *>(ast->declarations[0].get());
  auto *field = static_cast<BindingDeclNode *>(classDecl->members[0].get());
  auto *method = static_cast<MethodDeclNode *>(classDecl->members[1].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  auto *localBinding = static_cast<BindingDeclNode *>(body->statements[0].get());
  auto *returnStmt = static_cast<ReturnStmtNode *>(body->statements[1].get());
  auto *outerBinary = static_cast<BinaryExprNode *>(returnStmt->value.get());
  auto *leftBinary = static_cast<BinaryExprNode *>(outerBinary->left.get());
  auto *member = static_cast<MemberAccessNode *>(leftBinary->left.get());
  auto *parameterRef = static_cast<IdentifierNode *>(leftBinary->right.get());
  auto *localRef = static_cast<IdentifierNode *>(outerBinary->right.get());

  SemanticAnalyzer sema;
  sema.analyze(ast.get());

  const auto fieldSymbol = sema.getResolvedSymbol(field->location);
  ASSERT_TRUE(fieldSymbol.has_value());
  ASSERT_EQ(fieldSymbol->kind, SymbolKind::Field);

  const auto methodSymbol = sema.getResolvedSymbol(method->location);
  ASSERT_TRUE(methodSymbol.has_value());
  ASSERT_EQ(methodSymbol->kind, SymbolKind::Method);

  const auto parameterSymbol =
      sema.getResolvedSymbol(method->parameters[0].location);
  ASSERT_TRUE(parameterSymbol.has_value());
  ASSERT_EQ(parameterSymbol->kind, SymbolKind::Parameter);

  const auto localSymbol = sema.getResolvedSymbol(localBinding->location);
  ASSERT_TRUE(localSymbol.has_value());
  ASSERT_EQ(localSymbol->kind, SymbolKind::Variable);

  const auto memberRef = sema.getResolvedSymbol(member->memberLocation);
  ASSERT_TRUE(memberRef.has_value());
  ASSERT_EQ(memberRef->kind, SymbolKind::Field);
  ASSERT_TRUE(memberRef->definition.has_value());
  ASSERT_EQ(memberRef->definition->location.line, field->location.line);
  ASSERT_EQ(memberRef->definition->location.column, field->location.column);

  const auto parameterRefSymbol = sema.getResolvedSymbol(parameterRef->location);
  ASSERT_TRUE(parameterRefSymbol.has_value());
  ASSERT_EQ(parameterRefSymbol->kind, SymbolKind::Parameter);
  ASSERT_TRUE(parameterRefSymbol->definition.has_value());
  ASSERT_EQ(parameterRefSymbol->definition->location.line,
            method->parameters[0].location.line);
  ASSERT_EQ(parameterRefSymbol->definition->location.column,
            method->parameters[0].location.column);

  const auto localRefSymbol = sema.getResolvedSymbol(localRef->location);
  ASSERT_TRUE(localRefSymbol.has_value());
  ASSERT_EQ(localRefSymbol->kind, SymbolKind::Variable);
  ASSERT_TRUE(localRefSymbol->definition.has_value());
  ASSERT_EQ(localRefSymbol->definition->location.line, localBinding->location.line);
  ASSERT_EQ(localRefSymbol->definition->location.column,
            localBinding->location.column);
  return true;
}

TEST(SemanticProgramViewResolvesImportedSymbolsAcrossFiles) {
  std::vector<std::string> errors;
  auto boxAst = parseForSemanticTests(
      "Box is class {\n"
      "  value is 1 as int;\n"
      "}\n",
      &errors, "Box.npp");
  auto useAst = parseForSemanticTests(
      "module Box;\n"
      "Init is method() as int {\n"
      "  box is Box();\n"
      "  return box.value;\n"
      "}\n",
      &errors, "UseBox.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(boxAst != nullptr);
  ASSERT_TRUE(useAst != nullptr);

  std::vector<ASTNode *> declarations;
  for (auto &decl : boxAst->declarations) {
    declarations.push_back(decl.get());
  }
  for (auto &decl : useAst->declarations) {
    declarations.push_back(decl.get());
  }

  auto *method = static_cast<MethodDeclNode *>(useAst->declarations[1].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  auto *binding = static_cast<BindingDeclNode *>(body->statements[0].get());
  auto *call = static_cast<CallExprNode *>(binding->value.get());
  auto *classRef = call->callee.get();
  auto *returnStmt = static_cast<ReturnStmtNode *>(body->statements[1].get());
  auto *member = static_cast<MemberAccessNode *>(returnStmt->value.get());

  ProgramView programView;
  programView.location = {1, 1, "UseBox.npp"};
  programView.moduleName = "UseBox";
  programView.declarations = declarations;

  SemanticAnalyzer sema;
  sema.setAvailableModules(std::unordered_set<std::string>{"box"}, true);
  sema.analyze(programView);

  ASSERT_FALSE(sema.hasErrors());

  const auto classSymbol = sema.getResolvedSymbol(classRef->location);
  ASSERT_TRUE(classSymbol.has_value());
  ASSERT_EQ(classSymbol->kind, SymbolKind::Class);
  ASSERT_TRUE(classSymbol->definition.has_value());
  ASSERT_EQ(classSymbol->definition->location.file, "Box.npp");
  ASSERT_EQ(classSymbol->definition->location.line, 1);

  const auto fieldSymbol = sema.getResolvedSymbol(member->memberLocation);
  ASSERT_TRUE(fieldSymbol.has_value());
  ASSERT_EQ(fieldSymbol->kind, SymbolKind::Field);
  ASSERT_TRUE(fieldSymbol->definition.has_value());
  ASSERT_EQ(fieldSymbol->definition->location.file, "Box.npp");
  ASSERT_EQ(fieldSymbol->definition->location.line, 2);
  ASSERT_EQ(fieldSymbol->definition->location.column, 3);
  return true;
}

TEST(SemanticProgramViewResolvesImportedMethodCallsAcrossFiles) {
  std::vector<std::string> errors;
  auto utilAst = parseForSemanticTests(
      "PrintValue is method(value as int) {\n"
      "  return;\n"
      "}\n",
      &errors, "Util.npp");
  auto useAst = parseForSemanticTests(
      "module Util;\n"
      "Init is method() {\n"
      "  PrintValue(1);\n"
      "}\n",
      &errors, "UseUtil.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(utilAst != nullptr);
  ASSERT_TRUE(useAst != nullptr);

  std::vector<ASTNode *> declarations;
  for (auto &decl : utilAst->declarations) {
    declarations.push_back(decl.get());
  }
  for (auto &decl : useAst->declarations) {
    declarations.push_back(decl.get());
  }

  auto *method = static_cast<MethodDeclNode *>(useAst->declarations[1].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  auto *call = static_cast<CallExprNode *>(body->statements[0].get());
  auto *callee = static_cast<IdentifierNode *>(call->callee.get());

  ProgramView programView;
  programView.location = {1, 1, "UseUtil.npp"};
  programView.moduleName = "UseUtil";
  programView.declarations = declarations;

  SemanticAnalyzer sema;
  sema.setAvailableModules(std::unordered_set<std::string>{"util"}, true);
  sema.analyze(programView);

  ASSERT_FALSE(sema.hasErrors());

  const auto methodSymbol = sema.getResolvedSymbol(callee->location);
  ASSERT_TRUE(methodSymbol.has_value());
  ASSERT_EQ(methodSymbol->kind, SymbolKind::Method);
  ASSERT_TRUE(methodSymbol->definition.has_value());
  ASSERT_EQ(methodSymbol->definition->location.file, "Util.npp");
  ASSERT_EQ(methodSymbol->definition->location.line, 1);
  ASSERT_EQ(methodSymbol->definition->location.column, 1);
  return true;
}

TEST(FrontendNavigationApisResolveDefinitionsReferencesAndDocumentSymbols) {
  const std::string source =
      "Box is class {\n"
      "  value is 1 as int;\n"
      "  GetValue is method() as int {\n"
      "    return value;\n"
      "  }\n"
      "}\n";
  const std::filesystem::path path =
      writeNavigationTestFile("npp_frontend_navigation_test.npp", source);

  const auto definition =
      frontend::getDefinition(path.string(), 4, 12);
  ASSERT_TRUE(definition.has_value());
  ASSERT_EQ(definition->file, path.string());
  ASSERT_EQ(definition->range.start.line, 2);
  ASSERT_EQ(definition->range.start.column, 3);

  const auto references = frontend::getReferences(path.string(), 2, 3);
  ASSERT_EQ(references.size(), 1u);
  ASSERT_EQ(references[0].range.start.line, 4);
  ASSERT_EQ(references[0].range.start.column, 12);

  const auto symbols = frontend::getDocumentSymbols(path.string());
  ASSERT_EQ(symbols.size(), 1u);
  ASSERT_EQ(symbols[0].name, "Box");
  ASSERT_EQ(symbols[0].children.size(), 2u);
  ASSERT_EQ(symbols[0].children[0].name, "value");
  ASSERT_EQ(symbols[0].children[1].name, "GetValue");
  return true;
}

TEST(SemanticScopeSnapshotTracksVisibleSymbolsByLocation) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method(input as int) {\n"
      "  value is 1 as int;\n"
      "  if(true) {\n"
      "    inner is 2 as int;\n"
      "  }\n"
      "  Print(value);\n"
      "}\n",
      &errors, "ScopeSnapshot.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());

  const auto beforeIf = sema.getScopeSnapshot({3, 3, "ScopeSnapshot.npp"});
  ASSERT_TRUE(std::any_of(beforeIf.begin(), beforeIf.end(),
                          [](const VisibleSymbolInfo &info) {
                            return info.name == "value";
                          }));
  ASSERT_TRUE(std::any_of(beforeIf.begin(), beforeIf.end(),
                          [](const VisibleSymbolInfo &info) {
                            return info.name == "input";
                          }));

  const auto afterIf = sema.getScopeSnapshot({6, 3, "ScopeSnapshot.npp"});
  ASSERT_FALSE(std::any_of(afterIf.begin(), afterIf.end(),
                           [](const VisibleSymbolInfo &info) {
                             return info.name == "inner";
                           }));
  return true;
}

TEST(SemanticTypeMemberLookupExposesClassMembers) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Box is class {\n"
      "  value is 1 as int;\n"
      "  GetValue is method() as int {\n"
      "    return value;\n"
      "  }\n"
      "}\n",
      &errors, "TypeMembers.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());

  const auto members = sema.getTypeMembers(NType::makeClass("Box"));
  ASSERT_TRUE(std::any_of(members.begin(), members.end(),
                          [](const VisibleSymbolInfo &info) {
                            return info.name == "value" &&
                                   info.kind == SymbolKind::Field;
                          }));
  ASSERT_TRUE(std::any_of(members.begin(), members.end(),
                          [](const VisibleSymbolInfo &info) {
                            return info.name == "GetValue" &&
                                   info.kind == SymbolKind::Method;
                          }));
  return true;
}

TEST(SemanticCallableSignaturesExposeParameterNamesAndTypes) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Sum is method(left as int, right as float) as double {\n"
      "  return left + right;\n"
      "}\n",
      &errors, "SignatureInfo.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());

  const auto signatures = sema.getCallableSignatures("Sum");
  ASSERT_EQ(signatures.size(), 1u);
  ASSERT_EQ(signatures[0].parameters.size(), 2u);
  ASSERT_EQ(signatures[0].parameters[0].name, "left");
  ASSERT_EQ(signatures[0].parameters[0].typeName, "int");
  ASSERT_EQ(signatures[0].parameters[1].name, "right");
  ASSERT_EQ(signatures[0].parameters[1].typeName, "float");
  ASSERT_EQ(signatures[0].returnType, "double");
  return true;
}

TEST(SemanticVariableNameEnforcesCSharpStyle) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  testObject is 1 as int;\n"
      "  _cacheValue is 2 as int;\n"
      "  return;\n"
      "}\n",
      &errors, "VarNameOk.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticVariableNameRejectsUppercaseAndMidUnderscore) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  TestObject is 1 as int;\n"
      "  test_Object is 2 as int;\n"
      "  return;\n"
      "}\n",
      &errors, "VarNameBad.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());
  return true;
}

TEST(SemanticRejectsSelfModuleImportWhenStrictNamingEnabled) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "module Box;\n"
      "Box is class {\n"
      "  value is 0 as int;\n"
      "}\n",
      &errors, "Box.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setStrictFileNamingRules(true, "Box");
  sema.setMaxClassesPerFile(1);
  sema.analyze(ast.get());

  ASSERT_TRUE(sema.hasErrors());
  bool sawSelfImportError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("cannot import itself") != std::string::npos) {
      sawSelfImportError = true;
      break;
    }
  }
  ASSERT_TRUE(sawSelfImportError);
  return true;
}

TEST(SemanticRejectsLowercaseOrUnderscoreFileStemWhenStrictNamingEnabled) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() { return; }\n",
      &errors, "bad_file.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setStrictFileNamingRules(true, "bad_file");
  sema.analyze(ast.get());

  ASSERT_TRUE(sema.hasErrors());
  bool sawLowercaseError = false;
  bool sawUnderscoreError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("must start with an uppercase letter") !=
        std::string::npos) {
      sawLowercaseError = true;
    }
    if (err.message.find("cannot include '_'") != std::string::npos) {
      sawUnderscoreError = true;
    }
  }
  ASSERT_TRUE(sawLowercaseError);
  ASSERT_TRUE(sawUnderscoreError);
  return true;
}

TEST(SemanticRejectsClassFilenameMismatchWhenStrictNamingEnabled) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Vec2 is class {\n"
      "  x is 0 as int;\n"
      "}\n",
      &errors, "Vector2.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setStrictFileNamingRules(true, "Vector2");
  sema.setMaxClassesPerFile(1);
  sema.analyze(ast.get());

  ASSERT_TRUE(sema.hasErrors());
  bool sawMismatchError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Class name must match module filename") !=
        std::string::npos) {
      sawMismatchError = true;
      break;
    }
  }
  ASSERT_TRUE(sawMismatchError);
  return true;
}

TEST(SemanticConstVariableRejectsMutation) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  const count is 2 as int;\n"
      "  count--;\n"
      "}\n",
      &errors, "ConstMutation.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawConstMutationError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Cannot mutate const variable") != std::string::npos) {
      sawConstMutationError = true;
      break;
    }
  }
  ASSERT_TRUE(sawConstMutationError);
  return true;
}

TEST(SemanticStaticAssertFalseProducesError) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  static_assert(false, \"must fail\");\n"
      "}\n",
      &errors, "StaticAssertFail.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawStaticAssertError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("static_assert failed") != std::string::npos) {
      sawStaticAssertError = true;
      break;
    }
  }
  ASSERT_TRUE(sawStaticAssertError);
  return true;
}

TEST(SemanticTypeofExpressionIsValid) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  typeName is typeof(1);\n"
      "  Print(typeName);\n"
      "}\n",
      &errors, "TypeofOk.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticAllowsExternMethodUsage) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "extern cSqrt method(x as float) as float;\n"
      "Init is method() {\n"
      "  y is cSqrt(9.0);\n"
      "}\n",
      &errors, "ExternUse.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticRejectsRemovedModuleCppDeclaration) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "modulecpp Tensorflow;\n"
      "Init is method() {\n"
      "  version is Tensorflow.Version() as string;\n"
      "}\n",
      &errors, "ModuleCppOk.npp");
  ASSERT_FALSE(errors.empty());
  ASSERT_TRUE(ast != nullptr);
  ASSERT_TRUE(errors[0].find("modulecpp") != std::string::npos);
  return true;
}

TEST(SemanticRejectsRemovedModuleCppDeclarationWithoutFallback) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "modulecpp Tensorflow;\n"
      "Init is method() { return; }\n",
      &errors, "ModuleCppMissing.npp");
  ASSERT_FALSE(errors.empty());
  ASSERT_TRUE(ast != nullptr);
  ASSERT_TRUE(errors[0].find("modulecpp") != std::string::npos);
  return true;
}

TEST(SemanticAcceptsDynamicDeclarationsAndShorthandBindings) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init method() {\n"
      "  a;\n"
      "  b 10;\n"
      "  c as dynamic;\n"
      "  d is 0 as dynamic;\n"
      "}\n",
      &errors, "DynamicShort.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticTreatsRebindingAsAssignmentUpdate) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init method() {\n"
      "  a \"wow\" as dynamic;\n"
      "  a 10;\n"
      "  Print(a);\n"
      "}\n",
      &errors, "RebindDynamic.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticAcceptsEnumInterfaceStructAndDictionaryType) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Color enum { Red, Green, Blue };\n"
      "Renderable interface {\n"
      "  Draw method() as void;\n"
      "}\n"
      "Point struct {\n"
      "  values is 0 as int;\n"
      "}\n"
      "Init method() {\n"
      "  palette as Dictionary<string, int>;\n"
      "}\n",
      &errors, "CSharpDecls.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticAcceptsMatchAndCastPipeline) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Color enum { Red, Yellow, Green };\n"
      "Init method() {\n"
      "  color is Color.Red as Color;\n"
      "  value is 10 as int;\n"
      "  value as maybe string then float;\n"
      "  match (color) {\n"
      "    Color.Red then {\n"
      "      Print(\"Red\");\n"
      "    }\n"
      "    default then {\n"
      "      Print(\"Other\");\n"
      "    }\n"
      "  }\n"
      "}\n",
      &errors, "MatchCastOk.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticAcceptsMultiBindingDeclarationAndUpdate) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init method() {\n"
      "  x is 1 as int;\n"
      "  y is 2 as int;\n"
      "  x, y is 0;\n"
      "}\n",
      &errors, "MultiBindOk.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticAcceptsMultiSelectorMatchExpression) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Color enum { Red, Yellow, Green };\n"
      "Init method() {\n"
      "  firstColor is Color.Red as Color;\n"
      "  secondColor is Color.Yellow as Color;\n"
      "  result is match(firstColor, secondColor) {\n"
      "    Color.Red, Color.Yellow then Color.Green;\n"
      "    default then Color.Red;\n"
      "  };\n"
      "  Print(result);\n"
      "}\n",
      &errors, "MultiMatchExprOk.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticAcceptsImplicitSingleStatementBodies) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Color enum { Red };\n"
      "Echo method(value as string)\n"
      "  Print(value);\n"
      "Init method(items as Array<int>) {\n"
      "  color is Color.Red as Color;\n"
      "  if(true) Print(\"if\"); else Print(\"else\");\n"
      "  while(false) Print(\"while\");\n"
      "  for(i is 0; i < 1; i++) Print(i);\n"
      "  for(item in items) Print(item);\n"
      "  match(color) {\n"
      "    Color.Red then Print(\"match\");\n"
      "  }\n"
      "  try Print(\"try\"); catch(error) Print(error); finally Print(\"finally\");\n"
      "}\n",
      &errors, "ImplicitBodiesOk.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticRejectsImpossibleMandatoryCast) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init method() {\n"
      "  flag is true as bool;\n"
      "  flag as Tensor<float>;\n"
      "}\n",
      &errors, "BadCast.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawInvalidCast = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Invalid cast step") != std::string::npos) {
      sawInvalidCast = true;
      break;
    }
  }
  ASSERT_TRUE(sawInvalidCast);
  return true;
}

TEST(SemanticAcceptsGpuBlockInsideMethod) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init method() {\n"
      "  gpu {\n"
      "    value is 1 as int;\n"
      "  }\n"
      "}\n",
      &errors, "GpuBlockOk.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticNamedArgumentsBindByParameterName) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Sum is method(first as int, second as int) as int {\n"
      "  return first + second;\n"
      "}\n"
      "Init is method() {\n"
      "  result is Sum(second: 2, first: 1);\n"
      "  Print(result);\n"
      "}\n",
      &errors, "NamedArgsOk.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticNamedArgumentsRejectUnknownParameterName) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Sum is method(first as int, second as int) as int {\n"
      "  return first + second;\n"
      "}\n"
      "Init is method() {\n"
      "  result is Sum(third: 2, first: 1);\n"
      "  Print(result);\n"
      "}\n",
      &errors, "NamedArgsBad.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawUnknownNamedArg = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Unknown named argument 'third'") !=
        std::string::npos) {
      sawUnknownNamedArg = true;
      break;
    }
  }
  ASSERT_TRUE(sawUnknownNamedArg);
  return true;
}

TEST(SemanticAllowsFusionChainSyntax) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
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
      &errors, "FusionChainOk.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticAllowsRegisteredBuiltinFusionChainSyntax) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method(input as Tensor<float>, kernel as Tensor<float>,\n"
      "               bias as Tensor<float>, gamma as Tensor<float>,\n"
      "               beta as Tensor<float>, mean as Tensor<float>,\n"
      "               variance as Tensor<float>) {\n"
      "  result is NN.Conv2D-BatchNorm-ReLU(\n"
      "      input, kernel, bias, gamma, beta, mean, variance,\n"
      "      0.001, 1, 1, 0, 0);\n"
      "}\n",
      &errors, "BuiltinFusionChainOk.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticInputGenericFluentChainAcceptsValidUsage) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  age is Input<int>(\"Age: \").Min(18).Max(99).Default(21).TimeoutMs(5000);\n"
      "  password is Input<string>(\"Password: \").Secret().Default(\"guest\");\n"
      "}\n",
      &errors, "InputFluentOk.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticInputGenericAcceptsEnumUsage) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Color is enum { Red, Green, Blue };\n"
      "Init is method() {\n"
      "  choice is Input<Color>(\"Color: \").Default(Color.Green).TimeoutMs(4000);\n"
      "}\n",
      &errors, "InputEnumOk.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticInputDefaultsMissingGenericToString) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  value is Input(\"Enter: \").Secret().Default(\"guest\");\n"
      "}\n",
      &errors, "InputDefaultStringOk.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticInputGenericRejectsInvalidFluentMethodForType) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  name is Input<string>(\"Name: \").Min(2);\n"
      "}\n",
      &errors, "InputFluentBadType.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawTypeSpecificError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Input<T>.Min() is only valid for numeric T") !=
        std::string::npos) {
      sawTypeSpecificError = true;
      break;
    }
  }
  ASSERT_TRUE(sawTypeSpecificError);
  return true;
}

TEST(SemanticInputGenericRejectsSecretForEnumType) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Color is enum { Red, Green, Blue };\n"
      "Init is method() {\n"
      "  choice is Input<Color>(\"Color: \").Secret();\n"
      "}\n",
      &errors, "InputEnumSecretBad.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawSecretTypeError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Input<T>.Secret() is only valid for T = string") !=
        std::string::npos) {
      sawSecretTypeError = true;
      break;
    }
  }
  ASSERT_TRUE(sawSecretTypeError);
  return true;
}

TEST(SemanticInputGenericRejectsInvalidGenericArity) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  age is Input<int, float>(\"Age: \").Default(\"oops\");\n"
      "}\n",
      &errors, "InputFluentBadGeneric.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawGenericArityError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Input<T> requires exactly one generic type argument") !=
        std::string::npos) {
      sawGenericArityError = true;
      break;
    }
  }
  ASSERT_TRUE(sawGenericArityError);
  return true;
}

TEST(SemanticInputGenericRejectsInvalidDefaultType) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  age is Input<int>(\"Age: \").Default(\"oops\");\n"
      "}\n",
      &errors, "InputFluentBadDefault.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawDefaultTypeError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Input<T>.Default() argument type mismatch") !=
        std::string::npos) {
      sawDefaultTypeError = true;
      break;
    }
  }
  ASSERT_TRUE(sawDefaultTypeError);
  return true;
}

TEST(SemanticMethodNameMinLengthRuleRejectsShortNames) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Ab is method() {\n"
      "  return;\n"
      "}\n",
      &errors, "ShortMethod.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setRequireMethodUppercaseStart(false);
  sema.setMinMethodNameLength(4);
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());
  return true;
}

TEST(SemanticClassVisibilityRuleRequiresExplicitModifier) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Thing is class {\n"
      "}\n",
      &errors, "ClassVisibility.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setRequireClassExplicitVisibility(true);
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());
  return true;
}

TEST(SemanticPropertyVisibilityRuleRequiresExplicitModifier) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Box is public class {\n"
      "  value is 1 as int;\n"
      "  GetValue is method() as int {\n"
      "    return value;\n"
      "  }\n"
      "}\n",
      &errors, "MemberVisibility.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setRequirePropertyExplicitVisibility(true);
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());
  return true;
}

TEST(SemanticConstUppercaseRuleAllowsUpperSnakeCase) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  const MAX_BUFFER_SIZE is 4 as int;\n"
      "  return;\n"
      "}\n",
      &errors, "ConstUpperOk.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setRequireConstUppercase(true);
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticConstUppercaseRuleRejectsLowercaseConst) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  const maxBufferSize is 4 as int;\n"
      "  return;\n"
      "}\n",
      &errors, "ConstUpperBad.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setRequireConstUppercase(true);
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());
  return true;
}

TEST(SemanticMaxLinesPerMethodRuleRejectsLongMethod) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  a is 1 as int;\n"
      "  b is 2 as int;\n"
      "  c is 3 as int;\n"
      "  d is 4 as int;\n"
      "}\n",
      &errors, "MethodLength.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setMaxLinesPerMethod(3);
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());
  return true;
}

TEST(SemanticMaxLinesPerBlockRuleRejectsLongBlock) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  if (true) {\n"
      "    a is 1 as int;\n"
      "    b is 2 as int;\n"
      "  }\n"
      "}\n",
      &errors, "BlockLength.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setMaxLinesPerBlockStatement(3);
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());
  return true;
}

TEST(SemanticMaxNestingDepthRuleRejectsDeepControlFlow) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  if (true) {\n"
      "    if (true) {\n"
      "      if (true) {\n"
      "        if (true) {\n"
      "          Print(\"deep\");\n"
      "        }\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "}\n",
      &errors, "NestingDepth.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setMaxNestingDepth(3);
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());
  return true;
}

TEST(SemanticPublicMethodDocsRuleRequiresSummaryComments) {
  const std::string source =
      "Ping is public method() {\n"
      "  return;\n"
      "}\n";
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(source, &errors, "PublicDocsBad.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setRequirePublicMethodDocs(true);
  sema.setSourceText(source);
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());
  return true;
}

TEST(SemanticPublicMethodDocsRuleAcceptsSummaryComments) {
  const std::string source =
      "/// <summary>\n"
      "/// Responds with success.\n"
      "/// </summary>\n"
      "Ping is public method() {\n"
      "  return;\n"
      "}\n";
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(source, &errors, "PublicDocsOk.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setRequirePublicMethodDocs(true);
  sema.setSourceText(source);
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticCanvasRejectsDuplicateInlineEventHandlers) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init method() {\n"
      "  window is 0 as dynamic;\n"
      "  canvas(window) {\n"
      "    OnFrame { Print(\"a\"); }\n"
      "    OnFrame { Print(\"b\"); }\n"
      "  }\n"
      "}\n",
      &errors, "CanvasDuplicateInline.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawDuplicate = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Duplicate inline canvas event handler") !=
        std::string::npos) {
      sawDuplicate = true;
      break;
    }
  }
  ASSERT_TRUE(sawDuplicate);
  return true;
}

TEST(SemanticCanvasRejectsParameterizedInlineEventHandler) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init method() {\n"
      "  window is 0 as dynamic;\n"
      "  canvas(window) {\n"
      "    OnFrame is method(dt as float) {\n"
      "      Print(dt);\n"
      "    }\n"
      "  }\n"
      "}\n",
      &errors, "CanvasInlineParams.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawParameterlessError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Canvas event methods must be parameterless") !=
        std::string::npos) {
      sawParameterlessError = true;
      break;
    }
  }
  ASSERT_TRUE(sawParameterlessError);
  return true;
}

TEST(SemanticCanvasRejectsExternalHandlerWithNonVoidReturn) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "FrameHandler method() as int {\n"
      "  return 1;\n"
      "}\n"
      "Init method() {\n"
      "  window is 0 as dynamic;\n"
      "  canvas(window, onFrame: FrameHandler) {\n"
      "    OnClose { Print(\"done\"); }\n"
      "  }\n"
      "}\n",
      &errors, "CanvasExternalReturn.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawReturnError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Canvas event methods must return void") !=
        std::string::npos) {
      sawReturnError = true;
      break;
    }
  }
  ASSERT_TRUE(sawReturnError);
  return true;
}

TEST(SemanticCanvasAllowsInlineAndExternalForSameEvent) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "FrameHandler method() {\n"
      "  Print(\"external\");\n"
      "}\n"
      "Init method() {\n"
      "  window is 0 as dynamic;\n"
      "  canvas(window, onFrame: FrameHandler) {\n"
      "    OnFrame { Print(\"inline\"); }\n"
      "  }\n"
      "}\n",
      &errors, "CanvasInlinePrecedence.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticRejectsUninitializedVariableRead) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() as int {\n"
      "  value as int;\n"
      "  return value;\n"
      "}\n",
      &errors, "UninitializedRead.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawUninitialized = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("used before it is initialized") !=
        std::string::npos) {
      sawUninitialized = true;
      break;
    }
  }
  ASSERT_TRUE(sawUninitialized);
  return true;
}

TEST(SemanticRejectsPossiblyUninitializedVariableAfterBranch) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method(flag as bool) as int {\n"
      "  value as int;\n"
      "  if(flag) {\n"
      "    value 1;\n"
      "  }\n"
      "  return value;\n"
      "}\n",
      &errors, "MaybeUninitialized.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawMaybeUninitialized = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("may be uninitialized when used") !=
        std::string::npos) {
      sawMaybeUninitialized = true;
      break;
    }
  }
  ASSERT_TRUE(sawMaybeUninitialized);
  return true;
}

TEST(SemanticRejectsNullUnsafeUse) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  value is null;\n"
      "  Print(value.Length);\n"
      "}\n",
      &errors, "NullUnsafeUse.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawNullUse = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Null value used in member access") !=
        std::string::npos) {
      sawNullUse = true;
      break;
    }
  }
  ASSERT_TRUE(sawNullUse);
  return true;
}

TEST(SemanticRejectsPossiblyNullUseAfterBranch) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method(flag as bool) {\n"
      "  value is \"ok\" as dynamic;\n"
      "  if(flag) {\n"
      "    value null;\n"
      "  }\n"
      "  Print(value.Length);\n"
      "}\n",
      &errors, "MaybeNullUse.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawMaybeNull = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("may be null when used in member access") !=
        std::string::npos) {
      sawMaybeNull = true;
      break;
    }
  }
  ASSERT_TRUE(sawMaybeNull);
  return true;
}

TEST(SemanticRejectsUnreachableCodeAfterReturn) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  return;\n"
      "  Print(\"dead\");\n"
      "}\n",
      &errors, "UnreachableAfterReturn.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawUnreachable = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Unreachable code") != std::string::npos) {
      sawUnreachable = true;
      break;
    }
  }
  ASSERT_TRUE(sawUnreachable);
  return true;
}

TEST(SemanticShaderRequiresFragmentStage) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Effect is shader {\n"
      "  Vertex method(position as Vector3) {\n"
      "    return position;\n"
      "  }\n"
      "}\n",
      &errors, "ShaderMissingFragment.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawMissingFragment = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Shader must define a Fragment stage") !=
        std::string::npos) {
      sawMissingFragment = true;
      break;
    }
  }
  ASSERT_TRUE(sawMissingFragment);
  return true;
}

TEST(SemanticShaderRejectsPassMismatchWithFragmentParams) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Effect is shader {\n"
      "  Vertex method(position as Vector3, uv as Vector2) {\n"
      "    pass uv;\n"
      "    return MVP * position;\n"
      "  }\n"
      "  Fragment method(normal as Vector3) {\n"
      "    return normal;\n"
      "  }\n"
      "}\n",
      &errors, "ShaderPassMismatch.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawPassMismatch = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Fragment stage is missing parameter for passed varying 'uv'") !=
        std::string::npos) {
      sawPassMismatch = true;
      break;
    }
  }
  ASSERT_TRUE(sawPassMismatch);
  return true;
}

TEST(SemanticCanvasOnFrameAcceptsTypedGraphicsV2Commands) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "BasicLit is shader {\n"
      "  tint as Color;\n"
      "  Vertex method(position as Vector3) {\n"
      "    return position;\n"
      "  }\n"
      "  Fragment method() {\n"
      "    return tint;\n"
      "  }\n"
      "}\n"
      "Init method() {\n"
      "  mainWindow is Window.Create(1280, 720, \"App\");\n"
      "  mesh is Mesh.Load(\"examples/assets/triangle.obj\");\n"
      "  material is Material<BasicLit>();\n"
      "  material.tint is Color(1.0, 0.2, 0.1, 1.0);\n"
      "  canvas(mainWindow) {\n"
      "    OnFrame {\n"
      "      cmd.Clear(Color(0.08, 0.08, 0.10, 1.0));\n"
      "      cmd.DrawIndexed(mesh, material);\n"
      "      cmd.DrawInstanced(mesh, material, 1);\n"
      "    }\n"
      "  }\n"
      "}\n",
      &errors, "GraphicsV2TypedCanvas.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticRejectsInvalidMaterialBindingName) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "BasicLit is shader {\n"
      "  tint as Color;\n"
      "  Vertex method(position as Vector3) {\n"
      "    return position;\n"
      "  }\n"
      "  Fragment method() {\n"
      "    return tint;\n"
      "  }\n"
      "}\n"
      "Init method() {\n"
      "  material is Material<BasicLit>();\n"
      "  material.albedo is Color(1.0, 1.0, 1.0, 1.0);\n"
      "}\n",
      &errors, "GraphicsMaterialBindingMismatch.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawBindingError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("N2012") !=
        std::string::npos) {
      sawBindingError = true;
      break;
    }
  }
  ASSERT_TRUE(sawBindingError);
  return true;
}

TEST(SemanticAcceptsSamplerBindingsOnShadersAndMaterials) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
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
      "  texture is Texture2D.Load(\"examples/assets/checker.png\");\n"
      "  sampler is Sampler.Create();\n"
      "  material is Material<Textured>();\n"
      "  material.albedo is texture;\n"
      "  material.linearSampler is sampler;\n"
      "}\n",
      &errors, "GraphicsSamplerBindings.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticAcceptsTextureSamplingInFragmentStage) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Textured is shader {\n"
      "  tint as Color;\n"
      "  albedo as Texture2D;\n"
      "  linearSampler as Sampler;\n"
      "  Vertex method(position as Vector3, uv as Vector2) {\n"
      "    pass uv;\n"
      "    return MVP * position;\n"
      "  }\n"
      "  Fragment method(uv as Vector2) {\n"
      "    return albedo.Sample(linearSampler, uv) * tint;\n"
      "  }\n"
      "}\n",
      &errors, "GraphicsTextureSampling.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticRejectsTextureSamplingOutsideFragmentStage) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Textured is shader {\n"
      "  albedo as Texture2D;\n"
      "  linearSampler as Sampler;\n"
      "  Vertex method(position as Vector3, uv as Vector2) {\n"
      "    return albedo.Sample(linearSampler, uv);\n"
      "  }\n"
      "  Fragment method() {\n"
      "    return Color(1.0, 1.0, 1.0, 1.0);\n"
      "  }\n"
      "}\n",
      &errors, "GraphicsTextureSampleInVertex.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawSampleError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Texture2D.Sample(...) is only allowed in Fragment stage") !=
        std::string::npos) {
      sawSampleError = true;
      break;
    }
  }
  ASSERT_TRUE(sawSampleError);
  return true;
}

TEST(SemanticAcceptsScene2DFlowInsideOnFrame) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init method() {\n"
      "  window is Window.Create(1280, 720, \"Scene2D\");\n"
      "  scene is Scene.Create();\n"
      "  cameraEntity is scene.CreateEntity(\"Camera\");\n"
      "  camera is cameraEntity.AddCamera2D();\n"
      "  camera.SetPrimary(true);\n"
      "  camera.SetZoom(2.0);\n"
      "  spriteEntity is scene.CreateEntity(\"Sprite\");\n"
      "  transform is spriteEntity.GetTransform();\n"
      "  transform.SetPosition(Vector3(1.0, 2.0, 0.0));\n"
      "  transform.SetScale(Vector3(2.0, 2.0, 1.0));\n"
      "  sprite is spriteEntity.AddSpriteRenderer2D();\n"
      "  sprite.SetSize(Vector2(2.0, 2.0));\n"
      "  sprite.SetPivot(Vector2(0.5, 0.5));\n"
      "  sprite.SetFlipX(false);\n"
      "  sprite.SetSortingLayer(1);\n"
      "  shape is scene.CreateEntity(\"Shape\").AddShapeRenderer2D();\n"
      "  shape.SetRectangle(Vector2(4.0, 3.0));\n"
      "  shape.SetFilled(true);\n"
      "  renderer is Renderer2D.Create();\n"
      "  renderer.SetClearColor(Color(0.1, 0.1, 0.1, 1.0));\n"
      "  renderer.SetCamera(camera);\n"
      "  canvas(window) {\n"
      "    OnFrame {\n"
      "      renderer.Render(scene);\n"
      "    }\n"
      "  }\n"
      "}\n",
      &errors, "Scene2DFlow.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticRejectsRenderer2DRenderOutsideFrame) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init method() {\n"
      "  scene is Scene.Create();\n"
      "  renderer is Renderer2D.Create();\n"
      "  renderer.Render(scene);\n"
      "}\n",
      &errors, "Scene2DRenderOutsideFrame.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawRenderError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Renderer2D.Render") != std::string::npos &&
        err.message.find("OnFrame") != std::string::npos) {
      sawRenderError = true;
      break;
    }
  }
  ASSERT_TRUE(sawRenderError);
  return true;
}

TEST(SemanticRejectsDuplicateBuiltInScene2DComponents) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init method() {\n"
      "  scene is Scene.Create();\n"
      "  entity is scene.CreateEntity(\"Player\");\n"
      "  entity.AddSpriteRenderer2D();\n"
      "  entity.AddSpriteRenderer2D();\n"
      "}\n",
      &errors, "Scene2DDuplicateComponent.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawDuplicateError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("AddSpriteRenderer2D") != std::string::npos &&
        err.message.find("already has") != std::string::npos) {
      sawDuplicateError = true;
      break;
    }
  }
  ASSERT_TRUE(sawDuplicateError);
  return true;
}

TEST(SemanticRejectsTransformParentCycles) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init method() {\n"
      "  scene is Scene.Create();\n"
      "  parentEntity is scene.CreateEntity(\"Parent\");\n"
      "  childEntity is scene.CreateEntity(\"Child\");\n"
      "  parentTransform is parentEntity.GetTransform();\n"
      "  childTransform is childEntity.GetTransform();\n"
      "  childTransform.SetParent(parentEntity);\n"
      "  parentTransform.SetParent(childEntity);\n"
      "}\n",
      &errors, "Scene2DParentCycle.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawCycleError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("cycle") != std::string::npos &&
        err.message.find("SetParent") != std::string::npos) {
      sawCycleError = true;
      break;
    }
  }
  ASSERT_TRUE(sawCycleError);
  return true;
}

TEST(SemanticRejectsSamplerBindingTypeMismatch) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Textured is shader {\n"
      "  linearSampler as Sampler;\n"
      "  Vertex method(position as Vector3) {\n"
      "    return position;\n"
      "  }\n"
      "  Fragment method() {\n"
      "    return 0;\n"
      "  }\n"
      "}\n"
      "Init method() {\n"
      "  material is Material<Textured>();\n"
      "  material.linearSampler is Color(1.0, 1.0, 1.0, 1.0);\n"
      "}\n",
      &errors, "GraphicsSamplerBindingMismatch.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawBindingError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Type mismatch: cannot assign 'Color' to 'Sampler'") !=
        std::string::npos) {
      sawBindingError = true;
      break;
    }
  }
  ASSERT_TRUE(sawBindingError);
  return true;
}

TEST(SemanticRejectsMatrixBindingTypeMismatch) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Effect is shader {\n"
      "  transform as Matrix4;\n"
      "  Vertex method(position as Vector3) {\n"
      "    return MVP * position;\n"
      "  }\n"
      "  Fragment method() {\n"
      "    return Color(1.0, 1.0, 1.0, 1.0);\n"
      "  }\n"
      "}\n"
      "Init method() {\n"
      "  material is Material<Effect>();\n"
      "  material.transform is Color(1.0, 1.0, 1.0, 1.0);\n"
      "}\n",
      &errors, "GraphicsMatrixBindingMismatch.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawBindingError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Type mismatch: cannot assign 'Color' to 'Matrix4'") !=
        std::string::npos) {
      sawBindingError = true;
      break;
    }
  }
  ASSERT_TRUE(sawBindingError);
  return true;
}

TEST(SemanticRejectsDirectShaderInstantiationN2011) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Effect is shader {\n"
      "  Vertex method(position as Vector3) {\n"
      "    return position;\n"
      "  }\n"
      "  Fragment method() {\n"
      "    return Color(1.0, 1.0, 1.0, 1.0);\n"
      "  }\n"
      "}\n"
      "Init method() {\n"
      "  bad is Effect();\n"
      "}\n",
      &errors, "ShaderDirectInstantiation.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawN2011 = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("N2011") != std::string::npos) {
      sawN2011 = true;
      break;
    }
  }
  ASSERT_TRUE(sawN2011);
  return true;
}

TEST(SemanticAcceptsShaderDescriptorStaticMethodCall) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Effect is shader {\n"
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
      "}\n"
      "Init method() {\n"
      "  tintColor is Effect.DefaultTint();\n"
      "}\n",
      &errors, "ShaderStaticDescriptorMethod.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticRejectsPassInCpuSideDescriptorMethod) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Effect is shader {\n"
      "  Helper method() as int {\n"
      "    pass uv;\n"
      "    return 0;\n"
      "  }\n"
      "  Vertex method(position as Vector3, uv as Vector2) {\n"
      "    pass uv;\n"
      "    return position;\n"
      "  }\n"
      "  Fragment method(uv as Vector2) {\n"
      "    return Color(1.0, 1.0, 1.0, 1.0);\n"
      "  }\n"
      "}\n",
      &errors, "ShaderCpuMethodPassInvalid.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawPassError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("cannot use 'pass'") != std::string::npos) {
      sawPassError = true;
      break;
    }
  }
  ASSERT_TRUE(sawPassError);
  return true;
}

TEST(SemanticRejectsMvpInCpuSideDescriptorMethod) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Effect is shader {\n"
      "  Helper method() as Matrix4 {\n"
      "    return MVP;\n"
      "  }\n"
      "  Vertex method(position as Vector3) {\n"
      "    return position;\n"
      "  }\n"
      "  Fragment method() {\n"
      "    return Color(1.0, 1.0, 1.0, 1.0);\n"
      "  }\n"
      "}\n",
      &errors, "ShaderCpuMethodMvpInvalid.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawMvpError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("cannot access MVP") != std::string::npos) {
      sawMvpError = true;
      break;
    }
  }
  ASSERT_TRUE(sawMvpError);
  return true;
}

TEST(SemanticRejectsLegacyGraphicsDrawApi) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "module Graphics;\n"
      "Init method() {\n"
      "  Graphics.Draw(null, null);\n"
      "}\n",
      &errors, "LegacyGraphicsDraw.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.setAvailableModules(std::unordered_set<std::string>{"graphics"}, true);
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawLegacyError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("Legacy Graphics API 'Graphics.Draw' was removed") !=
        std::string::npos) {
      sawLegacyError = true;
      break;
    }
  }
  ASSERT_TRUE(sawLegacyError);
  return true;
}

TEST(SemanticModuleMemberLookupResolvesQualifiedMethodAccess) {
  std::vector<std::string> errors;
  auto mathAst = parseForSemanticTests(
      "Sqrt is method(value as int) as int {\n"
      "  return value;\n"
      "}\n",
      &errors, "Math.npp");
  auto useAst = parseForSemanticTests(
      "module Math;\n"
      "Init is method() as int {\n"
      "  return Math.Sqrt(9);\n"
      "}\n",
      &errors, "UseMath.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(mathAst != nullptr);
  ASSERT_TRUE(useAst != nullptr);

  std::vector<ASTNode *> declarations;
  for (auto &decl : mathAst->declarations) {
    declarations.push_back(decl.get());
  }
  for (auto &decl : useAst->declarations) {
    declarations.push_back(decl.get());
  }

  auto *method = static_cast<MethodDeclNode *>(useAst->declarations[1].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  auto *ret = static_cast<ReturnStmtNode *>(body->statements[0].get());
  auto *call = static_cast<CallExprNode *>(ret->value.get());
  auto *callee = static_cast<MemberAccessNode *>(call->callee.get());

  ProgramView programView;
  programView.location = {1, 1, "UseMath.npp"};
  programView.moduleName = "UseMath";
  programView.declarations = declarations;

  SemanticAnalyzer sema;
  sema.setAvailableModules(std::unordered_set<std::string>{"math"}, true);
  sema.analyze(programView);

  ASSERT_FALSE(sema.hasErrors());
  const auto symbol = sema.getResolvedSymbol(callee->memberLocation);
  ASSERT_TRUE(symbol.has_value());
  ASSERT_EQ(symbol->kind, SymbolKind::Method);
  return true;
}

TEST(SemanticExpandModuleInjectsMembersIntoGlobalScope) {
  std::vector<std::string> errors;
  auto coreAst = parseForSemanticTests(
      "DoubleIt is method(value as int) as int {\n"
      "  return value + value;\n"
      "}\n",
      &errors, "MathCore.npp");
  auto useAst = parseForSemanticTests(
      "expand module MathCore;\n"
      "Init is method() as int {\n"
      "  return DoubleIt(4);\n"
      "}\n",
      &errors, "UseExpand.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(coreAst != nullptr);
  ASSERT_TRUE(useAst != nullptr);

  std::vector<ASTNode *> declarations;
  for (auto &decl : coreAst->declarations) {
    declarations.push_back(decl.get());
  }
  for (auto &decl : useAst->declarations) {
    declarations.push_back(decl.get());
  }

  auto *method = static_cast<MethodDeclNode *>(useAst->declarations[1].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  auto *ret = static_cast<ReturnStmtNode *>(body->statements[0].get());
  auto *call = static_cast<CallExprNode *>(ret->value.get());
  auto *callee = static_cast<IdentifierNode *>(call->callee.get());

  ProgramView programView;
  programView.location = {1, 1, "UseExpand.npp"};
  programView.moduleName = "UseExpand";
  programView.declarations = declarations;

  SemanticAnalyzer sema;
  sema.setAvailableModules(std::unordered_set<std::string>{"mathcore"}, true);
  sema.analyze(programView);

  ASSERT_FALSE(sema.hasErrors());
  const auto symbol = sema.getResolvedSymbol(callee->location);
  ASSERT_TRUE(symbol.has_value());
  ASSERT_EQ(symbol->kind, SymbolKind::Method);
  return true;
}

TEST(SemanticCaretPowInfersFloatType) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  value is 2 ^ 3;\n"
      "}\n",
      &errors, "CaretPow.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  auto *method = static_cast<MethodDeclNode *>(ast->declarations[0].get());
  auto *body = static_cast<BlockNode *>(method->body.get());
  auto *binding = static_cast<BindingDeclNode *>(body->statements[0].get());

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_FALSE(sema.hasErrors());
  ASSERT_TRUE(sema.getInferredType(binding->value.get()) != nullptr);
  ASSERT_EQ(sema.getInferredType(binding->value.get())->toString(), "float");
  return true;
}

TEST(SemanticCaretCaretRejectsNonIntegerRhs) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "Init is method() {\n"
      "  value is 9 ^^ 2.5;\n"
      "}\n",
      &errors, "CaretRootBad.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  ASSERT_TRUE(sema.hasErrors());

  bool sawError = false;
  for (const auto &err : sema.getErrors()) {
    if (err.message.find("^^ right-hand side must be a positive integer") !=
        std::string::npos) {
      sawError = true;
      break;
    }
  }
  ASSERT_TRUE(sawError);
  return true;
}

TEST(SemanticExternDeclarationAnalyzesWithoutBody) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "extern FileExists method(path as string) as bool;\n",
      &errors, "ExternContract.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());

  ASSERT_FALSE(sema.hasErrors());
  return true;
}

TEST(SemanticInlineSymbolExternDeclarationAnalyzesWithoutBody) {
  std::vector<std::string> errors;
  auto ast = parseForSemanticTests(
      "extern(\"fs_file_exists\") FileExists method(path as string) as bool;\n",
      &errors, "ExternInlineContract.npp");
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(ast != nullptr);

  SemanticAnalyzer sema;
  sema.analyze(ast.get());

  ASSERT_FALSE(sema.hasErrors());
  return true;
}
