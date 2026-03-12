#include "neuronc/parser/Parser.h"

#include <cctype>

namespace neuron {

namespace {

bool isPascalCaseIdentifier(const std::string &name) {
  if (name.empty()) {
    return false;
  }
  const unsigned char first = static_cast<unsigned char>(name.front());
  return std::isupper(first) != 0;
}

ASTNodePtr cloneFusionTarget(const ASTNode *node) {
  if (node == nullptr) {
    return nullptr;
  }

  switch (node->type) {
  case ASTNodeType::Identifier: {
    const auto *identifier = static_cast<const IdentifierNode *>(node);
    return std::make_unique<IdentifierNode>(identifier->name,
                                            identifier->location);
  }
  case ASTNodeType::TypeSpec: {
    const auto *typeSpec = static_cast<const TypeSpecNode *>(node);
    auto clone =
        std::make_unique<TypeSpecNode>(typeSpec->typeName, typeSpec->location);
    for (const auto &arg : typeSpec->genericArgs) {
      ASTNodePtr clonedArg = cloneFusionTarget(arg.get());
      if (clonedArg == nullptr) {
        return nullptr;
      }
      clone->genericArgs.push_back(std::move(clonedArg));
    }
    return clone;
  }
  case ASTNodeType::MemberAccessExpr: {
    const auto *member = static_cast<const MemberAccessNode *>(node);
    ASTNodePtr objectClone = cloneFusionTarget(member->object.get());
    if (objectClone == nullptr) {
      return nullptr;
    }
    return std::make_unique<MemberAccessNode>(std::move(objectClone),
                                              member->member,
                                              member->location,
                                              member->memberLocation);
  }
  case ASTNodeType::IntLiteral: {
    const auto *literal = static_cast<const IntLiteralNode *>(node);
    return std::make_unique<IntLiteralNode>(literal->value, literal->location);
  }
  case ASTNodeType::FloatLiteral: {
    const auto *literal = static_cast<const FloatLiteralNode *>(node);
    return std::make_unique<FloatLiteralNode>(literal->value,
                                              literal->location);
  }
  case ASTNodeType::StringLiteral: {
    const auto *literal = static_cast<const StringLiteralNode *>(node);
    return std::make_unique<StringLiteralNode>(literal->value,
                                               literal->location);
  }
  case ASTNodeType::BoolLiteral: {
    const auto *literal = static_cast<const BoolLiteralNode *>(node);
    return std::make_unique<BoolLiteralNode>(literal->value, literal->location);
  }
  case ASTNodeType::NullLiteral:
    return std::make_unique<NullLiteralNode>(node->location);
  default:
    return nullptr;
  }
}

std::string resolveFusionCallName(const ASTNode *node) {
  if (node == nullptr) {
    return "";
  }
  if (node->type == ASTNodeType::Identifier) {
    return static_cast<const IdentifierNode *>(node)->name;
  }
  if (node->type == ASTNodeType::TypeSpec) {
    return static_cast<const TypeSpecNode *>(node)->typeName;
  }
  if (node->type == ASTNodeType::MemberAccessExpr) {
    const auto *member = static_cast<const MemberAccessNode *>(node);
    const std::string base = resolveFusionCallName(member->object.get());
    if (base.empty()) {
      return "";
    }
    return base + "." + member->member;
  }
  return "";
}

ASTNodePtr cloneFusionQualifier(const ASTNode *node) {
  if (node == nullptr || node->type != ASTNodeType::MemberAccessExpr) {
    return nullptr;
  }
  const auto *member = static_cast<const MemberAccessNode *>(node);
  return cloneFusionTarget(member->object.get());
}

bool isFusionBaseCallee(const ASTNode *node) {
  if (node == nullptr) {
    return false;
  }
  if (node->type == ASTNodeType::Identifier) {
    return isPascalCaseIdentifier(
        static_cast<const IdentifierNode *>(node)->name);
  }
  if (node->type == ASTNodeType::MemberAccessExpr) {
    const auto *member = static_cast<const MemberAccessNode *>(node);
    return isPascalCaseIdentifier(member->member) &&
           cloneFusionTarget(member->object.get()) != nullptr;
  }
  return false;
}

struct InputCallView {
  const CallExprNode *baseCall = nullptr;
  const TypeSpecNode *baseTypeSpec = nullptr;
  std::vector<const CallExprNode *> stageCalls;
  std::vector<std::string> stageMethods;
};

bool collectInputCallView(const CallExprNode *node, InputCallView *view) {
  if (node == nullptr || view == nullptr || node->callee == nullptr) {
    return false;
  }

  if (node->callee->type == ASTNodeType::Identifier) {
    const auto *identifier = static_cast<const IdentifierNode *>(node->callee.get());
    if (identifier->name != "Input") {
      return false;
    }
    view->baseCall = node;
    return true;
  }

  if (node->callee->type == ASTNodeType::TypeSpec) {
    const auto *typeSpec = static_cast<const TypeSpecNode *>(node->callee.get());
    if (typeSpec->typeName != "Input") {
      return false;
    }
    view->baseCall = node;
    view->baseTypeSpec = typeSpec;
    return true;
  }

  if (node->callee->type != ASTNodeType::MemberAccessExpr) {
    return false;
  }

  const auto *member = static_cast<const MemberAccessNode *>(node->callee.get());
  if (member->object == nullptr ||
      member->object->type != ASTNodeType::CallExpr) {
    return false;
  }

  if (!collectInputCallView(
          static_cast<const CallExprNode *>(member->object.get()), view)) {
    return false;
  }

  view->stageCalls.push_back(node);
  view->stageMethods.push_back(member->member);
  return true;
}

ASTNodePtr buildFusionCallChain(ASTNodePtr baseCallee,
                                std::vector<Token> chainedStages,
                                ParsedCallArguments args,
                                const SourceLocation &callLoc) {
  std::vector<std::string> fusionCallNames;
  const std::string baseCallName = resolveFusionCallName(baseCallee.get());
  if (!baseCallName.empty()) {
    fusionCallNames.push_back(baseCallName);
  }

  ASTNodePtr qualifierTemplate = cloneFusionQualifier(baseCallee.get());
  ASTNodePtr currentCall = std::make_unique<CallExprNode>(
      std::move(baseCallee), std::move(args.values), std::move(args.labels),
      callLoc);

  for (const auto &stage : chainedStages) {
    ASTNodePtr stageCallee;
    if (qualifierTemplate != nullptr) {
      ASTNodePtr qualifierClone = cloneFusionTarget(qualifierTemplate.get());
      if (qualifierClone == nullptr) {
        return currentCall;
      }
      stageCallee = std::make_unique<MemberAccessNode>(std::move(qualifierClone),
                                                       stage.value,
                                                       stage.location,
                                                       stage.location);
    } else {
      stageCallee =
          std::make_unique<IdentifierNode>(stage.value, stage.location);
    }

    const std::string stageCallName = resolveFusionCallName(stageCallee.get());
    if (!stageCallName.empty()) {
      fusionCallNames.push_back(stageCallName);
    }

    std::vector<ASTNodePtr> stageArgs;
    stageArgs.push_back(std::move(currentCall));
    currentCall = std::make_unique<CallExprNode>(std::move(stageCallee),
                                                 std::move(stageArgs), callLoc);
  }

  if (currentCall != nullptr && currentCall->type == ASTNodeType::CallExpr) {
    auto *call = static_cast<CallExprNode *>(currentCall.get());
    call->isFusionChain = true;
    call->fusionCallNames = std::move(fusionCallNames);
  }
  return currentCall;
}

} // namespace

ASTNodePtr Parser::parsePostfix() {
  auto expr = parsePrimary();

  while (true) {
    if (check(TokenType::LeftParen)) {
      auto loc = current().location;
      auto args = parseArgumentList();
      expr = std::make_unique<CallExprNode>(std::move(expr),
                                            std::move(args.values),
                                            std::move(args.labels), loc);
    } else if (check(TokenType::Dot)) {
      auto loc = current().location;
      advance();
      auto member =
          expect(TokenType::Identifier, "Expected member name after '.'");
      expr = std::make_unique<MemberAccessNode>(std::move(expr), member.value,
                                                loc, member.location);
    } else if (check(TokenType::LeftBracket)) {
      auto loc = current().location;
      advance();
      auto first = parseExpression();
      if (match(TokenType::DotDot)) {
        auto end = parseExpression();
        expect(TokenType::RightBracket, "Expected ']' after slice");
        expr = std::make_unique<SliceExprNode>(std::move(expr), std::move(first),
                                               std::move(end), loc);
      } else {
        std::vector<ASTNodePtr> indices;
        indices.push_back(std::move(first));
        while (match(TokenType::Comma)) {
          indices.push_back(parseExpression());
        }
        expect(TokenType::RightBracket, "Expected ']' after index");
        expr = std::make_unique<IndexExprNode>(std::move(expr),
                                               std::move(indices), loc);
      }
    } else if (check(TokenType::Minus) && isFusionBaseCallee(expr.get())) {
      std::vector<Token> chainedStages;
      std::size_t offset = 0;
      while (lookahead(offset).type == TokenType::Minus) {
        const Token &stageToken = lookahead(offset + 1);
        if (stageToken.type != TokenType::Identifier ||
            !isPascalCaseIdentifier(stageToken.value)) {
          chainedStages.clear();
          break;
        }
        chainedStages.push_back(stageToken);
        offset += 2;
      }

      if (chainedStages.empty() || lookahead(offset).type != TokenType::LeftParen) {
        break;
      }

      for (std::size_t i = 0; i < chainedStages.size(); ++i) {
        advance();
        advance();
      }

      auto loc = current().location;
      auto args = parseArgumentList();
      expr = buildFusionCallChain(std::move(expr), std::move(chainedStages),
                                  std::move(args), loc);
    } else {
      break;
    }
  }

  if (expr != nullptr && expr->type == ASTNodeType::CallExpr) {
    InputCallView inputView;
    if (collectInputCallView(static_cast<const CallExprNode *>(expr.get()),
                             &inputView)) {
      auto inputExpr = std::make_unique<InputExprNode>(expr->location);
      if (inputView.baseTypeSpec != nullptr) {
        for (const auto &typeArg : inputView.baseTypeSpec->genericArgs) {
          inputExpr->typeArguments.push_back(cloneAstNode(typeArg.get()));
        }
      }
      if (inputView.baseCall != nullptr) {
        for (const auto &promptArg : inputView.baseCall->arguments) {
          inputExpr->promptArguments.push_back(cloneAstNode(promptArg.get()));
        }
      }
      for (std::size_t i = 0; i < inputView.stageCalls.size(); ++i) {
        std::vector<ASTNodePtr> args;
        for (const auto &arg : inputView.stageCalls[i]->arguments) {
          args.push_back(cloneAstNode(arg.get()));
        }
        inputExpr->stages.emplace_back(inputView.stageMethods[i], std::move(args),
                                       inputView.stageCalls[i]->location);
      }
      return inputExpr;
    }
  }

  return expr;
}

ASTNodePtr Parser::parsePrimary() {
  auto loc = current().location;

  if (check(TokenType::IntLiteral)) {
    int64_t val = std::stoll(current().value);
    advance();
    return std::make_unique<IntLiteralNode>(val, loc);
  }
  if (check(TokenType::FloatLiteral)) {
    double val = std::stod(current().value);
    advance();
    return std::make_unique<FloatLiteralNode>(val, loc);
  }
  if (check(TokenType::StringLiteral)) {
    auto val = current().value;
    advance();
    return std::make_unique<StringLiteralNode>(val, loc);
  }
  if (check(TokenType::True)) {
    advance();
    return std::make_unique<BoolLiteralNode>(true, loc);
  }
  if (check(TokenType::False)) {
    advance();
    return std::make_unique<BoolLiteralNode>(false, loc);
  }
  if (check(TokenType::Null)) {
    advance();
    return std::make_unique<NullLiteralNode>(loc);
  }

  if (check(TokenType::Typeof)) {
    advance();
    expect(TokenType::LeftParen, "Expected '(' after 'typeof'");
    auto expr = parseExpression();
    expect(TokenType::RightParen, "Expected ')' after typeof expression");
    return std::make_unique<TypeofExprNode>(std::move(expr), loc);
  }

  if (check(TokenType::Thread)) {
    advance();
    auto args = parseArgumentList();
    auto callee = std::make_unique<IdentifierNode>("thread", loc);
    return std::make_unique<CallExprNode>(std::move(callee),
                                          std::move(args.values),
                                          std::move(args.labels), loc);
  }

  if (check(TokenType::Identifier)) {
    auto name = current().value;
    advance();
    if (check(TokenType::Less)) {
      size_t savedPos = m_pos;
      advance();
      int depth = 1;
      bool isGeneric = false;
      while (!isAtEnd() && depth > 0) {
        if (check(TokenType::Less))
          depth++;
        else if (check(TokenType::Greater)) {
          depth--;
          if (depth == 0) {
            isGeneric = true;
            break;
          }
        } else if (check(TokenType::Semicolon) || check(TokenType::LeftBrace))
          break;
        advance();
      }

      if (isGeneric) {
        m_pos = savedPos;
        auto typeSpec = std::make_unique<TypeSpecNode>(name, loc);
        advance();
        while (!check(TokenType::Greater) && !isAtEnd()) {
          typeSpec->genericArgs.push_back(parsePrimary());
          if (!match(TokenType::Comma))
            break;
        }
        expect(TokenType::Greater, "Expected '>' after generic arguments");
        return typeSpec;
      } else {
        m_pos = savedPos;
      }
    }

    return std::make_unique<IdentifierNode>(name, loc);
  }

  if (check(TokenType::This)) {
    advance();
    return std::make_unique<IdentifierNode>("this", loc);
  }

  if (check(TokenType::Match)) {
    return parseMatchExpr();
  }

  if (match(TokenType::LeftParen)) {
    auto expr = parseExpression();
    expect(TokenType::RightParen, "Expected ')' after expression");
    return expr;
  }

  if (check(TokenType::Method)) {
    return parseMethodDecl("__lambda__", AccessModifier::None, loc);
  }

  if (check(TokenType::ValueOf)) {
    advance();
    auto operand = parsePostfix();
    return std::make_unique<UnaryExprNode>(TokenType::ValueOf,
                                           std::move(operand), true, loc);
  }

  if (check(TokenType::Await)) {
    advance();
    auto expr = parseExpression();
    return std::make_unique<UnaryExprNode>(TokenType::Await, std::move(expr),
                                           true, loc);
  }

  error("Expected expression, got '" + current().value + "'");
  if (!check(TokenType::RightParen) && !check(TokenType::RightBrace) &&
      !check(TokenType::RightBracket) && !check(TokenType::Semicolon) &&
      !check(TokenType::Comma) && !isAtEnd()) {
    advance();
  }
  return std::make_unique<ErrorNode>("missing expression", loc);
}

} // namespace neuron

