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

} // namespace

bool Parser::isWhitelistedImplicitBlockStmt(const ASTNode *node) const {
  if (node == nullptr) {
    return false;
  }

  switch (node->type) {
  case ASTNodeType::BindingDecl:
  case ASTNodeType::IfStmt:
  case ASTNodeType::MatchStmt:
  case ASTNodeType::MatchExpr:
  case ASTNodeType::WhileStmt:
  case ASTNodeType::ForStmt:
  case ASTNodeType::ForInStmt:
  case ASTNodeType::BreakStmt:
  case ASTNodeType::ContinueStmt:
  case ASTNodeType::ReturnStmt:
  case ASTNodeType::TryStmt:
  case ASTNodeType::ThrowStmt:
  case ASTNodeType::StaticAssertStmt:
  case ASTNodeType::CastStmt:
  case ASTNodeType::CallExpr:
  case ASTNodeType::InputExpr:
  case ASTNodeType::BinaryExpr:
  case ASTNodeType::UnaryExpr:
  case ASTNodeType::MemberAccessExpr:
  case ASTNodeType::IndexExpr:
  case ASTNodeType::SliceExpr:
  case ASTNodeType::TypeofExpr:
  case ASTNodeType::Identifier:
  case ASTNodeType::Error:
  case ASTNodeType::IntLiteral:
  case ASTNodeType::FloatLiteral:
  case ASTNodeType::StringLiteral:
  case ASTNodeType::BoolLiteral:
  case ASTNodeType::NullLiteral:
  case ASTNodeType::IncrementStmt:
  case ASTNodeType::DecrementStmt:
  case ASTNodeType::UnsafeBlock:
  case ASTNodeType::GpuBlock:
  case ASTNodeType::CanvasStmt:
    return true;
  default:
    return false;
  }
}

bool Parser::canStartImplicitSingleStatementBody() const {
  const auto hasIdentifierListBeforeIs = [&]() -> bool {
    if (!check(TokenType::Identifier) || peek().type != TokenType::Comma) {
      return false;
    }

    std::size_t offset = 1;
    while (lookahead(offset).type == TokenType::Comma) {
      if (lookahead(offset + 1).type != TokenType::Identifier) {
        return false;
      }
      offset += 2;
    }
    return lookahead(offset).type == TokenType::Is;
  };

  if (check(TokenType::Module) || check(TokenType::ModuleCpp) ||
      check(TokenType::Macro) || check(TokenType::Extern) ||
      check(TokenType::Public) || check(TokenType::Private) ||
      check(TokenType::Abstract) || check(TokenType::Virtual) ||
      check(TokenType::Override) || check(TokenType::Overload) ||
      check(TokenType::Constexpr)) {
    return false;
  }

  if (check(TokenType::Const) || check(TokenType::Atomic) ||
      check(TokenType::If) || check(TokenType::Match) ||
      check(TokenType::While) || check(TokenType::For) ||
      check(TokenType::Parallel) || check(TokenType::Return) ||
      check(TokenType::Try) || check(TokenType::Throw) ||
      check(TokenType::StaticAssert) || check(TokenType::Break) ||
      check(TokenType::Continue) || check(TokenType::Unsafe) ||
      check(TokenType::Gpu) || check(TokenType::Canvas) ||
      check(TokenType::ValueOf) || check(TokenType::This)) {
    return true;
  }

  if (!check(TokenType::Identifier) && !check(TokenType::Constructor)) {
    return false;
  }

  if (check(TokenType::Identifier) && peek().type == TokenType::LeftBrace &&
      isPascalCaseIdentifier(current().value)) {
    return false;
  }

  if (hasIdentifierListBeforeIs()) {
    return true;
  }

  if (peek().type == TokenType::Method || peek().type == TokenType::Async ||
      peek().type == TokenType::Class || peek().type == TokenType::Struct ||
      peek().type == TokenType::Interface || peek().type == TokenType::Enum ||
      peek().type == TokenType::Shader || peek().type == TokenType::Public ||
      peek().type == TokenType::Private || peek().type == TokenType::Abstract ||
      peek().type == TokenType::Virtual ||
      peek().type == TokenType::Override ||
      peek().type == TokenType::Overload) {
    return false;
  }

  return true;
}

