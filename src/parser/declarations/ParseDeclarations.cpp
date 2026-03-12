#include "neuronc/parser/Parser.h"

namespace neuron {

ASTNodePtr Parser::parseBindingOrMethodOrClass() {
  auto loc = current().location;
  size_t startPos = m_pos;

  AccessModifier leadingAccess = AccessModifier::None;
  bool leadingAbstract = false;
  bool leadingVirtual = false;
  bool leadingOverride = false;
  bool leadingOverload = false;
  bool leadingProgress = true;
  while (leadingProgress) {
    leadingProgress = false;
    if (check(TokenType::Public)) {
      leadingAccess = AccessModifier::Public;
      advance();
      leadingProgress = true;
    } else if (check(TokenType::Private)) {
      leadingAccess = AccessModifier::Private;
      advance();
      leadingProgress = true;
    } else if (check(TokenType::Abstract)) {
      leadingAbstract = true;
      advance();
      leadingProgress = true;
    } else if (check(TokenType::Virtual)) {
      leadingVirtual = true;
      advance();
      leadingProgress = true;
    } else if (check(TokenType::Override)) {
      leadingOverride = true;
      advance();
      leadingProgress = true;
    } else if (check(TokenType::Overload)) {
      leadingOverload = true;
      advance();
      leadingProgress = true;
    }
  }

  if (!check(TokenType::Identifier) && !check(TokenType::Constructor)) {
    error("Expected identifier");
    synchronize();
    return nullptr;
  }

  std::string name = current().value;
  advance();
  std::vector<std::pair<std::string, SourceLocation>> bindingNames;
  bindingNames.push_back({name, loc});
  while (match(TokenType::Comma)) {
    Token nextName =
        expect(TokenType::Identifier, "Expected identifier after ','");
    bindingNames.push_back({nextName.value, nextName.location});
  }

  const bool hasIs = match(TokenType::Is);

  AccessModifier declAccess = leadingAccess;
  bool declAbstract = leadingAbstract;
  bool declVirtual = leadingVirtual;
  bool declOverride = leadingOverride;
  bool declOverload = leadingOverload;
  bool modifierProgress = true;
  while (modifierProgress) {
    modifierProgress = false;
    if (check(TokenType::Public)) {
      declAccess = AccessModifier::Public;
      advance();
      modifierProgress = true;
    } else if (check(TokenType::Private)) {
      declAccess = AccessModifier::Private;
      advance();
      modifierProgress = true;
    } else if (check(TokenType::Abstract)) {
      declAbstract = true;
      advance();
      modifierProgress = true;
    } else if (check(TokenType::Virtual)) {
      declVirtual = true;
      advance();
      modifierProgress = true;
    } else if (check(TokenType::Override)) {
      declOverride = true;
      advance();
      modifierProgress = true;
    } else if (check(TokenType::Overload)) {
      declOverload = true;
      advance();
      modifierProgress = true;
    }
  }

  if (!hasIs && !(check(TokenType::Method) || check(TokenType::Async) ||
                  check(TokenType::Class) || check(TokenType::Struct) ||
                  check(TokenType::Interface) || check(TokenType::Enum) ||
                  check(TokenType::Shader))) {
    m_pos = startPos;
    return nullptr;
  }

  if (check(TokenType::Method) || check(TokenType::Async)) {
    if (bindingNames.size() > 1) {
      error("Multiple identifiers are only supported for bindings");
      synchronize();
      return nullptr;
    }
    bool isAsync = false;
    if (check(TokenType::Async)) {
      isAsync = true;
      advance();
      if (!check(TokenType::Method)) {
        error("Expected 'method' after 'async'");
        return nullptr;
      }
    }
    auto method = parseMethodDecl(name, declAccess, loc);
    if (method) {
      auto *methodDecl = static_cast<MethodDeclNode *>(method.get());
      methodDecl->isAsync = methodDecl->isAsync || isAsync;
      methodDecl->isAbstract = declAbstract;
      methodDecl->isVirtual = declVirtual;
      methodDecl->isOverride = declOverride;
      methodDecl->isOverload = declOverload;
    }
    return method;
  }

  if (check(TokenType::Class) || check(TokenType::Struct) ||
      check(TokenType::Interface)) {
    if (bindingNames.size() > 1) {
      error("Multiple identifiers are only supported for bindings");
      synchronize();
      return nullptr;
    }
    auto classDecl = parseClassDecl(name, declAccess, loc);
    if (classDecl) {
      static_cast<ClassDeclNode *>(classDecl.get())->isAbstract = declAbstract;
    }
    return classDecl;
  }

  if (check(TokenType::Enum)) {
    if (bindingNames.size() > 1) {
      error("Multiple identifiers are only supported for bindings");
      synchronize();
      return nullptr;
    }
    return parseEnumDecl(name, declAccess, loc);
  }

  if (check(TokenType::Shader)) {
    if (bindingNames.size() > 1) {
      error("Multiple identifiers are only supported for bindings");
      synchronize();
      return nullptr;
    }
    return parseShaderDecl(name, declAccess, loc);
  }

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

  const ASTNode *valueTemplate = value.get();
  const ASTNode *typeTemplate = typeAnnotation.get();
  auto binding = std::make_unique<BindingDeclNode>(bindingNames.front().first, kind,
                                                   std::move(value), loc);
  binding->access = declAccess;
  binding->typeAnnotation = std::move(typeAnnotation);
  for (std::size_t i = 1; i < bindingNames.size(); ++i) {
    auto extraBinding = std::make_unique<BindingDeclNode>(
        bindingNames[i].first, kind, cloneAstNode(valueTemplate),
        bindingNames[i].second);
    extraBinding->access = declAccess;
    extraBinding->typeAnnotation = cloneAstNode(typeTemplate);
    queuePendingDeclaration(std::move(extraBinding));
  }
  return binding;
}

ASTNodePtr Parser::parseConstBinding() {
  auto loc = current().location;
  expect(TokenType::Const, "Expected 'const'");

  if (!check(TokenType::Identifier) && !check(TokenType::Constructor)) {
    error("Expected identifier after 'const'");
    synchronize();
    return nullptr;
  }

  std::vector<std::pair<std::string, SourceLocation>> bindingNames;
  bindingNames.push_back({current().value, current().location});
  advance();
  while (match(TokenType::Comma)) {
    Token nextName =
        expect(TokenType::Identifier, "Expected identifier after ','");
    bindingNames.push_back({nextName.value, nextName.location});
  }
  expect(TokenType::Is, "Expected 'is' after const variable name");

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

  expect(TokenType::Semicolon, "Expected ';' after const binding");
  const ASTNode *valueTemplate = value.get();
  const ASTNode *typeTemplate = typeAnnotation.get();
  auto binding = std::make_unique<BindingDeclNode>(
      bindingNames.front().first, kind, std::move(value), bindingNames.front().second);
  binding->typeAnnotation = std::move(typeAnnotation);
  binding->isConst = true;
  for (std::size_t i = 1; i < bindingNames.size(); ++i) {
    auto extraBinding = std::make_unique<BindingDeclNode>(
        bindingNames[i].first, kind, cloneAstNode(valueTemplate),
        bindingNames[i].second);
    extraBinding->typeAnnotation = cloneAstNode(typeTemplate);
    extraBinding->isConst = true;
    queuePendingDeclaration(std::move(extraBinding));
  }
  return binding;
}

ASTNodePtr Parser::parseAtomicBinding() {
  auto loc = current().location;
  expect(TokenType::Atomic, "Expected 'atomic'");

  if (!check(TokenType::Identifier) && !check(TokenType::Constructor)) {
    error("Expected identifier after 'atomic'");
    synchronize();
    return nullptr;
  }

  std::vector<std::pair<std::string, SourceLocation>> bindingNames;
  bindingNames.push_back({current().value, current().location});
  advance();
  while (match(TokenType::Comma)) {
    Token nextName =
        expect(TokenType::Identifier, "Expected identifier after ','");
    bindingNames.push_back({nextName.value, nextName.location});
  }
  expect(TokenType::Is, "Expected 'is' after atomic variable name");

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

  expect(TokenType::Semicolon, "Expected ';' after atomic binding");
  const ASTNode *valueTemplate = value.get();
  const ASTNode *typeTemplate = typeAnnotation.get();
  auto binding = std::make_unique<BindingDeclNode>(
      bindingNames.front().first, kind, std::move(value), bindingNames.front().second);
  binding->typeAnnotation = std::move(typeAnnotation);
  binding->isAtomic = true;
  for (std::size_t i = 1; i < bindingNames.size(); ++i) {
    auto extraBinding = std::make_unique<BindingDeclNode>(
        bindingNames[i].first, kind, cloneAstNode(valueTemplate),
        bindingNames[i].second);
    extraBinding->typeAnnotation = cloneAstNode(typeTemplate);
    extraBinding->isAtomic = true;
    queuePendingDeclaration(std::move(extraBinding));
  }
  return binding;
}

} // namespace neuron

