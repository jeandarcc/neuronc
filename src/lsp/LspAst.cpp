#include "LspAst.h"

#include "LspPath.h"

#include <algorithm>

namespace neuron::lsp::detail {

namespace {

void walkAstList(const std::vector<ASTNodePtr> &nodes,
                 const std::function<void(const ASTNode *)> &visitor) {
  for (const auto &node : nodes) {
    walkAst(node.get(), visitor);
  }
}

} // namespace

void walkAst(const ASTNode *node,
             const std::function<void(const ASTNode *)> &visitor) {
  if (node == nullptr) {
    return;
  }
  visitor(node);

  switch (node->type) {
  case ASTNodeType::Program:
    walkAstList(static_cast<const ProgramNode *>(node)->declarations, visitor);
    return;
  case ASTNodeType::BindingDecl: {
    const auto *binding = static_cast<const BindingDeclNode *>(node);
    walkAst(binding->target.get(), visitor);
    walkAst(binding->value.get(), visitor);
    walkAst(binding->typeAnnotation.get(), visitor);
    return;
  }
  case ASTNodeType::BinaryExpr: {
    const auto *binary = static_cast<const BinaryExprNode *>(node);
    walkAst(binary->left.get(), visitor);
    walkAst(binary->right.get(), visitor);
    return;
  }
  case ASTNodeType::UnaryExpr:
    walkAst(static_cast<const UnaryExprNode *>(node)->operand.get(), visitor);
    return;
  case ASTNodeType::CallExpr: {
    const auto *call = static_cast<const CallExprNode *>(node);
    walkAst(call->callee.get(), visitor);
    for (const auto &arg : call->arguments) {
      walkAst(arg.get(), visitor);
    }
    return;
  }
  case ASTNodeType::InputExpr: {
    const auto *input = static_cast<const InputExprNode *>(node);
    for (const auto &arg : input->typeArguments) {
      walkAst(arg.get(), visitor);
    }
    for (const auto &arg : input->promptArguments) {
      walkAst(arg.get(), visitor);
    }
    for (const auto &stage : input->stages) {
      for (const auto &arg : stage.arguments) {
        walkAst(arg.get(), visitor);
      }
    }
    return;
  }
  case ASTNodeType::MemberAccessExpr:
    walkAst(static_cast<const MemberAccessNode *>(node)->object.get(), visitor);
    return;
  case ASTNodeType::IndexExpr: {
    const auto *index = static_cast<const IndexExprNode *>(node);
    walkAst(index->object.get(), visitor);
    for (const auto &item : index->indices) {
      walkAst(item.get(), visitor);
    }
    return;
  }
  case ASTNodeType::SliceExpr: {
    const auto *slice = static_cast<const SliceExprNode *>(node);
    walkAst(slice->object.get(), visitor);
    walkAst(slice->start.get(), visitor);
    walkAst(slice->end.get(), visitor);
    return;
  }
  case ASTNodeType::TypeofExpr:
    walkAst(static_cast<const TypeofExprNode *>(node)->expression.get(), visitor);
    return;
  case ASTNodeType::MethodDecl: {
    const auto *method = static_cast<const MethodDeclNode *>(node);
    for (const auto &param : method->parameters) {
      walkAst(param.typeSpec.get(), visitor);
    }
    walkAst(method->returnType.get(), visitor);
    walkAst(method->body.get(), visitor);
    return;
  }
  case ASTNodeType::ClassDecl:
    walkAstList(static_cast<const ClassDeclNode *>(node)->members, visitor);
    return;
  case ASTNodeType::Block:
    walkAstList(static_cast<const BlockNode *>(node)->statements, visitor);
    return;
  case ASTNodeType::IfStmt: {
    const auto *stmt = static_cast<const IfStmtNode *>(node);
    walkAst(stmt->condition.get(), visitor);
    walkAst(stmt->thenBlock.get(), visitor);
    walkAst(stmt->elseBlock.get(), visitor);
    return;
  }
  case ASTNodeType::MatchStmt: {
    const auto *stmt = static_cast<const MatchStmtNode *>(node);
    for (const auto &expr : stmt->expressions) {
      walkAst(expr.get(), visitor);
    }
    walkAstList(stmt->arms, visitor);
    return;
  }
  case ASTNodeType::MatchExpr: {
    const auto *expr = static_cast<const MatchExprNode *>(node);
    for (const auto &selector : expr->expressions) {
      walkAst(selector.get(), visitor);
    }
    walkAstList(expr->arms, visitor);
    return;
  }
  case ASTNodeType::MatchArm: {
    const auto *arm = static_cast<const MatchArmNode *>(node);
    for (const auto &pattern : arm->patternExprs) {
      walkAst(pattern.get(), visitor);
    }
    walkAst(arm->body.get(), visitor);
    walkAst(arm->valueExpr.get(), visitor);
    return;
  }
  case ASTNodeType::WhileStmt: {
    const auto *stmt = static_cast<const WhileStmtNode *>(node);
    walkAst(stmt->condition.get(), visitor);
    walkAst(stmt->body.get(), visitor);
    return;
  }
  case ASTNodeType::ForStmt: {
    const auto *stmt = static_cast<const ForStmtNode *>(node);
    walkAst(stmt->init.get(), visitor);
    walkAst(stmt->condition.get(), visitor);
    walkAst(stmt->increment.get(), visitor);
    walkAst(stmt->body.get(), visitor);
    return;
  }
  case ASTNodeType::ForInStmt: {
    const auto *stmt = static_cast<const ForInStmtNode *>(node);
    walkAst(stmt->iterable.get(), visitor);
    walkAst(stmt->body.get(), visitor);
    return;
  }
  case ASTNodeType::ReturnStmt:
    walkAst(static_cast<const ReturnStmtNode *>(node)->value.get(), visitor);
    return;
  case ASTNodeType::CastStmt: {
    const auto *stmt = static_cast<const CastStmtNode *>(node);
    walkAst(stmt->target.get(), visitor);
    for (const auto &step : stmt->steps) {
      walkAst(step.typeSpec.get(), visitor);
    }
    return;
  }
  case ASTNodeType::CatchClause: {
    const auto *clause = static_cast<const CatchClauseNode *>(node);
    walkAst(clause->errorType.get(), visitor);
    walkAst(clause->body.get(), visitor);
    return;
  }
  case ASTNodeType::TryStmt: {
    const auto *stmt = static_cast<const TryStmtNode *>(node);
    walkAst(stmt->tryBlock.get(), visitor);
    walkAstList(stmt->catchClauses, visitor);
    walkAst(stmt->finallyBlock.get(), visitor);
    return;
  }
  case ASTNodeType::ThrowStmt:
    walkAst(static_cast<const ThrowStmtNode *>(node)->value.get(), visitor);
    return;
  case ASTNodeType::StaticAssertStmt:
    walkAst(static_cast<const StaticAssertStmtNode *>(node)->condition.get(),
            visitor);
    return;
  case ASTNodeType::UnsafeBlock:
    walkAst(static_cast<const UnsafeBlockNode *>(node)->body.get(), visitor);
    return;
  case ASTNodeType::GpuBlock:
    walkAst(static_cast<const GpuBlockNode *>(node)->body.get(), visitor);
    return;
  case ASTNodeType::CanvasStmt: {
    const auto *canvas = static_cast<const CanvasStmtNode *>(node);
    walkAst(canvas->windowExpr.get(), visitor);
    walkAstList(canvas->handlers, visitor);
    return;
  }
  case ASTNodeType::CanvasEventHandler:
    walkAst(static_cast<const CanvasEventHandlerNode *>(node)->handlerMethod.get(),
            visitor);
    return;
  case ASTNodeType::ShaderStage:
    walkAst(static_cast<const ShaderStageNode *>(node)->methodDecl.get(), visitor);
    return;
  case ASTNodeType::ShaderDecl: {
    const auto *shader = static_cast<const ShaderDeclNode *>(node);
    walkAstList(shader->uniforms, visitor);
    walkAstList(shader->stages, visitor);
    walkAstList(shader->methods, visitor);
    return;
  }
  case ASTNodeType::TypeSpec: {
    const auto *typeSpec = static_cast<const TypeSpecNode *>(node);
    for (const auto &arg : typeSpec->genericArgs) {
      walkAst(arg.get(), visitor);
    }
    return;
  }
  default:
    return;
  }
}

SourceLocation astNodeEndLocation(const ASTNode *node) {
  if (node == nullptr) {
    return {};
  }
  if (node->type == ASTNodeType::Block) {
    const auto *block = static_cast<const BlockNode *>(node);
    return {block->endLine, block->endColumn, block->location.file};
  }
  if (node->type == ASTNodeType::MethodDecl) {
    const auto *method = static_cast<const MethodDeclNode *>(node);
    if (method->body != nullptr && method->body->type == ASTNodeType::Block) {
      const auto *block = static_cast<const BlockNode *>(method->body.get());
      return {block->endLine, block->endColumn, block->location.file};
    }
  }
  return node->location;
}

int compareSourceLocations(const SourceLocation &lhs, const SourceLocation &rhs) {
  if (lhs.line != rhs.line) {
    return lhs.line < rhs.line ? -1 : 1;
  }
  if (lhs.column != rhs.column) {
    return lhs.column < rhs.column ? -1 : 1;
  }
  return 0;
}

bool locationWithinSpan(const SourceLocation &location,
                        const SourceLocation &start,
                        const SourceLocation &end) {
  return samePath(location.file, start.file) && samePath(location.file, end.file) &&
         compareSourceLocations(location, start) >= 0 &&
         compareSourceLocations(location, end) <= 0;
}

SymbolRange makePointSymbolRange(const SourceLocation &location, int length) {
  SymbolRange range;
  range.start = location;
  range.end = location;
  range.end.column += std::max(1, length);
  return range;
}

SymbolRange makeNodeSymbolRange(const ASTNode *node, int selectionLength) {
  if (node == nullptr) {
    return {};
  }

  SymbolRange range;
  range.start = node->location;
  range.end = astNodeEndLocation(node);
  if (!samePath(range.start.file, range.end.file) ||
      compareSourceLocations(range.end, range.start) < 0) {
    range = makePointSymbolRange(node->location, selectionLength);
  }
  return range;
}

const MethodDeclNode *findMethodByLocation(const ASTNode *root,
                                           const SourceLocation &location) {
  const MethodDeclNode *result = nullptr;
  walkAst(root, [&](const ASTNode *node) {
    if (node != nullptr && node->type == ASTNodeType::MethodDecl &&
        sameLocation(node->location, location)) {
      result = static_cast<const MethodDeclNode *>(node);
    }
  });
  return result;
}

const ClassDeclNode *findClassByLocation(const ASTNode *root,
                                         const SourceLocation &location) {
  const ClassDeclNode *result = nullptr;
  walkAst(root, [&](const ASTNode *node) {
    if (node != nullptr && node->type == ASTNodeType::ClassDecl &&
        sameLocation(node->location, location)) {
      result = static_cast<const ClassDeclNode *>(node);
    }
  });
  return result;
}

const BindingDeclNode *findBindingByLocation(const ASTNode *root,
                                             const SourceLocation &location) {
  const BindingDeclNode *result = nullptr;
  walkAst(root, [&](const ASTNode *node) {
    if (node != nullptr && node->type == ASTNodeType::BindingDecl &&
        sameLocation(node->location, location)) {
      result = static_cast<const BindingDeclNode *>(node);
    }
  });
  return result;
}

const MethodDeclNode *findEnclosingMethod(const ASTNode *root,
                                          const SourceLocation &location) {
  const MethodDeclNode *result = nullptr;
  walkAst(root, [&](const ASTNode *node) {
    if (node == nullptr || node->type != ASTNodeType::MethodDecl ||
        !samePath(node->location.file, location.file)) {
      return;
    }
    const auto *method = static_cast<const MethodDeclNode *>(node);
    const SourceLocation endLocation = astNodeEndLocation(method);
    if (!locationWithinSpan(location, method->location, endLocation)) {
      return;
    }
    if (result == nullptr ||
        compareSourceLocations(method->location, result->location) >= 0) {
      result = method;
    }
  });
  return result;
}

const ClassDeclNode *findOwningClass(const ASTNode *root,
                                     const MethodDeclNode *method) {
  if (root == nullptr || method == nullptr) {
    return nullptr;
  }

  const ClassDeclNode *result = nullptr;
  walkAst(root, [&](const ASTNode *node) {
    if (node == nullptr || node->type != ASTNodeType::ClassDecl) {
      return;
    }
    const auto *classDecl = static_cast<const ClassDeclNode *>(node);
    const bool ownsMethod = std::any_of(
        classDecl->members.begin(), classDecl->members.end(),
        [&](const ASTNodePtr &member) { return member.get() == method; });
    if (ownsMethod) {
      result = classDecl;
    }
  });
  return result;
}

const CallExprNode *findCallByCalleeLocation(const ASTNode *root,
                                             const SourceLocation &location) {
  const CallExprNode *result = nullptr;
  walkAst(root, [&](const ASTNode *node) {
    if (node == nullptr || node->type != ASTNodeType::CallExpr) {
      return;
    }

    const auto *call = static_cast<const CallExprNode *>(node);
    if (call->callee == nullptr) {
      return;
    }

    bool matches = false;
    switch (call->callee->type) {
    case ASTNodeType::Identifier:
    case ASTNodeType::TypeSpec:
      matches = sameLocation(call->callee->location, location);
      break;
    case ASTNodeType::MemberAccessExpr:
      matches = sameLocation(
          static_cast<const MemberAccessNode *>(call->callee.get())->memberLocation,
          location);
      break;
    default:
      break;
    }

    if (matches) {
      result = call;
    }
  });
  return result;
}

} // namespace neuron::lsp::detail