ASTNodePtr Parser::parseBlockOrWhitelistedSingleStmt(
    std::string_view ownerDescription) {
  if (check(TokenType::LeftBrace)) {
    return parseBlock();
  }

  auto loc = current().location;
  auto block = std::make_unique<BlockNode>(loc);
  block->hasExplicitBraces = false;

  if (!canStartImplicitSingleStatementBody()) {
    error("Expected '{' or a whitelisted single-statement body for " +
          std::string(ownerDescription));
    return block;
  }

  ASTNodePtr stmt = parseDeclaration();
  if (stmt != nullptr) {
    if (!isWhitelistedImplicitBlockStmt(stmt.get())) {
      error("Implicit single-statement body is not allowed for " +
            std::string(ownerDescription) + "; use '{ ... }'.");
    }
    block->endLine = stmt->location.line;
    block->endColumn = stmt->location.column;
    block->statements.push_back(std::move(stmt));
  }

  while (!m_pendingDeclarations.empty()) {
    ASTNodePtr pending = takePendingDeclaration();
    if (pending == nullptr) {
      continue;
    }
    if (!isWhitelistedImplicitBlockStmt(pending.get())) {
      error("Implicit single-statement body is not allowed for " +
            std::string(ownerDescription) + "; use '{ ... }'.");
    }
    block->endLine = pending->location.line;
    block->endColumn = pending->location.column;
    block->statements.push_back(std::move(pending));
  }

  return block;
}

