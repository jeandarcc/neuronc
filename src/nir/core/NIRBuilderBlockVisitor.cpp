#include "neuronc/nir/NIRBuilder.h"

#include "../detail/NIRBuilderShared.h"

namespace neuron::nir {

void NIRBuilder::visitBlock(BlockNode *node) {
  enterScope();
  for (auto &stmt : node->statements) {
    if (stmt->type == ASTNodeType::BindingDecl) {
      visitBindingDecl(static_cast<BindingDeclNode *>(stmt.get()));
    } else if (stmt->type == ASTNodeType::IfStmt) {
      buildIfStmt(static_cast<IfStmtNode *>(stmt.get()));
    } else if (stmt->type == ASTNodeType::MatchStmt) {
      buildMatchStmt(static_cast<MatchStmtNode *>(stmt.get()));
    } else if (stmt->type == ASTNodeType::WhileStmt) {
      buildWhileStmt(static_cast<WhileStmtNode *>(stmt.get()));
    } else if (stmt->type == ASTNodeType::ForStmt) {
      buildForStmt(static_cast<ForStmtNode *>(stmt.get()));
    } else if (stmt->type == ASTNodeType::CastStmt) {
      visitCastStmt(static_cast<CastStmtNode *>(stmt.get()));
    } else if (stmt->type == ASTNodeType::ReturnStmt) {
      buildReturnStmt(static_cast<ReturnStmtNode *>(stmt.get()));
    } else if (stmt->type == ASTNodeType::BreakStmt) {
      buildBreakStmt(static_cast<BreakStmtNode *>(stmt.get()));
    } else if (stmt->type == ASTNodeType::ContinueStmt) {
      buildContinueStmt(static_cast<ContinueStmtNode *>(stmt.get()));
    } else if (stmt->type == ASTNodeType::IncrementStmt) {
      buildIncrementStmt(static_cast<IncrementStmtNode *>(stmt.get()));
    } else if (stmt->type == ASTNodeType::DecrementStmt) {
      buildDecrementStmt(static_cast<DecrementStmtNode *>(stmt.get()));
    } else if (stmt->type == ASTNodeType::TryStmt) {
      buildTryStmt(static_cast<TryStmtNode *>(stmt.get()));
    } else if (stmt->type == ASTNodeType::ThrowStmt) {
      buildThrowStmt(static_cast<ThrowStmtNode *>(stmt.get()));
    } else if (stmt->type == ASTNodeType::GpuBlock) {
      auto *gpuBlock = static_cast<GpuBlockNode *>(stmt.get());
      if (gpuBlock->body != nullptr &&
          gpuBlock->body->type == ASTNodeType::Block) {
        const std::size_t scopeDepthAtEntry = m_gpuRuntimeScopeStack.size();
        emitGpuScopeBeginInst(static_cast<int32_t>(gpuBlock->preferenceMode),
                              static_cast<int32_t>(gpuBlock->preferenceTarget));
        m_gpuRuntimeScopeStack.push_back(
            {m_breakTargetStack.size(), m_continueTargetStack.size()});
        ++m_gpuDepth;
        visitBlock(static_cast<BlockNode *>(gpuBlock->body.get()));
        --m_gpuDepth;
        if (m_gpuRuntimeScopeStack.size() > scopeDepthAtEntry) {
          if (!detail::blockHasTerminator(m_currentBlock)) {
            emitGpuScopeEndInst();
          }
          m_gpuRuntimeScopeStack.pop_back();
        }
      }
    } else if (stmt->type == ASTNodeType::CanvasStmt) {
      buildCanvasStmt(static_cast<CanvasStmtNode *>(stmt.get()));
    } else if (stmt->type == ASTNodeType::ShaderDecl) {
      visitShaderDecl(static_cast<ShaderDeclNode *>(stmt.get()));
    } else if (stmt->type == ASTNodeType::ShaderPassStmt) {
    } else {
      buildExpression(stmt.get());
    }

    if (detail::blockHasTerminator(m_currentBlock)) {
      break;
    }
  }
  leaveScope();
}

} // namespace neuron::nir

