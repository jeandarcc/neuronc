#include "neuronc/parser/Parser.h"

namespace neuron {

ASTNodePtr Parser::parseMatchStmt() {
  auto loc = current().location;
  expect(TokenType::Match, "Expected 'match'");
  expect(TokenType::LeftParen, "Expected '(' after 'match'");

  auto stmt = std::make_unique<MatchStmtNode>(loc);
  stmt->expressions.push_back(parseExpression());
  while (match(TokenType::Comma)) {
    stmt->expressions.push_back(parseExpression());
  }

  expect(TokenType::RightParen, "Expected ')' after match expression");
  expect(TokenType::LeftBrace, "Expected '{' to open match body");

  while (!check(TokenType::RightBrace) && !isAtEnd()) {
    const size_t beforePos = m_pos;
    auto armLoc = current().location;
    bool isDefault = false;
    std::vector<ASTNodePtr> patternExprs;

    if (match(TokenType::Default)) {
      isDefault = true;
    } else {
      patternExprs.push_back(parseExpression());
      while (match(TokenType::Comma)) {
        patternExprs.push_back(parseExpression());
      }
    }

    expect(TokenType::Then, "Expected 'then' after match arm pattern");
    stmt->arms.push_back(std::make_unique<MatchArmNode>(
        std::move(patternExprs),
        parseBlockOrWhitelistedSingleStmt("match arm body"), nullptr,
        isDefault, armLoc));
    if (m_pos == beforePos) {
      recoverNoProgress();
    }
  }

  expect(TokenType::RightBrace, "Expected '}' to close match body");
  return stmt;
}

ASTNodePtr Parser::parseMatchExpr() {
  auto loc = current().location;
  expect(TokenType::Match, "Expected 'match'");
  expect(TokenType::LeftParen, "Expected '(' after 'match'");

  auto expr = std::make_unique<MatchExprNode>(loc);
  expr->expressions.push_back(parseExpression());
  while (match(TokenType::Comma)) {
    expr->expressions.push_back(parseExpression());
  }

  expect(TokenType::RightParen, "Expected ')' after match expression");
  expect(TokenType::LeftBrace, "Expected '{' to open match body");

  while (!check(TokenType::RightBrace) && !isAtEnd()) {
    const size_t beforePos = m_pos;
    auto armLoc = current().location;
    bool isDefault = false;
    std::vector<ASTNodePtr> patternExprs;

    if (match(TokenType::Default)) {
      isDefault = true;
    } else {
      patternExprs.push_back(parseExpression());
      while (match(TokenType::Comma)) {
        patternExprs.push_back(parseExpression());
      }
    }

    expect(TokenType::Then, "Expected 'then' after match arm pattern");
    ASTNodePtr valueExpr = parseExpression();
    expect(TokenType::Semicolon, "Expected ';' after match arm expression");

    expr->arms.push_back(std::make_unique<MatchArmNode>(
        std::move(patternExprs), nullptr, std::move(valueExpr), isDefault,
        armLoc));
    if (m_pos == beforePos) {
      recoverNoProgress();
    }
  }

  expect(TokenType::RightBrace, "Expected '}' to close match body");
  return expr;
}

ASTNodePtr Parser::parseIfStmt() {
  auto loc = current().location;
  expect(TokenType::If, "Expected 'if'");
  expect(TokenType::LeftParen, "Expected '(' after 'if'");

  auto stmt = std::make_unique<IfStmtNode>(loc);
  stmt->condition = parseExpression();
  expect(TokenType::RightParen, "Expected ')' after condition");
  stmt->thenBlock = parseBlockOrWhitelistedSingleStmt("if body");

  if (match(TokenType::Else)) {
    if (check(TokenType::If)) {
      stmt->elseBlock = parseIfStmt();
    } else {
      stmt->elseBlock = parseBlockOrWhitelistedSingleStmt("else body");
    }
  }

  return stmt;
}

ASTNodePtr Parser::parseWhileStmt() {
  auto loc = current().location;
  expect(TokenType::While, "Expected 'while'");
  expect(TokenType::LeftParen, "Expected '(' after 'while'");

  auto stmt = std::make_unique<WhileStmtNode>(loc);
  stmt->condition = parseExpression();
  expect(TokenType::RightParen, "Expected ')' after condition");
  stmt->body = parseBlockOrWhitelistedSingleStmt("while body");

  return stmt;
}

ASTNodePtr Parser::parseForStmt() {
  auto loc = current().location;
  bool isParallel = false;

  if (match(TokenType::Parallel)) {
    isParallel = true;
  }

  expect(TokenType::For, "Expected 'for'");
  expect(TokenType::LeftParen, "Expected '(' after 'for'");

  if (check(TokenType::Identifier) && peek().type == TokenType::In) {
    auto forIn = std::make_unique<ForInStmtNode>(loc);
    forIn->variable = current().value;
    forIn->variableLocation = current().location;
    advance();
    advance();
    forIn->iterable = parseExpression();
    expect(TokenType::RightParen, "Expected ')' after for-in");
    forIn->body = parseBlockOrWhitelistedSingleStmt("for-in body");
    return forIn;
  }

  auto forStmt = std::make_unique<ForStmtNode>(loc);
  forStmt->isParallel = isParallel;
  forStmt->init = parseStatement();
  forStmt->condition = parseExpression();
  expect(TokenType::Semicolon, "Expected ';' after for condition");

  if (check(TokenType::Identifier) &&
      (peek().type == TokenType::PlusPlus ||
       peek().type == TokenType::MinusMinus)) {
    std::string var = current().value;
    auto incLoc = current().location;
    auto op = peek().type;
    advance();
    advance();
    if (op == TokenType::PlusPlus) {
      forStmt->increment = std::make_unique<IncrementStmtNode>(var, incLoc);
    } else {
      forStmt->increment = std::make_unique<DecrementStmtNode>(var, incLoc);
    }
  } else {
    forStmt->increment = parseExpression();
  }

  expect(TokenType::RightParen, "Expected ')' after for clauses");
  forStmt->body = parseBlockOrWhitelistedSingleStmt("for body");
  return forStmt;
}

ASTNodePtr Parser::parseTryStmt() {
  auto loc = current().location;
  expect(TokenType::Try, "Expected 'try'");

  auto tryStmt = std::make_unique<TryStmtNode>(loc);
  tryStmt->tryBlock = parseBlockOrWhitelistedSingleStmt("try body");

  while (check(TokenType::Catch)) {
    auto catchLoc = current().location;
    advance();
    expect(TokenType::LeftParen, "Expected '(' after 'catch'");

    auto catchClause = std::make_unique<CatchClauseNode>(catchLoc);
    if (check(TokenType::Identifier)) {
      std::string first = current().value;
      SourceLocation firstLocation = current().location;
      advance();
      if (check(TokenType::Identifier)) {
        catchClause->errorType =
            std::make_unique<TypeSpecNode>(first, firstLocation);
        catchClause->errorName = current().value;
        catchClause->errorLocation = current().location;
        advance();
      } else {
        catchClause->errorName = first;
        catchClause->errorLocation = firstLocation;
      }
    }

    expect(TokenType::RightParen, "Expected ')' after catch parameter");
    catchClause->body = parseBlockOrWhitelistedSingleStmt("catch body");
    tryStmt->catchClauses.push_back(std::move(catchClause));
  }

  if (match(TokenType::Finally)) {
    tryStmt->finallyBlock = parseBlockOrWhitelistedSingleStmt("finally body");
  }

  return tryStmt;
}

ASTNodePtr Parser::parseThrowStmt() {
  auto loc = current().location;
  advance();
  auto stmt = std::make_unique<ThrowStmtNode>(loc);
  stmt->value = parseExpression();
  expect(TokenType::Semicolon, "Expected ';' after throw");
  return stmt;
}

} // namespace neuron