ASTNodePtr Parser::parseStatement() {
  auto loc = current().location;
  const auto hasIdentifierListBeforeIs = [&]() -> bool {
    if (!check(TokenType::Identifier) || peek().type != TokenType::Comma) {
      return false;
    }

    std::size_t offset = 1;
    while (lookahead(offset).type == TokenType::Comma) {
      if (lookahead(offset + 1).type != TokenType::Identifier) {
        return false;
      }
      offset += 2;
    }
    return lookahead(offset).type == TokenType::Is;
  };

  if (match(TokenType::Semicolon)) {
    return nullptr;
  }
  if (check(TokenType::Const))
    return parseConstBinding();
  if (check(TokenType::Atomic))
    return parseAtomicBinding();
  if (check(TokenType::If))
    return parseIfStmt();
  if (check(TokenType::Match))
    return parseMatchStmt();
  if (check(TokenType::While))
    return parseWhileStmt();
  if (check(TokenType::For) || check(TokenType::Parallel))
    return parseForStmt();
  if (check(TokenType::Return))
    return parseReturnStmt();
  if (check(TokenType::Try))
    return parseTryStmt();
  if (check(TokenType::Throw))
    return parseThrowStmt();
  if (check(TokenType::StaticAssert))
    return parseStaticAssertStmt();
  if (check(TokenType::Break))
    return parseBreakStmt();
  if (check(TokenType::Continue))
    return parseContinueStmt();
  if (check(TokenType::Unsafe))
    return parseUnsafeBlock();
  if (check(TokenType::Gpu))
    return parseGpuBlock();
  if (check(TokenType::Canvas))
    return parseCanvasStmt();
  if (check(TokenType::Pass))
    return parseShaderPassStmt();
  if (check(TokenType::LeftBrace))
    return parseBlock();

  if (check(TokenType::Public) || check(TokenType::Private) ||
      check(TokenType::Abstract) || check(TokenType::Virtual) ||
      check(TokenType::Override) || check(TokenType::Overload)) {
    return parseBindingOrMethodOrClass();
  }

  if (check(TokenType::Constexpr)) {
    advance();
    if (check(TokenType::Identifier) &&
        (peek().type == TokenType::Is || peek().type == TokenType::Method ||
         peek().type == TokenType::Async || peek().type == TokenType::Class ||
         peek().type == TokenType::Struct ||
         peek().type == TokenType::Interface || peek().type == TokenType::Enum ||
         peek().type == TokenType::Shader ||
         peek().type == TokenType::Public || peek().type == TokenType::Private ||
         peek().type == TokenType::Abstract ||
         peek().type == TokenType::Virtual ||
         peek().type == TokenType::Override ||
         peek().type == TokenType::Overload)) {
      auto result = parseBindingOrMethodOrClass();
      if (result && result->type == ASTNodeType::MethodDecl) {
        static_cast<MethodDeclNode *>(result.get())->isConstexpr = true;
      }
      return result;
    }
    error("Expected identifier after 'constexpr'");
    synchronize();
    return nullptr;
  }

  if (check(TokenType::ValueOf)) {
    advance();
    auto target = parseExpression();
    expect(TokenType::Is, "Expected 'is' in value assignment");
    auto value = parseExpression();
    expect(TokenType::Semicolon, "Expected ';'");
    return std::make_unique<BindingDeclNode>("__deref__", BindingKind::ValueOf,
                                             std::move(value), loc);
  }

  if (check(TokenType::Identifier) || check(TokenType::Constructor) ||
      check(TokenType::This)) {
    if (check(TokenType::Identifier) && peek().type == TokenType::LeftBrace &&
        isPascalCaseIdentifier(current().value)) {
      std::string name = current().value;
      SourceLocation methodLoc = current().location;
      advance();
      return parseMethodShorthand(name, methodLoc);
    }

    if ((check(TokenType::Identifier) || check(TokenType::Constructor)) &&
        (peek().type == TokenType::Is || peek().type == TokenType::Method ||
         peek().type == TokenType::Async || peek().type == TokenType::Class ||
         peek().type == TokenType::Struct ||
         peek().type == TokenType::Interface || peek().type == TokenType::Enum ||
         peek().type == TokenType::Shader ||
         peek().type == TokenType::Public || peek().type == TokenType::Private ||
         peek().type == TokenType::Abstract ||
         peek().type == TokenType::Virtual ||
         peek().type == TokenType::Override ||
         peek().type == TokenType::Overload || hasIdentifierListBeforeIs())) {
      return parseBindingOrMethodOrClass();
    }

    if (check(TokenType::Identifier) && peek().type == TokenType::Semicolon) {
      std::string varName = current().value;
      auto declLoc = current().location;
      advance();
      advance();
      auto binding = std::make_unique<BindingDeclNode>(varName, BindingKind::Value,
                                                       nullptr, declLoc);
      binding->typeAnnotation = std::make_unique<TypeSpecNode>("dynamic", declLoc);
      return binding;
    }

    if (check(TokenType::Identifier) &&
        (peek().type == TokenType::IntLiteral ||
         peek().type == TokenType::FloatLiteral ||
         peek().type == TokenType::StringLiteral ||
         peek().type == TokenType::True || peek().type == TokenType::False ||
         peek().type == TokenType::Null ||
         peek().type == TokenType::Identifier || peek().type == TokenType::This ||
         peek().type == TokenType::Another || peek().type == TokenType::AddressOf ||
         peek().type == TokenType::ValueOf || peek().type == TokenType::Move ||
         peek().type == TokenType::Minus || peek().type == TokenType::Not ||
         peek().type == TokenType::Typeof || peek().type == TokenType::Thread)) {
      std::string varName = current().value;
      auto declLoc = current().location;
      advance();

      BindingKind kind = BindingKind::Value;
      if (check(TokenType::Another)) {
        kind = BindingKind::Copy;
        advance();
      } else if (check(TokenType::AddressOf)) {
        kind = BindingKind::AddressOf;
        advance();
      } else if (check(TokenType::ValueOf)) {
        kind = BindingKind::ValueOf;
        advance();
      } else if (check(TokenType::Move)) {
        kind = BindingKind::MoveFrom;
        advance();
      }

      auto value = parseExpression();
      ASTNodePtr typeAnnotation = nullptr;
      if (match(TokenType::As)) {
        typeAnnotation = parseTypeSpec();
      }
      expect(TokenType::Semicolon, "Expected ';' after binding");
      auto binding = std::make_unique<BindingDeclNode>(varName, kind,
                                                       std::move(value), declLoc);
      binding->typeAnnotation = std::move(typeAnnotation);
      return binding;
    }

    if (check(TokenType::Identifier) && peek().type == TokenType::PlusPlus) {
      std::string varName = current().value;
      advance();
      advance();
      expect(TokenType::Semicolon, "Expected ';' after increment");
      return std::make_unique<IncrementStmtNode>(varName, loc);
    }

    if (check(TokenType::Identifier) && peek().type == TokenType::MinusMinus) {
      std::string varName = current().value;
      advance();
      advance();
      expect(TokenType::Semicolon, "Expected ';' after decrement");
      return std::make_unique<DecrementStmtNode>(varName, loc);
    }

    auto expr = parseExpression();
    if (check(TokenType::As)) {
      return parseCastStmt(std::move(expr), loc);
    }

    if (check(TokenType::Is)) {
      advance();
      BindingKind kind = BindingKind::Value;
      if (check(TokenType::Another)) {
        kind = BindingKind::Copy;
        advance();
      } else if (check(TokenType::AddressOf)) {
        kind = BindingKind::AddressOf;
        advance();
      } else if (check(TokenType::ValueOf)) {
        kind = BindingKind::ValueOf;
        advance();
      } else if (check(TokenType::Move)) {
        kind = BindingKind::MoveFrom;
        advance();
      }

      if (check(TokenType::Method) || check(TokenType::Async)) {
        auto method = parseMethodDecl("__expr_target__", AccessModifier::None, loc);
        expect(TokenType::Semicolon, "Expected ';'");
        return method;
      }

      auto value = parseExpression();
      ASTNodePtr typeAnnotation = nullptr;
      if (match(TokenType::As)) {
        typeAnnotation = parseTypeSpec();
      }
      expect(TokenType::Semicolon, "Expected ';' after assignment");

      auto assign = std::make_unique<BindingDeclNode>("__assign__", kind,
                                                      std::move(value), loc);
      assign->target = std::move(expr);
      assign->typeAnnotation = std::move(typeAnnotation);
      return assign;
    }

    expect(TokenType::Semicolon, "Expected ';' after expression");
    return expr;
  }

  error("N1103", {{"token", current().value}},
        "Unexpected token: '" + current().value + "'");
  synchronize();
  return nullptr;
}

