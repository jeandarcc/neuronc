#pragma once

#include "neuronc/parser/AST.h"

#include <optional>
#include <string>
#include <string_view>

namespace neuron {
class BlockNode;
class CastStmtNode;
class ForInStmtNode;
class ForStmtNode;
class IfStmtNode;
class MacroDeclNode;
class MatchStmtNode;
class ReturnStmtNode;
class StaticAssertStmtNode;
class ThrowStmtNode;
class TryStmtNode;
class WhileStmtNode;
} // namespace neuron

namespace neuron::sema_detail {

class SemanticDriver;

class StatementAnalyzer {
public:
  explicit StatementAnalyzer(SemanticDriver &driver);

  void visitBlock(BlockNode *node);
  void visitIfStmt(IfStmtNode *node);
  void visitMatchStmt(MatchStmtNode *node);
  void visitCastStmt(CastStmtNode *node);
  void visitWhileStmt(WhileStmtNode *node);
  void visitForStmt(ForStmtNode *node);
  void visitForInStmt(ForInStmtNode *node);
  void visitReturnStmt(ReturnStmtNode *node);
  void visitTryStmt(TryStmtNode *node);
  void visitThrowStmt(ThrowStmtNode *node);
  void visitMacroDecl(MacroDeclNode *node);
  void visitStaticAssertStmt(StaticAssertStmtNode *node);

private:
  bool visitStatementNode(ASTNode *node);
  bool visitStatementLike(ASTNode *node);
  std::optional<bool> evaluateConstantBool(ASTNode *node) const;
  void validateControlDepth(const SourceLocation &loc,
                            const std::string &statementName,
                            std::string_view hint);

  SemanticDriver &m_driver;
};

} // namespace neuron::sema_detail
