#pragma once

#include "MIRNode.h"
#include "neuronc/parser/AST.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace neuron::mir {

class MIRBuilder {
public:
  MIRBuilder() = default;

  std::unique_ptr<Module> build(ASTNode *root, const std::string &moduleName);
  const std::vector<std::string> &errors() const { return m_errors; }
  bool hasErrors() const { return !m_errors.empty(); }

private:
  struct LoopTargets {
    std::size_t continueBlock = 0;
    std::size_t breakBlock = 0;
  };

  std::unique_ptr<Module> m_module;
  std::vector<std::string> m_errors;
  std::vector<std::unordered_map<std::string, std::string>> m_scopes;
  std::vector<LoopTargets> m_loopStack;
  std::unordered_map<std::string, int> m_nameVersions;
  std::size_t m_currentFunction = 0;
  std::size_t m_currentBlock = 0;
  int m_tempCounter = 0;
  int m_blockCounter = 0;
  int m_scopeDepth = -1;

  Function &function();
  BasicBlock &block();
  std::size_t createBlock(const std::string &hint);
  void switchTo(std::size_t blockIndex);
  void pushScope();
  void popScope();
  bool isDeclared(const std::string &name) const;
  bool isDeclaredInCurrentScope(const std::string &name) const;
  std::string resolveName(const std::string &name) const;
  void registerLocal(const std::string &lowered, const std::string &sourceName,
                     const SourceLocation &location, bool isParameter);
  std::string declare(const std::string &name, const SourceLocation &location,
                      bool isParameter = false);
  Operand nextTemp();
  Operand constantTemp(const SourceLocation &location, std::string value);
  Operand copyTemp(const SourceLocation &location, const std::string &name);
  Instruction &emit(Instruction instruction);
  void emitJump(const SourceLocation &location, std::size_t target);
  void emitBranch(const SourceLocation &location, const Operand &condition,
                  std::size_t trueTarget, std::size_t falseTarget);
  void emitReturn(const SourceLocation &location, const Operand &value);
  void addSuccessor(std::size_t from, std::size_t to);
  std::string typeText(const ASTNode *node) const;
  std::string describeNode(const ASTNode *node) const;
  void unsupportedStmt(const ASTNode *node, std::string reason);
  Operand unsupportedExpr(const ASTNode *node, std::string reason);
  void buildProgram(ProgramNode *program);
  void buildMethod(MethodDeclNode *method, const std::string &qualifiedName);
  void lowerStatement(ASTNode *node);
  void lowerBlock(ASTNode *node, bool scoped);
  void lowerIf(IfStmtNode *node);
  void lowerWhile(WhileStmtNode *node);
  void lowerFor(ForStmtNode *node);
  void lowerForIn(ForInStmtNode *node);
  void lowerMatchStatement(MatchStmtNode *node);
  Operand lowerMatchExpression(MatchExprNode *node);
  Operand lowerExpression(ASTNode *node);
  Operand lowerCondition(ASTNode *node);
  Operand lowerMatchCondition(const std::vector<Operand> &selectors,
                              MatchArmNode *arm);
  Operand lowerBindingValue(BindingDeclNode *node);
  void lowerBinding(BindingDeclNode *node);
  void lowerAssignmentTarget(ASTNode *target, const Operand &value,
                             const SourceLocation &location,
                             const std::string &fallbackName,
                             const std::string &note);
  void lowerCast(CastStmtNode *node);
  void lowerUpdate(ASTNode *node, const std::string &name, std::string op);
};

} // namespace neuron::mir