ASTNodePtr Parser::parseCastStmt(ASTNodePtr target, SourceLocation loc) {
  expect(TokenType::As, "Expected 'as' in cast statement");

  auto stmt = std::make_unique<CastStmtNode>(std::move(target), loc);
  stmt->steps.push_back(parseCastStep(&stmt->pipelineNullable, true));
  while (match(TokenType::Then)) {
    stmt->steps.push_back(parseCastStep(nullptr, false));
  }

  expect(TokenType::Semicolon, "Expected ';' after cast statement");
  return stmt;
}

ASTNodePtr Parser::parseBlock() {
  auto loc = current().location;
  expect(TokenType::LeftBrace, "Expected '{'");

  auto block = std::make_unique<BlockNode>(loc);
  while (!m_pendingDeclarations.empty() ||
         (!check(TokenType::RightBrace) && !isAtEnd())) {
    const size_t beforePos = m_pos;
    const size_t beforePending = m_pendingDeclarations.size();
    auto stmt = parseDeclaration();
    if (stmt) {
      block->statements.push_back(std::move(stmt));
    } else if (m_pos == beforePos &&
               m_pendingDeclarations.size() == beforePending) {
      recoverNoProgress();
    }
  }

  Token closing = expect(TokenType::RightBrace, "Expected '}'");
  block->endLine = closing.location.line;
  block->endColumn = closing.location.column;
  return block;
}

ASTNodePtr Parser::parseReturnStmt() {
  auto loc = current().location;
  advance();

  auto stmt = std::make_unique<ReturnStmtNode>(loc);
  if (!check(TokenType::Semicolon)) {
    stmt->value = parseExpression();
  }
  expect(TokenType::Semicolon, "Expected ';' after return");
  return stmt;
}

ASTNodePtr Parser::parseStaticAssertStmt() {
  auto loc = current().location;
  expect(TokenType::StaticAssert, "Expected 'static_assert'");
  expect(TokenType::LeftParen, "Expected '(' after 'static_assert'");

  auto node = std::make_unique<StaticAssertStmtNode>(loc);
  node->condition = parseExpression();
  if (match(TokenType::Comma)) {
    auto msg = expect(TokenType::StringLiteral,
                      "Expected string literal as static_assert message");
    node->message = msg.value;
  }

  expect(TokenType::RightParen, "Expected ')' after static_assert");
  expect(TokenType::Semicolon, "Expected ';' after static_assert");
  return node;
}

ASTNodePtr Parser::parseBreakStmt() {
  auto loc = current().location;
  advance();
  expect(TokenType::Semicolon, "Expected ';' after break");
  return std::make_unique<BreakStmtNode>(loc);
}

ASTNodePtr Parser::parseContinueStmt() {
  auto loc = current().location;
  advance();
  expect(TokenType::Semicolon, "Expected ';' after continue");
  return std::make_unique<ContinueStmtNode>(loc);
}

ASTNodePtr Parser::parseUnsafeBlock() {
  auto loc = current().location;
  advance();
  auto node = std::make_unique<UnsafeBlockNode>(loc);
  node->body = parseBlock();
  return node;
}

} // namespace neuron

