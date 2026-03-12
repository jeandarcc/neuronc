#include "neuronc/parser/Parser.h"

namespace neuron {

ASTNodePtr Parser::cloneAstNode(const ASTNode *node) const {
  if (node == nullptr) {
    return nullptr;
  }

  switch (node->type) {
  case ASTNodeType::IntLiteral:
    return std::make_unique<IntLiteralNode>(
        static_cast<const IntLiteralNode *>(node)->value, node->location);
  case ASTNodeType::FloatLiteral:
    return std::make_unique<FloatLiteralNode>(
        static_cast<const FloatLiteralNode *>(node)->value, node->location);
  case ASTNodeType::StringLiteral:
    return std::make_unique<StringLiteralNode>(
        static_cast<const StringLiteralNode *>(node)->value, node->location);
  case ASTNodeType::BoolLiteral:
    return std::make_unique<BoolLiteralNode>(
        static_cast<const BoolLiteralNode *>(node)->value, node->location);
  case ASTNodeType::NullLiteral:
    return std::make_unique<NullLiteralNode>(node->location);
  case ASTNodeType::Identifier:
    return std::make_unique<IdentifierNode>(
        static_cast<const IdentifierNode *>(node)->name, node->location);
  case ASTNodeType::Error:
    return std::make_unique<ErrorNode>(
        static_cast<const ErrorNode *>(node)->message, node->location);
  case ASTNodeType::TypeSpec: {
    const auto *typeNode = static_cast<const TypeSpecNode *>(node);
    auto clone = std::make_unique<TypeSpecNode>(typeNode->typeName,
                                                typeNode->location);
    for (const auto &arg : typeNode->genericArgs) {
      clone->genericArgs.push_back(cloneAstNode(arg.get()));
    }
    return clone;
  }
  case ASTNodeType::BinaryExpr: {
    const auto *binary = static_cast<const BinaryExprNode *>(node);
    return std::make_unique<BinaryExprNode>(
        binary->op, cloneAstNode(binary->left.get()),
        cloneAstNode(binary->right.get()), binary->location);
  }
  case ASTNodeType::UnaryExpr: {
    const auto *unary = static_cast<const UnaryExprNode *>(node);
    return std::make_unique<UnaryExprNode>(
        unary->op, cloneAstNode(unary->operand.get()), unary->isPrefix,
        unary->location);
  }
  case ASTNodeType::CallExpr: {
    const auto *call = static_cast<const CallExprNode *>(node);
    std::vector<ASTNodePtr> args;
    args.reserve(call->arguments.size());
    for (const auto &arg : call->arguments) {
      args.push_back(cloneAstNode(arg.get()));
    }
    auto clone = std::make_unique<CallExprNode>(
        cloneAstNode(call->callee.get()), std::move(args), call->argumentLabels,
        call->location);
    clone->isFusionChain = call->isFusionChain;
    clone->fusionCallNames = call->fusionCallNames;
    return clone;
  }
  case ASTNodeType::InputExpr: {
    const auto *input = static_cast<const InputExprNode *>(node);
    auto clone = std::make_unique<InputExprNode>(input->location);
    for (const auto &typeArg : input->typeArguments) {
      clone->typeArguments.push_back(cloneAstNode(typeArg.get()));
    }
    for (const auto &promptArg : input->promptArguments) {
      clone->promptArguments.push_back(cloneAstNode(promptArg.get()));
    }
    for (const auto &stage : input->stages) {
      std::vector<ASTNodePtr> args;
      args.reserve(stage.arguments.size());
      for (const auto &arg : stage.arguments) {
        args.push_back(cloneAstNode(arg.get()));
      }
      clone->stages.emplace_back(stage.method, std::move(args), stage.location);
    }
    return clone;
  }
  case ASTNodeType::MemberAccessExpr: {
    const auto *member = static_cast<const MemberAccessNode *>(node);
    return std::make_unique<MemberAccessNode>(
        cloneAstNode(member->object.get()), member->member, member->location,
        member->memberLocation);
  }
  case ASTNodeType::IndexExpr: {
    const auto *index = static_cast<const IndexExprNode *>(node);
    std::vector<ASTNodePtr> indices;
    indices.reserve(index->indices.size());
    for (const auto &item : index->indices) {
      indices.push_back(cloneAstNode(item.get()));
    }
    return std::make_unique<IndexExprNode>(
        cloneAstNode(index->object.get()), std::move(indices), index->location);
  }
  case ASTNodeType::SliceExpr: {
    const auto *slice = static_cast<const SliceExprNode *>(node);
    return std::make_unique<SliceExprNode>(
        cloneAstNode(slice->object.get()), cloneAstNode(slice->start.get()),
        cloneAstNode(slice->end.get()), slice->location);
  }
  case ASTNodeType::TypeofExpr: {
    const auto *typeofNode = static_cast<const TypeofExprNode *>(node);
    return std::make_unique<TypeofExprNode>(
        cloneAstNode(typeofNode->expression.get()), typeofNode->location);
  }
  case ASTNodeType::IncrementStmt:
    return std::make_unique<IncrementStmtNode>(
        static_cast<const IncrementStmtNode *>(node)->variable, node->location);
  case ASTNodeType::DecrementStmt:
    return std::make_unique<DecrementStmtNode>(
        static_cast<const DecrementStmtNode *>(node)->variable, node->location);
  case ASTNodeType::MatchExpr: {
    const auto *matchExpr = static_cast<const MatchExprNode *>(node);
    auto clone = std::make_unique<MatchExprNode>(matchExpr->location);
    for (const auto &expr : matchExpr->expressions) {
      clone->expressions.push_back(cloneAstNode(expr.get()));
    }
    for (const auto &armNode : matchExpr->arms) {
      const auto *arm = static_cast<const MatchArmNode *>(armNode.get());
      std::vector<ASTNodePtr> patterns;
      patterns.reserve(arm->patternExprs.size());
      for (const auto &pattern : arm->patternExprs) {
        patterns.push_back(cloneAstNode(pattern.get()));
      }
      clone->arms.push_back(std::make_unique<MatchArmNode>(
          std::move(patterns), cloneAstNode(arm->body.get()),
          cloneAstNode(arm->valueExpr.get()), arm->isDefault, arm->location));
    }
    return clone;
  }
  default:
    return nullptr;
  }
}

ASTNodePtr Parser::parseTypeSpec() {
  auto loc = current().location;
  auto name = expect(TokenType::Identifier, "Expected type name");
  auto typeSpec = std::make_unique<TypeSpecNode>(name.value, loc);

  if (match(TokenType::Less)) {
    while (!check(TokenType::Greater) && !isAtEnd()) {
      typeSpec->genericArgs.push_back(parsePrimary());
      if (!match(TokenType::Comma))
        break;
    }
    expect(TokenType::Greater, "Expected '>' after generic arguments");
  }

  return typeSpec;
}

CastStepNode Parser::parseCastStep(bool *outPipelineNullable,
                                   bool allowPipelineNullable) {
  auto loc = current().location;
  bool allowNullOnFailure = false;
  ASTNodePtr typeSpec = nullptr;

  if (match(TokenType::LeftParen)) {
    if (match(TokenType::Maybe)) {
      allowNullOnFailure = true;
    }
    typeSpec = parseTypeSpec();
    expect(TokenType::RightParen, "Expected ')' after cast step");
  } else {
    if (allowPipelineNullable && match(TokenType::Maybe)) {
      if (outPipelineNullable) {
        *outPipelineNullable = true;
      }
      allowNullOnFailure = true;
    } else if (match(TokenType::Maybe)) {
      allowNullOnFailure = true;
    }
    typeSpec = parseTypeSpec();
  }

  return CastStepNode(std::move(typeSpec), allowNullOnFailure, loc);
}

std::vector<ParameterNode> Parser::parseParameterList() {
  expect(TokenType::LeftParen, "Expected '('");
  std::vector<ParameterNode> params;

  if (!check(TokenType::RightParen)) {
    do {
      ParameterNode param;
      Token nameToken = expect(TokenType::Identifier, "Expected parameter name");
      param.name = nameToken.value;
      param.location = nameToken.location;
      expect(TokenType::As, "Expected 'as' after parameter name");
      if (check(TokenType::Method)) {
        auto methodLoc = current().location;
        advance();
        param.typeSpec = std::make_unique<TypeSpecNode>("method", methodLoc);
      } else {
        param.typeSpec = parseTypeSpec();
      }
      params.push_back(std::move(param));
    } while (match(TokenType::Comma));
  }

  expect(TokenType::RightParen, "Expected ')'");
  return params;
}

ParsedCallArguments Parser::parseArgumentList() {
  expect(TokenType::LeftParen, "Expected '('");
  ParsedCallArguments args;

  if (!check(TokenType::RightParen)) {
    do {
      std::string label;
      if (check(TokenType::Identifier) && peek().type == TokenType::Colon) {
        label = advance().value;
        advance();
      }
      args.values.push_back(parseExpression());
      args.labels.push_back(std::move(label));
    } while (match(TokenType::Comma));
  }

  expect(TokenType::RightParen, "Expected ')'");
  return args;
}

} // namespace neuron

