#include "neuronc/parser/Parser.h"

namespace neuron {

ASTNodePtr Parser::parseExternDecl() {
  auto loc = current().location;
  expect(TokenType::Extern, "Expected 'extern'");

  std::optional<std::string> symbolOverride;
  if (match(TokenType::LeftParen)) {
    Token symbolToken =
        expect(TokenType::StringLiteral, "Expected string literal after 'extern('");
    symbolOverride = symbolToken.value;
    expect(TokenType::RightParen, "Expected ')' after extern symbol override");
  }

  Token nameToken =
      expect(TokenType::Identifier, "Expected extern declaration name");

  if (!check(TokenType::Method)) {
    error("Expected 'method' after extern declaration name");
    synchronize();
    return nullptr;
  }

  auto method = parseMethodDecl(nameToken.value, AccessModifier::None, loc);
  if (method == nullptr || method->type != ASTNodeType::MethodDecl) {
    return nullptr;
  }

  auto *methodDecl = static_cast<MethodDeclNode *>(method.get());
  methodDecl->isExtern = true;
  methodDecl->externSymbolOverride = symbolOverride;
  methodDecl->body.reset();

  return std::make_unique<ExternDeclNode>(std::move(method), symbolOverride, loc);
}

ASTNodePtr Parser::parseMethodDecl(const std::string &name,
                                   AccessModifier access, SourceLocation loc) {
  expect(TokenType::Method, "Expected 'method'");

  auto method = std::make_unique<MethodDeclNode>(name, loc);
  method->access = access;

  if (match(TokenType::Less)) {
    while (!check(TokenType::Greater) && !isAtEnd()) {
      auto param =
          expect(TokenType::Identifier, "Expected generic parameter name");
      method->genericParams.push_back(param.value);
      if (!match(TokenType::Comma))
        break;
    }
    expect(TokenType::Greater, "Expected '>' after generic parameters");
  }

  if (check(TokenType::LeftParen)) {
    method->parameters = parseParameterList();
  }

  if (match(TokenType::As)) {
    method->returnType = parseTypeSpec();
  }

  if (!method->isExtern &&
      (check(TokenType::LeftBrace) || canStartImplicitSingleStatementBody())) {
    ++m_methodDepth;
    method->body = parseBlockOrWhitelistedSingleStmt("method body");
    --m_methodDepth;
  }

  match(TokenType::Semicolon);
  return method;
}

ASTNodePtr Parser::parseClassDecl(const std::string &name,
                                  AccessModifier access, SourceLocation loc) {
  auto classDecl = std::make_unique<ClassDeclNode>(name, loc);
  classDecl->access = access;
  if (check(TokenType::Class)) {
    classDecl->kind = ClassKind::Class;
    advance();
  } else if (check(TokenType::Struct)) {
    classDecl->kind = ClassKind::Struct;
    advance();
  } else if (check(TokenType::Interface)) {
    classDecl->kind = ClassKind::Interface;
    advance();
  } else {
    error("Expected 'class', 'struct', or 'interface'");
    return nullptr;
  }

  if (match(TokenType::Less)) {
    while (!check(TokenType::Greater) && !isAtEnd()) {
      auto param =
          expect(TokenType::Identifier, "Expected generic parameter name");
      classDecl->genericParams.push_back(param.value);
      if (!match(TokenType::Comma))
        break;
    }
    expect(TokenType::Greater, "Expected '>' after generic parameters");
  }

  if (match(TokenType::Inherits)) {
    do {
      auto base = expect(TokenType::Identifier, "Expected base class name");
      classDecl->baseClasses.push_back(base.value);
    } while (match(TokenType::Comma));
  }

  expect(TokenType::LeftBrace, "Expected '{' to open class body");
  while (!m_pendingDeclarations.empty() ||
         (!check(TokenType::RightBrace) && !isAtEnd())) {
    const size_t beforePos = m_pos;
    const size_t beforePending = m_pendingDeclarations.size();
    auto member = parseDeclaration();
    if (member) {
      classDecl->members.push_back(std::move(member));
    } else if (m_pos == beforePos &&
               m_pendingDeclarations.size() == beforePending) {
      recoverNoProgress();
    }
  }
  expect(TokenType::RightBrace, "Expected '}' to close class body");
  return classDecl;
}

ASTNodePtr Parser::parseEnumDecl(const std::string &name, AccessModifier access,
                                 SourceLocation loc) {
  expect(TokenType::Enum, "Expected 'enum'");
  auto enumDecl = std::make_unique<EnumDeclNode>(name, loc);
  enumDecl->access = access;

  expect(TokenType::LeftBrace, "Expected '{' to open enum body");
  while (!check(TokenType::RightBrace) && !isAtEnd()) {
    auto member = expect(TokenType::Identifier, "Expected enum member name");
    enumDecl->members.push_back(member.value);
    if (!match(TokenType::Comma)) {
      break;
    }
  }
  expect(TokenType::RightBrace, "Expected '}' to close enum body");
  match(TokenType::Semicolon);
  return enumDecl;
}

ASTNodePtr Parser::parseMethodShorthand(const std::string &name,
                                        SourceLocation loc) {
  auto method = std::make_unique<MethodDeclNode>(name, loc);
  method->returnType = std::make_unique<TypeSpecNode>("void", loc);
  ++m_methodDepth;
  method->body = parseBlockOrWhitelistedSingleStmt("method body");
  --m_methodDepth;
  match(TokenType::Semicolon);
  return method;
}

} // namespace neuron

