#pragma once

#include "neuronc/nir/NIR.h"
#include "neuronc/parser/AST.h"
#include "neuronc/sema/SemanticAnalyzer.h"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace neuron {
namespace nir {

class NIRBuilder {
public:
  NIRBuilder();

  std::unique_ptr<Module> build(ASTNode *root, const std::string &moduleName);
  void setModuleCppModules(
      const std::unordered_map<std::string, NativeModuleInfo> &modules);

  bool hasErrors() const { return !m_errors.empty(); }
  const std::vector<std::string> &errors() const { return m_errors; }
  void clearErrors() {
    m_errors.clear();
    m_hadError = false;
  }

  // Node Visitors
  void visitProgram(ProgramNode *node);
  void visitClassDecl(ClassDeclNode *node);
  void visitEnumDecl(EnumDeclNode *node);
  void visitMethodDecl(MethodDeclNode *node);
  void visitShaderDecl(ShaderDeclNode *node);
  void visitBindingDecl(BindingDeclNode *node);
  void visitBlock(BlockNode *node);
  void visitCastStmt(CastStmtNode *node);

  // Expression Visitors returning Values
  Value *buildExpression(ASTNode *node);
  Value *buildBinaryExpr(BinaryExprNode *node);
  Value *buildUnaryExpr(UnaryExprNode *node);
  Value *buildCallExpr(CallExprNode *node);
  Value *buildInputExpr(InputExprNode *node);
  Value *buildMatchExpr(MatchExprNode *node);
  Value *buildIdentifier(IdentifierNode *node);
  Value *buildAddressOf(ASTNode *node);
  Value *buildMemberAccess(MemberAccessNode *node);
  Value *buildSliceExpr(SliceExprNode *node);
  Value *buildLValue(ASTNode *node);

  // Statements
  void buildIfStmt(IfStmtNode *node);
  void buildMatchStmt(MatchStmtNode *node);
  void buildWhileStmt(WhileStmtNode *node);
  void buildForStmt(ForStmtNode *node);
  void buildReturnStmt(ReturnStmtNode *node);
  void buildBreakStmt(BreakStmtNode *node);
  void buildContinueStmt(ContinueStmtNode *node);
  void buildIncrementStmt(IncrementStmtNode *node);
  void buildDecrementStmt(DecrementStmtNode *node);
  void buildTryStmt(TryStmtNode *node);
  void buildThrowStmt(ThrowStmtNode *node);
  void buildCanvasStmt(CanvasStmtNode *node);

  // API to create instructions
  Instruction *createInst(InstKind kind, NTypePtr type,
                          const std::string &name = "");
  void insertInst(std::unique_ptr<Instruction> inst);
  void applyExecutionHint(Instruction *inst);
  void emitGpuScopeBeginInst(int32_t preferenceMode = 0,
                             int32_t preferenceTarget = 0);
  void emitGpuScopeEndInst();
  void unwindGpuScopesForReturn();
  void unwindGpuScopesForBreak();
  void unwindGpuScopesForContinue();
  void reportError(const SourceLocation &location, const std::string &message);

private:
  std::unique_ptr<Module> m_module;
  Function *m_currentFunction = nullptr;
  Block *m_currentBlock = nullptr;

  // Symbol table mapping AST variables to NIR Values
  std::vector<std::unordered_map<std::string, Value *>> m_scopes;
  std::vector<std::unordered_map<std::string, std::string>>
      m_materialShaderScopes;

  void enterScope();
  void leaveScope();
  void defineSymbol(const std::string &name, Value *val);
  Value *lookupSymbol(const std::string &name);
  void defineMaterialShader(const std::string &name, std::string shaderName);
  std::string lookupMaterialShader(const std::string &name) const;

  // Counters for unique naming
  int m_valDefCounter = 0;
  std::string nextValName();

  int m_blockCounter = 0;
  std::string nextBlockName();
  int m_gpuDepth = 0;
  struct GpuRuntimeScopeFrame {
    std::size_t breakDepthAtEntry = 0;
    std::size_t continueDepthAtEntry = 0;
  };
  std::vector<GpuRuntimeScopeFrame> m_gpuRuntimeScopeStack;

  std::vector<Block *> m_throwTargetStack;
  std::vector<Block *> m_breakTargetStack;
  std::vector<Block *> m_continueTargetStack;
  std::unordered_map<std::string, NativeModuleInfo> m_moduleCppModules;
  std::unordered_map<std::string, std::unordered_map<std::string, int64_t>>
      m_enumMembers;

  NTypePtr resolveType(ASTNode *node);

  bool m_hadError = false;
  std::vector<std::string> m_errors;
};


} // namespace nir
} // namespace neuron
