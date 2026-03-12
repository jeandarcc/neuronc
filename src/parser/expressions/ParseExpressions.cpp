#include "neuronc/parser/Parser.h"

namespace neuron {

ASTNodePtr Parser::parseExpression() { return parseOr(); }

ASTNodePtr Parser::parseOr() {
  auto left = parseAnd();
  while (check(TokenType::Or)) {
    auto loc = current().location;
    auto op = advance().type;
    auto right = parseAnd();
    left = std::make_unique<BinaryExprNode>(op, std::move(left),
                                            std::move(right), loc);
  }
  return left;
}

ASTNodePtr Parser::parseAnd() {
  auto left = parseEquality();
  while (check(TokenType::And)) {
    auto loc = current().location;
    auto op = advance().type;
    auto right = parseEquality();
    left = std::make_unique<BinaryExprNode>(op, std::move(left),
                                            std::move(right), loc);
  }
  return left;
}

ASTNodePtr Parser::parseEquality() {
  auto left = parseComparison();
  while (check(TokenType::EqualEqual) || check(TokenType::NotEqual)) {
    auto loc = current().location;
    auto op = advance().type;
    auto right = parseComparison();
    left = std::make_unique<BinaryExprNode>(op, std::move(left),
                                            std::move(right), loc);
  }
  return left;
}

ASTNodePtr Parser::parseComparison() {
  auto left = parseAddition();
  while (check(TokenType::Greater) || check(TokenType::Less) ||
         check(TokenType::GreaterEqual) || check(TokenType::LessEqual)) {
    auto loc = current().location;
    auto op = advance().type;
    auto right = parseAddition();
    left = std::make_unique<BinaryExprNode>(op, std::move(left),
                                            std::move(right), loc);
  }
  return left;
}

ASTNodePtr Parser::parseAddition() {
  auto left = parseMultiplication();
  while (check(TokenType::Plus) || check(TokenType::Minus)) {
    auto loc = current().location;
    auto op = advance().type;
    auto right = parseMultiplication();
    left = std::make_unique<BinaryExprNode>(op, std::move(left),
                                            std::move(right), loc);
  }
  return left;
}

ASTNodePtr Parser::parseMultiplication() {
  auto left = parsePower();
  while (check(TokenType::Star) || check(TokenType::Slash) ||
         check(TokenType::At)) {
    auto loc = current().location;
    auto op = advance().type;
    auto right = parsePower();
    left = std::make_unique<BinaryExprNode>(op, std::move(left),
                                            std::move(right), loc);
  }
  return left;
}

ASTNodePtr Parser::parsePower() {
  auto left = parseUnary();
  while (check(TokenType::Caret) || check(TokenType::CaretCaret)) {
    auto loc = current().location;
    auto op = advance().type;
    auto right = parseUnary();
    left = std::make_unique<BinaryExprNode>(op, std::move(left),
                                            std::move(right), loc);
  }
  return left;
}

ASTNodePtr Parser::parseUnary() {
  if (check(TokenType::Not) || check(TokenType::Minus)) {
    auto loc = current().location;
    auto op = advance().type;
    auto operand = parseUnary();
    return std::make_unique<UnaryExprNode>(op, std::move(operand), true, loc);
  }
  return parsePostfix();
}

} // namespace neuron
