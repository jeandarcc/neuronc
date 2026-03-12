#pragma once

#include "neuronc/sema/SemanticAnalyzer.h"

namespace neuron::sema_detail {

class SemanticDriver;

class DeclarationAnalyzer {
public:
  explicit DeclarationAnalyzer(SemanticDriver &driver);

  void analyze(const ProgramView &program);
  void visitProgram(ProgramNode *node);
  void visitDeclarations(const std::vector<ASTNode *> &declarations);
  void visitModuleDecl(ModuleDeclNode *node);
  void visitExpandModuleDecl(ExpandModuleDeclNode *node);
  void visitModuleCppDecl(ModuleCppDeclNode *node);
  void visitClassDecl(ClassDeclNode *node);
  void visitEnumDecl(EnumDeclNode *node);
  void visitMethodDecl(MethodDeclNode *node);

private:
  void registerGlobalDeclarations(const std::vector<ASTNode *> &declarations);

  SemanticDriver &m_driver;
};

} // namespace neuron::sema_detail
