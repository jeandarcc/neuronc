#include "GraphicsAnalyzer.h"

#include "AnalysisHelpers.h"
#include "SemanticDriver.h"

#include <unordered_map>
#include <unordered_set>

namespace neuron::sema_detail {

namespace {

struct ShaderExprTypeInfo {
  std::string typeName;
  bool valid = true;
};

bool isShaderNumericType(const std::string &typeName) {
  return typeName == "int" || typeName == "float" || typeName == "double";
}

bool isShaderVectorType(const std::string &typeName) {
  return typeName == "Vector2" || typeName == "Vector3" ||
         typeName == "Vector4" || typeName == "Color";
}

bool isShaderVertexInputName(const std::string &name) {
  return name == "position" || name == "uv" || name == "normal";
}

bool isShaderVertexInputType(const std::string &name, const std::string &typeName) {
  if (name == "position") {
    return typeName == "Vector3";
  }
  if (name == "uv") {
    return typeName == "Vector2";
  }
  if (name == "normal") {
    return typeName == "Vector3";
  }
  return false;
}

uint32_t shaderVertexLayoutBitForName(const std::string &name) {
  if (name == "position") {
    return 1u << 0;
  }
  if (name == "uv") {
    return 1u << 1;
  }
  if (name == "normal") {
    return 1u << 2;
  }
  return 0u;
}

ShaderExprTypeInfo inferShaderExprType(
    AnalysisContext &context, ASTNode *expr, ShaderStageKind stageKind,
    const std::unordered_map<std::string, std::string> &knownTypes,
    bool *usesMvp) {
  if (expr == nullptr) {
    return {"<unknown>", false};
  }

  switch (expr->type) {
  case ASTNodeType::IntLiteral:
    return {"int", true};
  case ASTNodeType::FloatLiteral:
    return {"float", true};
  case ASTNodeType::Identifier: {
    const auto &name = static_cast<IdentifierNode *>(expr)->name;
    auto it = knownTypes.find(name);
    if (it == knownTypes.end()) {
      context.error(expr->location,
                    "Unsupported shader identifier: " + name);
      return {"<error>", false};
    }
    if (name == "MVP" && usesMvp != nullptr) {
      *usesMvp = true;
    }
    return {it->second, true};
  }
  case ASTNodeType::UnaryExpr: {
    auto *unary = static_cast<UnaryExprNode *>(expr);
    if (unary->op != TokenType::Minus) {
      context.error(expr->location,
                    "Unsupported shader unary operator");
      return {"<error>", false};
    }
    return inferShaderExprType(context, unary->operand.get(), stageKind,
                               knownTypes, usesMvp);
  }
  case ASTNodeType::BinaryExpr: {
    auto *binary = static_cast<BinaryExprNode *>(expr);
    if (binary->op != TokenType::Plus && binary->op != TokenType::Minus &&
        binary->op != TokenType::Star && binary->op != TokenType::Slash &&
        binary->op != TokenType::Caret &&
        binary->op != TokenType::CaretCaret) {
      context.error(expr->location,
                    "Unsupported shader binary operator");
      return {"<error>", false};
    }

    ShaderExprTypeInfo left = inferShaderExprType(
        context, binary->left.get(), stageKind, knownTypes, usesMvp);
    ShaderExprTypeInfo right = inferShaderExprType(
        context, binary->right.get(), stageKind, knownTypes, usesMvp);
    if (!left.valid || !right.valid) {
      return {"<error>", false};
    }

    if (binary->op == TokenType::CaretCaret && right.typeName != "int") {
      context.error(expr->location,
                    "Shader ^^ right-hand side must be int");
      return {"<error>", false};
    }

    if (binary->op == TokenType::Star && left.typeName == "Matrix4" &&
        right.typeName == "Vector3") {
      return {"Vector4", true};
    }
    if (binary->op == TokenType::Star && left.typeName == "Matrix4" &&
        right.typeName == "Vector4") {
      return {"Vector4", true};
    }
    if (left.typeName == right.typeName &&
        (isShaderNumericType(left.typeName) || isShaderVectorType(left.typeName) ||
         left.typeName == "Matrix4")) {
      return {left.typeName, true};
    }
    if ((isShaderVectorType(left.typeName) && isShaderNumericType(right.typeName)) ||
        (isShaderNumericType(left.typeName) && isShaderVectorType(right.typeName))) {
      return {isShaderVectorType(left.typeName) ? left.typeName : right.typeName,
              true};
    }

    context.error(expr->location,
                  "Unsupported shader operand types: '" + left.typeName +
                      "' and '" + right.typeName + "'");
    return {"<error>", false};
  }
  case ASTNodeType::CallExpr: {
    auto *call = static_cast<CallExprNode *>(expr);
    const std::string callName = resolveCallName(call->callee.get());
    if (callName == "Color") {
      if (call->arguments.size() != 4) {
        context.error(expr->location,
                      "Color(...) requires four components inside shaders");
        return {"<error>", false};
      }
      for (const auto &arg : call->arguments) {
        ShaderExprTypeInfo argType = inferShaderExprType(
            context, arg.get(), stageKind, knownTypes, usesMvp);
        if (!argType.valid || !isShaderNumericType(argType.typeName)) {
          context.error(arg ? arg->location : expr->location,
                        "Color(...) arguments must be numeric");
          return {"<error>", false};
        }
      }
      return {"Color", true};
    }
    if (callName == "Vector2" || callName == "Vector3" || callName == "Vector4") {
      const size_t expectedArgs =
          callName == "Vector2" ? 2u : (callName == "Vector3" ? 3u : 4u);
      if (call->arguments.size() != expectedArgs) {
        context.error(expr->location,
                      callName + "(...) has invalid component count");
        return {"<error>", false};
      }
      for (const auto &arg : call->arguments) {
        ShaderExprTypeInfo argType = inferShaderExprType(
            context, arg.get(), stageKind, knownTypes, usesMvp);
        if (!argType.valid || !isShaderNumericType(argType.typeName)) {
          context.error(arg ? arg->location : expr->location,
                        callName + "(...) arguments must be numeric");
          return {"<error>", false};
        }
      }
      return {callName, true};
    }
    if (call->callee != nullptr &&
        call->callee->type == ASTNodeType::MemberAccessExpr) {
      auto *member = static_cast<MemberAccessNode *>(call->callee.get());
      if (member->member == "Sample") {
        if (stageKind != ShaderStageKind::Fragment) {
          context.error(expr->location,
                        "Texture2D.Sample(...) is only allowed in Fragment stage");
          return {"<error>", false};
        }
        ShaderExprTypeInfo objectType = inferShaderExprType(
            context, member->object.get(), stageKind, knownTypes, usesMvp);
        if (!objectType.valid || objectType.typeName != "Texture2D") {
          context.error(member->location,
                        "Texture2D.Sample(...) requires a Texture2D receiver");
          return {"<error>", false};
        }
        if (call->arguments.size() != 2) {
          context.error(expr->location,
                        "Texture2D.Sample(...) requires sampler and uv arguments");
          return {"<error>", false};
        }
        ShaderExprTypeInfo samplerType = inferShaderExprType(
            context, call->arguments[0].get(), stageKind, knownTypes, usesMvp);
        ShaderExprTypeInfo uvType = inferShaderExprType(
            context, call->arguments[1].get(), stageKind, knownTypes, usesMvp);
        if (!samplerType.valid || samplerType.typeName != "Sampler") {
          context.error(call->arguments[0]->location,
                        "Texture2D.Sample(...) requires a Sampler first argument");
          return {"<error>", false};
        }
        if (!uvType.valid || uvType.typeName != "Vector2") {
          context.error(call->arguments[1]->location,
                        "Texture2D.Sample(...) requires a Vector2 uv argument");
          return {"<error>", false};
        }
        return {"Color", true};
      }
    }
    context.error(expr->location,
                  "Unsupported shader call expression: " + callName);
    return {"<error>", false};
  }
  default:
    context.error(expr->location, "Unsupported shader expression");
    return {"<error>", false};
  }
}

uint32_t validateVertexShaderStage(
    AnalysisContext &context, MethodDeclNode *method,
    const std::unordered_map<std::string, std::string> &uniformTypes,
    std::unordered_map<std::string, std::string> *outPassedTypes,
    bool *usesMvp) {
  uint32_t vertexLayoutMask = 0;
  std::unordered_map<std::string, std::string> knownTypes = uniformTypes;
  knownTypes["MVP"] = "Matrix4";

  if (method == nullptr || method->body == nullptr ||
      method->body->type != ASTNodeType::Block) {
    context.error(method ? method->location : SourceLocation{},
                  "Vertex stage requires a block body");
    return 0;
  }

  for (const auto &param : method->parameters) {
    NTypePtr paramType = context.resolveType(param.typeSpec.get());
    const std::string typeName = paramType ? paramType->toString() : "<unknown>";
    if (!isShaderVertexInputName(param.name) ||
        !isShaderVertexInputType(param.name, typeName)) {
      context.error(param.location,
                    "Vertex stage parameter '" + param.name +
                        "' must use canonical graphics input type");
    }
    knownTypes[param.name] = typeName;
    vertexLayoutMask |= shaderVertexLayoutBitForName(param.name);
  }

  bool hasReturn = false;
  auto *body = static_cast<BlockNode *>(method->body.get());
  for (const auto &stmt : body->statements) {
    if (stmt->type == ASTNodeType::ShaderPassStmt) {
      const std::string &varyingName =
          static_cast<ShaderPassStmtNode *>(stmt.get())->varyingName;
      auto typeIt = knownTypes.find(varyingName);
      if (typeIt == knownTypes.end()) {
        context.error(stmt->location,
                      "pass requires a visible varying source identifier");
        continue;
      }
      (*outPassedTypes)[varyingName] = typeIt->second;
      continue;
    }
    if (stmt->type != ASTNodeType::ReturnStmt) {
      context.error(stmt->location,
                    "Vertex stage only supports pass and return statements");
      continue;
    }

    hasReturn = true;
    auto *ret = static_cast<ReturnStmtNode *>(stmt.get());
    ShaderExprTypeInfo retType = inferShaderExprType(
        context, ret->value.get(), ShaderStageKind::Vertex, knownTypes, usesMvp);
    if (!retType.valid) {
      continue;
    }
    if (retType.typeName != "Vector3" && retType.typeName != "Vector4") {
      context.error(ret->location,
                    "Vertex stage must return Vector3 or Vector4");
    }
  }

  if (!hasReturn) {
    context.error(method->location, "Vertex stage requires a return statement");
  }
  return vertexLayoutMask;
}

void validateFragmentShaderStage(
    AnalysisContext &context, MethodDeclNode *method,
    const std::unordered_map<std::string, std::string> &uniformTypes,
    const std::unordered_map<std::string, std::string> &passedTypes,
    bool *usesMvp) {
  std::unordered_map<std::string, std::string> knownTypes = uniformTypes;
  knownTypes["MVP"] = "Matrix4";

  if (method == nullptr || method->body == nullptr ||
      method->body->type != ASTNodeType::Block) {
    context.error(method ? method->location : SourceLocation{},
                  "Fragment stage requires a block body");
    return;
  }

  for (const auto &param : method->parameters) {
    NTypePtr paramType = context.resolveType(param.typeSpec.get());
    const std::string typeName = paramType ? paramType->toString() : "<unknown>";
    auto varyingIt = passedTypes.find(param.name);
    if (varyingIt == passedTypes.end()) {
      context.error(param.location,
                    "Fragment stage parameter '" + param.name +
                        "' was not passed from Vertex stage");
    } else if (varyingIt->second != typeName) {
      context.error(param.location,
                    "Fragment stage parameter '" + param.name +
                        "' type mismatch; expected '" + varyingIt->second +
                        "' but got '" + typeName + "'");
    }
    knownTypes[param.name] = typeName;
  }

  bool hasReturn = false;
  auto *body = static_cast<BlockNode *>(method->body.get());
  for (const auto &stmt : body->statements) {
    if (stmt->type == ASTNodeType::ShaderPassStmt) {
      context.error(stmt->location,
                    "pass statements are only allowed in Vertex stage");
      continue;
    }
    if (stmt->type != ASTNodeType::ReturnStmt) {
      context.error(stmt->location,
                    "Fragment stage only supports return statements");
      continue;
    }
    hasReturn = true;
    auto *ret = static_cast<ReturnStmtNode *>(stmt.get());
    ShaderExprTypeInfo retType = inferShaderExprType(
        context, ret->value.get(), ShaderStageKind::Fragment, knownTypes, usesMvp);
    if (!retType.valid) {
      continue;
    }
    if (retType.typeName != "Color" && retType.typeName != "Vector4") {
      context.error(ret->location,
                    "Fragment stage must return Color or Vector4");
    }
  }

  if (!hasReturn) {
    context.error(method->location, "Fragment stage requires a return statement");
  }
}

bool containsShaderPassStatement(const ASTNode *node) {
  if (node == nullptr) {
    return false;
  }
  if (node->type == ASTNodeType::ShaderPassStmt) {
    return true;
  }
  if (node->type == ASTNodeType::Block) {
    const auto *block = static_cast<const BlockNode *>(node);
    for (const auto &statement : block->statements) {
      if (containsShaderPassStatement(statement.get())) {
        return true;
      }
    }
    return false;
  }
  if (node->type == ASTNodeType::IfStmt) {
    const auto *ifNode = static_cast<const IfStmtNode *>(node);
    return containsShaderPassStatement(ifNode->condition.get()) ||
           containsShaderPassStatement(ifNode->thenBlock.get()) ||
           containsShaderPassStatement(ifNode->elseBlock.get());
  }
  if (node->type == ASTNodeType::WhileStmt) {
    const auto *whileNode = static_cast<const WhileStmtNode *>(node);
    return containsShaderPassStatement(whileNode->condition.get()) ||
           containsShaderPassStatement(whileNode->body.get());
  }
  if (node->type == ASTNodeType::ForStmt) {
    const auto *forNode = static_cast<const ForStmtNode *>(node);
    return containsShaderPassStatement(forNode->init.get()) ||
           containsShaderPassStatement(forNode->condition.get()) ||
           containsShaderPassStatement(forNode->increment.get()) ||
           containsShaderPassStatement(forNode->body.get());
  }
  if (node->type == ASTNodeType::ForInStmt) {
    const auto *forInNode = static_cast<const ForInStmtNode *>(node);
    return containsShaderPassStatement(forInNode->iterable.get()) ||
           containsShaderPassStatement(forInNode->body.get());
  }
  if (node->type == ASTNodeType::MatchStmt ||
      node->type == ASTNodeType::MatchExpr) {
    const auto &expressions =
        node->type == ASTNodeType::MatchStmt
            ? static_cast<const MatchStmtNode *>(node)->expressions
            : static_cast<const MatchExprNode *>(node)->expressions;
    const auto &arms = node->type == ASTNodeType::MatchStmt
                           ? static_cast<const MatchStmtNode *>(node)->arms
                           : static_cast<const MatchExprNode *>(node)->arms;
    for (const auto &expression : expressions) {
      if (containsShaderPassStatement(expression.get())) {
        return true;
      }
    }
    for (const auto &arm : arms) {
      if (containsShaderPassStatement(arm.get())) {
        return true;
      }
    }
    return false;
  }
  if (node->type == ASTNodeType::MatchArm) {
    const auto *arm = static_cast<const MatchArmNode *>(node);
    for (const auto &patternExpr : arm->patternExprs) {
      if (containsShaderPassStatement(patternExpr.get())) {
        return true;
      }
    }
    return containsShaderPassStatement(arm->body.get()) ||
           containsShaderPassStatement(arm->valueExpr.get());
  }
  if (node->type == ASTNodeType::ReturnStmt) {
    return containsShaderPassStatement(
        static_cast<const ReturnStmtNode *>(node)->value.get());
  }
  if (node->type == ASTNodeType::BindingDecl) {
    const auto *binding = static_cast<const BindingDeclNode *>(node);
    return containsShaderPassStatement(binding->target.get()) ||
           containsShaderPassStatement(binding->value.get()) ||
           containsShaderPassStatement(binding->typeAnnotation.get());
  }
  if (node->type == ASTNodeType::CastStmt) {
    const auto *castNode = static_cast<const CastStmtNode *>(node);
    if (containsShaderPassStatement(castNode->target.get())) {
      return true;
    }
    for (const auto &step : castNode->steps) {
      if (containsShaderPassStatement(step.typeSpec.get())) {
        return true;
      }
    }
    return false;
  }
  if (node->type == ASTNodeType::CallExpr) {
    const auto *call = static_cast<const CallExprNode *>(node);
    if (containsShaderPassStatement(call->callee.get())) {
      return true;
    }
    for (const auto &arg : call->arguments) {
      if (containsShaderPassStatement(arg.get())) {
        return true;
      }
    }
    return false;
  }
  if (node->type == ASTNodeType::MemberAccessExpr) {
    return containsShaderPassStatement(
        static_cast<const MemberAccessNode *>(node)->object.get());
  }
  if (node->type == ASTNodeType::IndexExpr) {
    const auto *index = static_cast<const IndexExprNode *>(node);
    if (containsShaderPassStatement(index->object.get())) {
      return true;
    }
    for (const auto &idx : index->indices) {
      if (containsShaderPassStatement(idx.get())) {
        return true;
      }
    }
    return false;
  }
  if (node->type == ASTNodeType::SliceExpr) {
    const auto *slice = static_cast<const SliceExprNode *>(node);
    return containsShaderPassStatement(slice->object.get()) ||
           containsShaderPassStatement(slice->start.get()) ||
           containsShaderPassStatement(slice->end.get());
  }
  if (node->type == ASTNodeType::TypeofExpr) {
    return containsShaderPassStatement(
        static_cast<const TypeofExprNode *>(node)->expression.get());
  }
  if (node->type == ASTNodeType::BinaryExpr) {
    const auto *binary = static_cast<const BinaryExprNode *>(node);
    return containsShaderPassStatement(binary->left.get()) ||
           containsShaderPassStatement(binary->right.get());
  }
  if (node->type == ASTNodeType::UnaryExpr) {
    return containsShaderPassStatement(
        static_cast<const UnaryExprNode *>(node)->operand.get());
  }
  if (node->type == ASTNodeType::TryStmt) {
    const auto *tryNode = static_cast<const TryStmtNode *>(node);
    if (containsShaderPassStatement(tryNode->tryBlock.get()) ||
        containsShaderPassStatement(tryNode->finallyBlock.get())) {
      return true;
    }
    for (const auto &catchNode : tryNode->catchClauses) {
      if (containsShaderPassStatement(catchNode.get())) {
        return true;
      }
    }
    return false;
  }
  if (node->type == ASTNodeType::CatchClause) {
    const auto *catchNode = static_cast<const CatchClauseNode *>(node);
    return containsShaderPassStatement(catchNode->errorType.get()) ||
           containsShaderPassStatement(catchNode->body.get());
  }
  if (node->type == ASTNodeType::ThrowStmt) {
    return containsShaderPassStatement(
        static_cast<const ThrowStmtNode *>(node)->value.get());
  }
  return false;
}

bool containsIdentifierNamed(const ASTNode *node, const std::string &name) {
  if (node == nullptr) {
    return false;
  }
  if (node->type == ASTNodeType::Identifier) {
    return static_cast<const IdentifierNode *>(node)->name == name;
  }
  if (node->type == ASTNodeType::Block) {
    const auto *block = static_cast<const BlockNode *>(node);
    for (const auto &statement : block->statements) {
      if (containsIdentifierNamed(statement.get(), name)) {
        return true;
      }
    }
    return false;
  }
  if (node->type == ASTNodeType::IfStmt) {
    const auto *ifNode = static_cast<const IfStmtNode *>(node);
    return containsIdentifierNamed(ifNode->condition.get(), name) ||
           containsIdentifierNamed(ifNode->thenBlock.get(), name) ||
           containsIdentifierNamed(ifNode->elseBlock.get(), name);
  }
  if (node->type == ASTNodeType::WhileStmt) {
    const auto *whileNode = static_cast<const WhileStmtNode *>(node);
    return containsIdentifierNamed(whileNode->condition.get(), name) ||
           containsIdentifierNamed(whileNode->body.get(), name);
  }
  if (node->type == ASTNodeType::ForStmt) {
    const auto *forNode = static_cast<const ForStmtNode *>(node);
    return containsIdentifierNamed(forNode->init.get(), name) ||
           containsIdentifierNamed(forNode->condition.get(), name) ||
           containsIdentifierNamed(forNode->increment.get(), name) ||
           containsIdentifierNamed(forNode->body.get(), name);
  }
  if (node->type == ASTNodeType::ForInStmt) {
    const auto *forInNode = static_cast<const ForInStmtNode *>(node);
    return containsIdentifierNamed(forInNode->iterable.get(), name) ||
           containsIdentifierNamed(forInNode->body.get(), name);
  }
  if (node->type == ASTNodeType::MatchStmt ||
      node->type == ASTNodeType::MatchExpr) {
    const auto &expressions =
        node->type == ASTNodeType::MatchStmt
            ? static_cast<const MatchStmtNode *>(node)->expressions
            : static_cast<const MatchExprNode *>(node)->expressions;
    const auto &arms = node->type == ASTNodeType::MatchStmt
                           ? static_cast<const MatchStmtNode *>(node)->arms
                           : static_cast<const MatchExprNode *>(node)->arms;
    for (const auto &expression : expressions) {
      if (containsIdentifierNamed(expression.get(), name)) {
        return true;
      }
    }
    for (const auto &arm : arms) {
      if (containsIdentifierNamed(arm.get(), name)) {
        return true;
      }
    }
    return false;
  }
  if (node->type == ASTNodeType::MatchArm) {
    const auto *arm = static_cast<const MatchArmNode *>(node);
    for (const auto &patternExpr : arm->patternExprs) {
      if (containsIdentifierNamed(patternExpr.get(), name)) {
        return true;
      }
    }
    return containsIdentifierNamed(arm->body.get(), name) ||
           containsIdentifierNamed(arm->valueExpr.get(), name);
  }
  if (node->type == ASTNodeType::ReturnStmt) {
    return containsIdentifierNamed(
        static_cast<const ReturnStmtNode *>(node)->value.get(), name);
  }
  if (node->type == ASTNodeType::BindingDecl) {
    const auto *binding = static_cast<const BindingDeclNode *>(node);
    return containsIdentifierNamed(binding->target.get(), name) ||
           containsIdentifierNamed(binding->value.get(), name) ||
           containsIdentifierNamed(binding->typeAnnotation.get(), name);
  }
  if (node->type == ASTNodeType::CastStmt) {
    const auto *castNode = static_cast<const CastStmtNode *>(node);
    if (containsIdentifierNamed(castNode->target.get(), name)) {
      return true;
    }
    for (const auto &step : castNode->steps) {
      if (containsIdentifierNamed(step.typeSpec.get(), name)) {
        return true;
      }
    }
    return false;
  }
  if (node->type == ASTNodeType::CallExpr) {
    const auto *call = static_cast<const CallExprNode *>(node);
    if (containsIdentifierNamed(call->callee.get(), name)) {
      return true;
    }
    for (const auto &arg : call->arguments) {
      if (containsIdentifierNamed(arg.get(), name)) {
        return true;
      }
    }
    return false;
  }
  if (node->type == ASTNodeType::MemberAccessExpr) {
    return containsIdentifierNamed(
        static_cast<const MemberAccessNode *>(node)->object.get(), name);
  }
  if (node->type == ASTNodeType::IndexExpr) {
    const auto *index = static_cast<const IndexExprNode *>(node);
    if (containsIdentifierNamed(index->object.get(), name)) {
      return true;
    }
    for (const auto &idx : index->indices) {
      if (containsIdentifierNamed(idx.get(), name)) {
        return true;
      }
    }
    return false;
  }
  if (node->type == ASTNodeType::SliceExpr) {
    const auto *slice = static_cast<const SliceExprNode *>(node);
    return containsIdentifierNamed(slice->object.get(), name) ||
           containsIdentifierNamed(slice->start.get(), name) ||
           containsIdentifierNamed(slice->end.get(), name);
  }
  if (node->type == ASTNodeType::TypeofExpr) {
    return containsIdentifierNamed(
        static_cast<const TypeofExprNode *>(node)->expression.get(), name);
  }
  if (node->type == ASTNodeType::BinaryExpr) {
    const auto *binary = static_cast<const BinaryExprNode *>(node);
    return containsIdentifierNamed(binary->left.get(), name) ||
           containsIdentifierNamed(binary->right.get(), name);
  }
  if (node->type == ASTNodeType::UnaryExpr) {
    return containsIdentifierNamed(
        static_cast<const UnaryExprNode *>(node)->operand.get(), name);
  }
  if (node->type == ASTNodeType::TryStmt) {
    const auto *tryNode = static_cast<const TryStmtNode *>(node);
    if (containsIdentifierNamed(tryNode->tryBlock.get(), name) ||
        containsIdentifierNamed(tryNode->finallyBlock.get(), name)) {
      return true;
    }
    for (const auto &catchNode : tryNode->catchClauses) {
      if (containsIdentifierNamed(catchNode.get(), name)) {
        return true;
      }
    }
    return false;
  }
  if (node->type == ASTNodeType::CatchClause) {
    const auto *catchNode = static_cast<const CatchClauseNode *>(node);
    return containsIdentifierNamed(catchNode->errorType.get(), name) ||
           containsIdentifierNamed(catchNode->body.get(), name);
  }
  if (node->type == ASTNodeType::ThrowStmt) {
    return containsIdentifierNamed(
        static_cast<const ThrowStmtNode *>(node)->value.get(), name);
  }
  return false;
}

void analyzeInlineCanvasMethod(SemanticDriver &driver, MethodDeclNode *method,
                               bool injectCommandList) {
  if (method == nullptr) {
    return;
  }

  AnalysisContext &context = driver.context();
  const sema_detail::FlowAnalyzer::Snapshot outerFlowState =
      driver.flow().snapshot();

  context.enterScope(method->name);
  driver.flow().enterScope();
  if (injectCommandList) {
    context.enterGraphicsFrame();
    NTypePtr commandListType = context.resolveType("CommandList");
    if (commandListType->isError() || commandListType->isUnknown()) {
      commandListType = NType::makeClass("CommandList");
    }
    if (Symbol *cmdSymbol =
            context.defineSymbol(context.currentScope(), "cmd",
                                 Symbol("cmd", SymbolKind::Parameter,
                                        commandListType))) {
      driver.flow().declareSymbol(cmdSymbol, true, nullptr, commandListType);
    }
  }

  if (method->body != nullptr) {
    context.recordScopeSnapshot(method->body->location);
    if (method->body->type == ASTNodeType::Block) {
      driver.visitBlock(static_cast<BlockNode *>(method->body.get()));
    } else {
      driver.inferExpression(method->body.get());
    }
  }

  driver.flow().leaveScope();
  if (injectCommandList) {
    context.leaveGraphicsFrame();
  }
  driver.flow().restore(outerFlowState);
  context.leaveScope();
  if (method->body != nullptr) {
    context.recordScopeSnapshot(nodeEndLocation(method));
  }
}

} // namespace

GraphicsAnalyzer::GraphicsAnalyzer(SemanticDriver &driver) : m_driver(driver) {}

void GraphicsAnalyzer::visitCanvasStmt(CanvasStmtNode *node) {
  if (node == nullptr) {
    return;
  }
  AnalysisContext &context = m_driver.context();
  if (node->windowExpr == nullptr) {
    context.error(node->location, "canvas requires a window expression");
    return;
  }
  m_driver.inferExpression(node->windowExpr.get());

  struct CanvasEventResolution {
    CanvasEventHandlerNode *inlineHandler = nullptr;
    CanvasEventHandlerNode *externalHandler = nullptr;
  };

  std::unordered_map<int, CanvasEventResolution> resolvedHandlers;
  bool hasOnFrame = false;
  for (auto &handlerNode : node->handlers) {
    if (handlerNode == nullptr ||
        handlerNode->type != ASTNodeType::CanvasEventHandler) {
      context.error(node->location, "Invalid canvas event handler entry");
      continue;
    }

    auto *handler = static_cast<CanvasEventHandlerNode *>(handlerNode.get());
    CanvasEventKind eventKind = handler->eventKind;
    if (eventKind == CanvasEventKind::Unknown) {
      eventKind = canvasEventFromName(handler->eventName);
    }
    if (eventKind == CanvasEventKind::Unknown) {
      context.error(handler->location, "Unknown canvas event handler");
      continue;
    }

    CanvasEventResolution &resolution =
        resolvedHandlers[static_cast<int>(eventKind)];
    if (handler->isExternalBinding) {
      if (resolution.externalHandler != nullptr) {
        context.error(handler->location,
                      "Duplicate external canvas event handler: " +
                          handler->eventName);
      } else {
        resolution.externalHandler = handler;
      }
    } else {
      if (resolution.inlineHandler != nullptr) {
        context.error(handler->location,
                      "Duplicate inline canvas event handler: " +
                          handler->eventName);
      } else {
        resolution.inlineHandler = handler;
      }
    }

    if (eventKind == CanvasEventKind::OnFrame) {
      hasOnFrame = true;
    }
  }

  for (const auto &entry : resolvedHandlers) {
    const CanvasEventResolution &resolution = entry.second;
    if (resolution.externalHandler != nullptr) {
      auto *handler = resolution.externalHandler;
      if (handler->externalMethodName.empty()) {
        context.error(handler->location,
                      "canvas event binding requires a method name");
      } else {
        Symbol *sym = context.currentScope()->lookup(handler->externalMethodName);
        if (sym == nullptr || sym->kind != SymbolKind::Method) {
          context.error(handler->location,
                        "Undefined canvas event method: " +
                            handler->externalMethodName);
        } else if (sym->type != nullptr &&
                   sym->type->kind == TypeKind::Method) {
          if (!sym->type->paramTypes.empty()) {
            context.error(handler->location,
                          "Canvas event methods must be parameterless: " +
                              handler->externalMethodName);
          }
          if (!sym->type->returnType->isVoid() &&
              !sym->type->returnType->isUnknown() &&
              !sym->type->returnType->isAuto()) {
            context.error(handler->location,
                          "Canvas event methods must return void: " +
                              handler->externalMethodName);
          }
        }
      }
    }

    if (resolution.inlineHandler == nullptr) {
      continue;
    }

    auto *handler = resolution.inlineHandler;
    if (handler->handlerMethod == nullptr ||
        handler->handlerMethod->type != ASTNodeType::MethodDecl) {
      context.error(handler->location,
                    "Inline canvas event handler must declare a method body");
      continue;
    }

    auto *method = static_cast<MethodDeclNode *>(handler->handlerMethod.get());
    if (!method->parameters.empty()) {
      context.error(method->location,
                    "Canvas event methods must be parameterless: " +
                        method->name);
    }
    NTypePtr retType = context.resolveType(method->returnType.get());
    if (!retType->isVoid() && !retType->isUnknown() && !retType->isAuto()) {
      context.error(method->location,
                    "Canvas event methods must return void: " + method->name);
    }

    Symbol methodSymbol(method->name, SymbolKind::Method,
                        NType::makeMethod(NType::makeVoid(), {}));
    methodSymbol.signatureKey = method->name;
    context.defineSymbol(context.currentScope(), method->name,
                         std::move(methodSymbol), &method->location,
                         symbolNameLength(method->name));
    context.registerCallableParamNames(method->name, {});
    context.registerCallableSignature(method->name, {}, "void");
    analyzeInlineCanvasMethod(m_driver, method,
                              handler->eventKind == CanvasEventKind::OnFrame);
  }

  if (!hasOnFrame) {
    context.error(node->location, "canvas requires an OnFrame handler");
  }
}

void GraphicsAnalyzer::visitShaderDecl(ShaderDeclNode *node) {
  if (node == nullptr) {
    return;
  }

  AnalysisContext &context = m_driver.context();
  context.enterScope("shader:" + node->name);
  m_driver.flow().enterScope();
  context.recordScopeSnapshot(node->location);

  NTypePtr mvpType = context.resolveType("Matrix4");
  if (mvpType->isError() || mvpType->isUnknown()) {
    mvpType = NType::makeDynamic();
  }
  if (Symbol *mvpSymbol =
          context.defineSymbol(context.currentScope(), "MVP",
                               Symbol("MVP", SymbolKind::Variable, mvpType))) {
    m_driver.flow().declareSymbol(mvpSymbol, true, nullptr, mvpType);
  }

  std::unordered_map<std::string, std::string> uniformTypes;

  for (auto &uniformNode : node->uniforms) {
    if (uniformNode == nullptr ||
        uniformNode->type != ASTNodeType::BindingDecl) {
      context.error(node->location, "Shader uniform must be a field-style binding");
      continue;
    }

    auto *uniform = static_cast<BindingDeclNode *>(uniformNode.get());
    m_driver.rules().validateVariableName(uniform->name, uniform->location,
                                          uniform->isConst);

    NTypePtr uniformType = context.resolveType(uniform->typeAnnotation.get());
    if ((uniformType->isAuto() || uniformType->isUnknown()) && uniform->value) {
      uniformType = m_driver.inferExpression(uniform->value.get());
    }
    if (uniformType->isAuto() || uniformType->isUnknown()) {
      context.error(uniform->location,
                    "Shader uniform '" + uniform->name +
                        "' requires an explicit type");
      uniformType = NType::makeDynamic();
    }

    Symbol *uniformSymbol = context.defineSymbol(
        context.currentScope(), uniform->name,
        Symbol(uniform->name, SymbolKind::Variable, uniformType),
        &uniform->location, symbolNameLength(uniform->name));
    if (uniformSymbol == nullptr) {
      context.error(uniform->location,
                    "Duplicate shader uniform: " + uniform->name);
    } else {
      context.registerShaderBinding(node->name, uniform->name,
                                    uniformType->toString());
      uniformTypes[uniform->name] = uniformType->toString();
      m_driver.flow().declareSymbol(uniformSymbol, true, uniform->value.get(),
                                    uniformType);
    }
  }

  bool hasVertexStage = false;
  bool hasFragmentStage = false;
  std::unordered_set<std::string> passedNames;
  std::unordered_set<std::string> fragmentParams;
  MethodDeclNode *vertexMethod = nullptr;
  MethodDeclNode *fragmentMethod = nullptr;

  for (auto &stageNode : node->stages) {
    if (stageNode == nullptr || stageNode->type != ASTNodeType::ShaderStage) {
      context.error(node->location, "Invalid shader stage node");
      continue;
    }

    auto *stage = static_cast<ShaderStageNode *>(stageNode.get());
    if (stage->methodDecl == nullptr ||
        stage->methodDecl->type != ASTNodeType::MethodDecl) {
      context.error(stage->location, "Shader stage requires a method declaration");
      continue;
    }

    auto *stageMethod = static_cast<MethodDeclNode *>(stage->methodDecl.get());
    if (stage->stageKind == ShaderStageKind::Vertex) {
      if (hasVertexStage) {
        context.error(stage->location, "Shader has duplicate Vertex stage");
      }
      hasVertexStage = true;
      vertexMethod = stageMethod;
      if (stageMethod->body != nullptr &&
          stageMethod->body->type == ASTNodeType::Block) {
        auto *body = static_cast<BlockNode *>(stageMethod->body.get());
        for (auto &stmt : body->statements) {
          if (stmt->type == ASTNodeType::ShaderPassStmt) {
            passedNames.insert(
                static_cast<ShaderPassStmtNode *>(stmt.get())->varyingName);
          }
        }
      }
    } else if (stage->stageKind == ShaderStageKind::Fragment) {
      if (hasFragmentStage) {
        context.error(stage->location, "Shader has duplicate Fragment stage");
      }
      hasFragmentStage = true;
      fragmentMethod = stageMethod;
      for (const auto &param : stageMethod->parameters) {
        fragmentParams.insert(param.name);
      }
      if (stageMethod->body != nullptr &&
          stageMethod->body->type == ASTNodeType::Block) {
        auto *body = static_cast<BlockNode *>(stageMethod->body.get());
        for (auto &stmt : body->statements) {
          if (stmt->type == ASTNodeType::ShaderPassStmt) {
            context.error(stmt->location,
                          "pass statements are only allowed in Vertex stage");
          }
        }
      }
    }

    m_driver.visitMethodDecl(stageMethod);
  }

  if (!hasVertexStage) {
    context.error(node->location, "Shader must define a Vertex stage");
  }
  if (!hasFragmentStage) {
    context.error(node->location, "Shader must define a Fragment stage");
  }

  for (const auto &passed : passedNames) {
    if (fragmentParams.find(passed) == fragmentParams.end()) {
      context.error(node->location,
                    "Fragment stage is missing parameter for passed varying '" +
                        passed + "'");
    }
  }

  std::unordered_map<std::string, std::string> passedTypes;
  bool usesMvp = false;
  if (vertexMethod != nullptr) {
    (void)validateVertexShaderStage(context, vertexMethod, uniformTypes,
                                    &passedTypes, &usesMvp);
  }
  if (fragmentMethod != nullptr) {
    validateFragmentShaderStage(context, fragmentMethod, uniformTypes,
                                passedTypes, &usesMvp);
  }

  for (auto &methodNode : node->methods) {
    if (methodNode == nullptr || methodNode->type != ASTNodeType::MethodDecl) {
      context.error(node->location, "Invalid shader descriptor method node");
      continue;
    }
    auto *method = static_cast<MethodDeclNode *>(methodNode.get());
    validateCpuSideDescriptorMethod(method, node->name);
    registerShaderDescriptorMethod(method, node->name);
    m_driver.visitMethodDecl(method);
  }

  m_driver.flow().leaveScope();
  context.leaveScope();
  if (!node->stages.empty()) {
    context.recordScopeSnapshot(nodeEndLocation(node->stages.back().get()));
  } else if (!node->uniforms.empty()) {
    context.recordScopeSnapshot(nodeEndLocation(node->uniforms.back().get()));
  } else {
    context.recordScopeSnapshot(node->location);
  }
}

void GraphicsAnalyzer::registerShaderDescriptorMethod(
    MethodDeclNode *method, const std::string &shaderName) {
  if (method == nullptr || shaderName.empty()) {
    return;
  }

  AnalysisContext &context = m_driver.context();
  std::vector<NTypePtr> parameterTypes;
  std::vector<std::string> parameterNames;
  std::vector<CallableParameterInfo> signatureParameters;
  parameterTypes.reserve(method->parameters.size());
  parameterNames.reserve(method->parameters.size());
  signatureParameters.reserve(method->parameters.size());
  for (const auto &param : method->parameters) {
    NTypePtr paramType = context.resolveType(param.typeSpec.get());
    parameterTypes.push_back(paramType);
    parameterNames.push_back(param.name);
    signatureParameters.push_back({param.name, typeDisplayName(paramType)});
  }

  NTypePtr returnType = context.resolveType(method->returnType.get());
  const std::string signatureKey = shaderName + "." + method->name;
  Symbol methodSymbol(signatureKey, SymbolKind::Method,
                      NType::makeMethod(returnType, std::move(parameterTypes)));
  methodSymbol.signatureKey = signatureKey;
  methodSymbol.isPublic = method->access == AccessModifier::Public;
  context.defineSymbol(context.globalScope(), signatureKey, std::move(methodSymbol),
                       &method->location, symbolNameLength(method->name));
  context.registerCallableParamNames(signatureKey, std::move(parameterNames));
  context.registerCallableSignature(signatureKey, std::move(signatureParameters),
                                    typeDisplayName(returnType));
}

void GraphicsAnalyzer::validateCpuSideDescriptorMethod(
    MethodDeclNode *method, const std::string &shaderName) {
  if (method == nullptr) {
    return;
  }

  AnalysisContext &context = m_driver.context();
  if (containsShaderPassStatement(method->body.get())) {
    context.error(method->location,
                  "Descriptor method '" + shaderName + "." + method->name +
                      "' cannot use 'pass' - only GPU-side Vertex/Fragment "
                      "stages can use 'pass'");
  }
  if (containsIdentifierNamed(method->body.get(), "MVP")) {
    context.error(method->location,
                  "Descriptor method '" + shaderName + "." + method->name +
                      "' cannot access MVP. MVP is only available inside "
                      "GPU-side shader stages");
  }
}

void GraphicsAnalyzer::visitGpuBlock(GpuBlockNode *node) {
  if (node == nullptr || node->body == nullptr) {
    return;
  }
  if (node->body->type == ASTNodeType::Block) {
    m_driver.visitBlock(static_cast<BlockNode *>(node->body.get()));
    return;
  }
  m_driver.inferExpression(node->body.get());
}

} // namespace neuron::sema_detail
