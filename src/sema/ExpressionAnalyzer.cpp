#include "ExpressionAnalyzer.h"

#include "AnalysisHelpers.h"
#include "SemanticDriver.h"
#include "neuronc/parser/FusionChain.h"

#include <algorithm>
#include <unordered_set>

namespace neuron::sema_detail {

namespace {

NTypePtr inferMutationExpr(SemanticDriver &driver, const std::string &variable,
                           const SourceLocation &loc,
                           std::string_view actionName) {
  AnalysisContext &context = driver.context();
  Symbol *symbol = context.currentScope()->lookup(variable);
  if (symbol == nullptr) {
    context.error(loc, "N2201", {{"name", variable}},
                  "Undefined identifier: " + variable);
    return NType::makeError();
  }
  if (symbol->isConst) {
    context.error(loc, "Cannot mutate const variable: " + variable);
  }
  if (!symbol->type->isNumeric() && !symbol->type->isUnknown()) {
    context.error(loc,
                  std::string(actionName) + " requires a numeric variable: " +
                      variable);
  }
  context.recordReference(symbol, loc, symbolNameLength(variable));
  return symbol->type;
}

bool operatorRequiresNonNullOperands(TokenType op) {
  return op == TokenType::Plus || op == TokenType::Minus ||
         op == TokenType::Star || op == TokenType::Slash ||
         op == TokenType::Greater || op == TokenType::Less ||
         op == TokenType::GreaterEqual || op == TokenType::LessEqual;
}

bool isVectorLikeType(const NTypePtr &type) {
  if (type == nullptr || type->kind != TypeKind::Class) {
    return false;
  }
  return type->name == "Vector2" || type->name == "Vector3" ||
         type->name == "Vector4" || type->name == "Color";
}

} // namespace

ExpressionAnalyzer::ExpressionAnalyzer(SemanticDriver &driver)
    : m_driver(driver) {}

NTypePtr ExpressionAnalyzer::infer(ASTNode *expr) {
  if (expr == nullptr) {
    return NType::makeUnknown();
  }

  NTypePtr inferred;
  switch (expr->type) {
  case ASTNodeType::IntLiteral:
    inferred = NType::makeInt();
    break;
  case ASTNodeType::FloatLiteral:
    inferred = NType::makeFloat();
    break;
  case ASTNodeType::StringLiteral:
    inferred = NType::makeString();
    break;
  case ASTNodeType::BoolLiteral:
    inferred = NType::makeBool();
    break;
  case ASTNodeType::NullLiteral:
    inferred = NType::makeNullable(NType::makeUnknown());
    break;
  case ASTNodeType::Identifier:
    inferred = inferIdentifier(static_cast<IdentifierNode *>(expr));
    break;
  case ASTNodeType::CallExpr:
    inferred = inferCallExpr(static_cast<CallExprNode *>(expr));
    break;
  case ASTNodeType::InputExpr:
    inferred = m_driver.input().infer(static_cast<InputExprNode *>(expr));
    break;
  case ASTNodeType::MemberAccessExpr:
    inferred = inferMemberAccess(static_cast<MemberAccessNode *>(expr));
    break;
  case ASTNodeType::IndexExpr:
    inferred = inferIndexExpr(static_cast<IndexExprNode *>(expr));
    break;
  case ASTNodeType::SliceExpr:
    inferred = inferSliceExpr(static_cast<SliceExprNode *>(expr));
    break;
  case ASTNodeType::MatchExpr:
    inferred = inferMatchExpr(static_cast<MatchExprNode *>(expr));
    break;
  case ASTNodeType::TypeofExpr:
    inferred = inferTypeofExpr(static_cast<TypeofExprNode *>(expr));
    break;
  case ASTNodeType::BinaryExpr:
    inferred = inferBinaryExpr(static_cast<BinaryExprNode *>(expr));
    break;
  case ASTNodeType::UnaryExpr:
    inferred = inferUnaryExpr(static_cast<UnaryExprNode *>(expr));
    break;
  case ASTNodeType::IncrementStmt:
    inferred = inferMutationExpr(
        m_driver, static_cast<IncrementStmtNode *>(expr)->variable,
        expr->location, "Increment");
    break;
  case ASTNodeType::DecrementStmt:
    inferred = inferMutationExpr(
        m_driver, static_cast<DecrementStmtNode *>(expr)->variable,
        expr->location, "Decrement");
    break;
  case ASTNodeType::MethodDecl:
    inferred = inferMethodDecl(static_cast<MethodDeclNode *>(expr));
    break;
  case ASTNodeType::ShaderDecl:
    inferred = NType::makeClass("Shader");
    break;
  case ASTNodeType::CanvasStmt:
  case ASTNodeType::ShaderPassStmt:
    inferred = NType::makeVoid();
    break;
  case ASTNodeType::TypeSpec:
    inferred = m_driver.context().resolveType(expr);
    break;
  default:
    inferred = NType::makeUnknown();
    break;
  }

  return m_driver.context().rememberType(expr, inferred);
}

NTypePtr ExpressionAnalyzer::inferMethodDecl(MethodDeclNode *node) {
  if (node == nullptr) {
    return NType::makeUnknown();
  }

  std::vector<NTypePtr> parameterTypes;
  parameterTypes.reserve(node->parameters.size());
  for (const auto &param : node->parameters) {
    parameterTypes.push_back(m_driver.context().resolveType(param.typeSpec.get()));
  }
  NTypePtr returnType = m_driver.context().resolveType(node->returnType.get());
  m_driver.visitMethodDecl(node);
  return NType::makeMethod(returnType, std::move(parameterTypes));
}

NTypePtr ExpressionAnalyzer::inferIdentifier(IdentifierNode *node) {
  if (node == nullptr) {
    return NType::makeError();
  }

  AnalysisContext &context = m_driver.context();
  Symbol *symbol = context.currentScope()->lookup(node->name);
  if (symbol == nullptr) {
    if (const auto *classRecord = context.types().findClass(node->name)) {
      if (Symbol *classSymbol = context.symbols().lookupGlobal(node->name)) {
        context.recordReference(classSymbol, node->location,
                                symbolNameLength(node->name));
      }
      return classRecord->type;
    }

    NTypePtr resolvedType = context.resolveType(node->name);
    if (!resolvedType->isError()) {
      if (Symbol *typeSymbol = context.symbols().lookupGlobal(node->name)) {
        context.recordReference(typeSymbol, node->location,
                                symbolNameLength(node->name));
      }
      return resolvedType;
    }

    context.error(node->location, "N2201", {{"name", node->name}},
                  "Undefined identifier: " + node->name);
    return NType::makeError();
  }

  context.recordReference(symbol, node->location, symbolNameLength(node->name));
  m_driver.flow().validateRead(symbol, node->location);
  if (symbol->isMoved) {
    context.error(node->location, "Use of moved variable: " + node->name);
  }
  return symbol->type;
}

NTypePtr ExpressionAnalyzer::inferCallExpr(CallExprNode *node) {
  if (node == nullptr) {
    return NType::makeUnknown();
  }

  const std::string callName = resolveCallName(node->callee.get());
  if (callName == "Graphics.CreateWindow" || callName == "Graphics.CreateCanvas" ||
      callName == "Graphics.Clear" || callName == "Graphics.Draw" ||
      callName == "Graphics.Present" || callName == "Draw" ||
      callName == "Clear" || callName == "Present") {
    m_driver.context().error(
        node->location,
        "Legacy Graphics API '" + callName +
            "' was removed in Graphics v2. Use Window.Create(...) and "
            "frame commands like cmd.Clear(...) / cmd.Draw(...).");
    return NType::makeError();
  }

  if (node->callee != nullptr) {
    const std::string calleeName = resolveCallName(node->callee.get());
    if (!calleeName.empty()) {
      const Symbol *symbol = m_driver.context().currentScope()->lookup(calleeName);
      const bool calleeIsBareIdentifier =
          node->callee->type == ASTNodeType::Identifier;
      const bool calleeIsNonGenericTypeSpec =
          node->callee->type == ASTNodeType::TypeSpec &&
          static_cast<TypeSpecNode *>(node->callee.get())->genericArgs.empty();
      if (symbol != nullptr && symbol->kind == SymbolKind::Shader &&
          (calleeIsBareIdentifier || calleeIsNonGenericTypeSpec)) {
        m_driver.context().error(
            node->location,
            "N2011: Shader type '" + calleeName +
                "' cannot be instantiated directly. Use Material<" +
                calleeName + ">() instead.");
        return NType::makeError();
      }
    }
  }

  if (const FusionPatternSpec *fusion = matchFusionChain(node)) {
    CallExprNode *baseCall = fusionBaseCall(node);
    if (baseCall == nullptr) {
      m_driver.context().error(node->location, "Malformed fusion chain call.");
      return NType::makeError();
    }

    for (auto &arg : baseCall->arguments) {
      m_driver.inferExpression(arg.get());
    }
    if (baseCall->arguments.size() != fusion->argumentCount) {
      m_driver.context().error(
          node->location,
          "Fusion chain '" + node->fusionCallNames.front() + "' expects " +
              std::to_string(fusion->argumentCount) +
              " arguments for the registered fused lowering.");
      return NType::makeError();
    }
    return NType::makeTensor(NType::makeFloat());
  }

  if (NTypePtr inputType = m_driver.input().tryInferCallChain(node);
      inputType != nullptr) {
    return inputType;
  }

  m_driver.binder().bindNamedCallArguments(node);
  NTypePtr calleeType = m_driver.inferExpression(node->callee.get());
  if (calleeType != nullptr && calleeType->isError()) {
    return NType::makeError();
  }

  if (node->callee != nullptr &&
      node->callee->type == ASTNodeType::MemberAccessExpr) {
    auto *member = static_cast<MemberAccessNode *>(node->callee.get());
    NTypePtr objectType = m_driver.inferExpression(member->object.get());
    AnalysisContext &context = m_driver.context();
    if (objectType->kind == TypeKind::Tensor &&
        (member->member == "Random" || member->member == "Zeros" ||
         member->member == "Ones" || member->member == "Identity")) {
      for (auto &arg : node->arguments) {
        m_driver.inferExpression(arg.get());
      }
      return objectType;
    }

    if (objectType->kind == TypeKind::Class &&
        objectType->name == "Renderer2D" && member->member == "Render" &&
        !context.isInGraphicsFrame()) {
      context.error(node->location,
                    "Renderer2D.Render(scene) is only allowed inside OnFrame");
      return NType::makeError();
    }

    if (objectType->kind == TypeKind::Class &&
        objectType->name == "Entity" &&
        (member->member == "AddCamera2D" ||
         member->member == "AddSpriteRenderer2D" ||
         member->member == "AddShapeRenderer2D" ||
         member->member == "AddTextRenderer2D")) {
      if (member->object != nullptr &&
          member->object->type == ASTNodeType::Identifier) {
        const std::string entityName =
            static_cast<IdentifierNode *>(member->object.get())->name;
        if (!context.registerEntityComponent(context.currentScope(), entityName,
                                             member->member)) {
          context.error(node->location,
                        "Entity '" + entityName +
                            "' already has component " + member->member);
          return NType::makeError();
        }
      }
    }

    if (objectType->kind == TypeKind::Class &&
        objectType->name == "Transform" && member->member == "SetParent" &&
        member->object != nullptr &&
        member->object->type == ASTNodeType::Identifier &&
        !node->arguments.empty() && node->arguments[0] != nullptr &&
        node->arguments[0]->type == ASTNodeType::Identifier) {
      const std::string transformName =
          static_cast<IdentifierNode *>(member->object.get())->name;
      const std::string childEntity =
          context.findTransformEntity(context.currentScope(), transformName);
      const std::string parentEntity =
          static_cast<IdentifierNode *>(node->arguments[0].get())->name;
      if (!childEntity.empty() &&
          !context.setEntityParent(context.currentScope(), childEntity,
                                   parentEntity)) {
        context.error(node->location,
                      "Transform.SetParent would create an entity cycle");
        return NType::makeError();
      }
    }
  }

  if (calleeType->kind == TypeKind::Class) {
    return calleeType;
  }

  if (calleeType->kind == TypeKind::Method) {
    for (auto &arg : node->arguments) {
      m_driver.inferExpression(arg.get());
    }
    return calleeType->returnType;
  }

  if (!callName.empty() && calleeType->isUnknown()) {
    m_driver.context().error(node->location, "N2403", {{"target", callName}},
                             "Unknown callable target: " + callName);
    return NType::makeError();
  }

  return NType::makeUnknown();
}

NTypePtr ExpressionAnalyzer::inferMatchExpr(MatchExprNode *node) {
  if (node == nullptr || node->expressions.empty()) {
    m_driver.context().error(node ? node->location : SourceLocation{},
                             "match expression requires at least one selector "
                             "expression");
    return NType::makeError();
  }

  std::vector<NTypePtr> matchTypes;
  matchTypes.reserve(node->expressions.size());
  for (const auto &expr : node->expressions) {
    matchTypes.push_back(m_driver.inferExpression(expr.get()));
  }

  bool sawDefault = false;
  NTypePtr resultType = NType::makeUnknown();
  bool sawValueExpr = false;

  for (const auto &armNode : node->arms) {
    auto *matchArm = static_cast<MatchArmNode *>(armNode.get());
    if (matchArm->isDefault) {
      if (sawDefault) {
        m_driver.context().error(matchArm->location,
                                 "match can only contain one default arm");
      }
      sawDefault = true;
    } else {
      if (matchArm->patternExprs.size() != matchTypes.size()) {
        m_driver.context().error(
            matchArm->location,
            "Match arm pattern count mismatch: expected " +
                std::to_string(matchTypes.size()) + " but got " +
                std::to_string(matchArm->patternExprs.size()));
      }

      const std::size_t pairCount =
          std::min(matchTypes.size(), matchArm->patternExprs.size());
      for (std::size_t i = 0; i < pairCount; ++i) {
        NTypePtr armType =
            m_driver.inferExpression(matchArm->patternExprs[i].get());
        if (!matchTypes[i]->isUnknown() && !armType->isUnknown() &&
            !matchTypes[i]->isDynamic() && !armType->isDynamic() &&
            !matchTypes[i]->equals(*armType)) {
          m_driver.context().error(
              matchArm->location,
              "Match arm type mismatch: expected '" +
                  matchTypes[i]->toString() + "' but got '" +
                  armType->toString() + "'");
        }
      }
    }

    if (matchArm->body != nullptr) {
      m_driver.context().error(
          matchArm->location,
          "match expression arms must use 'then <expression>;' form");
      continue;
    }
    if (matchArm->valueExpr == nullptr) {
      m_driver.context().error(matchArm->location,
                               "match expression arm is missing a value");
      continue;
    }

    NTypePtr armValueType = m_driver.inferExpression(matchArm->valueExpr.get());
    if (!sawValueExpr) {
      resultType = armValueType;
      sawValueExpr = true;
      continue;
    }

    if (!resultType->isUnknown() && !armValueType->isUnknown() &&
        !resultType->isDynamic() && !armValueType->isDynamic() &&
        !resultType->equals(*armValueType)) {
      m_driver.context().error(matchArm->location,
                               "Match arm value type mismatch: expected '" +
                                   resultType->toString() + "' but got '" +
                                   armValueType->toString() + "'");
    }
  }

  return sawValueExpr ? resultType : NType::makeUnknown();
}

NTypePtr ExpressionAnalyzer::inferMemberAccess(MemberAccessNode *node) {
  if (node == nullptr) {
    return NType::makeUnknown();
  }

  AnalysisContext &context = m_driver.context();
  NTypePtr objectType = m_driver.inferExpression(node->object.get());
  if (objectType != nullptr && objectType->isError()) {
    return NType::makeError();
  }
  if (!m_driver.flow().validateNonNullExpression(node->object.get(), objectType,
                                                 node->location,
                                                 "member access")) {
    return NType::makeError();
  }

  if (objectType->kind == TypeKind::Descriptor) {
    const std::string &shaderName = objectType->className;
    const auto *binding = context.findShaderBinding(shaderName, node->member);
    if (binding == nullptr) {
      context.error(node->location,
                    "N2012: '" + node->member +
                        "' is not a field of shader descriptor '" +
                        shaderName + "'");
      return NType::makeError();
    }
    return context.resolveType(binding->typeName);
  }

  if (objectType->kind == TypeKind::Class) {
    std::unordered_set<std::string> visiting;
    Symbol *symbol = context.types().lookupClassMemberRecursive(
        objectType->name, node->member, context.symbols(), &visiting);
    if (symbol != nullptr) {
      context.recordReference(symbol, node->memberLocation,
                              symbolNameLength(node->member));
      m_driver.flow().validateRead(symbol, node->memberLocation);
      return symbol->type;
    }
    if (objectType->name == "Shader" && node->object != nullptr &&
        node->object->type == ASTNodeType::Identifier) {
      auto *shaderIdentifier =
          static_cast<IdentifierNode *>(node->object.get());
      const Symbol *shaderSymbol =
          context.currentScope()->lookup(shaderIdentifier->name);
      if (shaderSymbol != nullptr && shaderSymbol->kind == SymbolKind::Shader) {
        const std::string signatureKey =
            shaderIdentifier->name + "." + node->member;
        if (Symbol *methodSymbol =
                context.currentScope()->lookup(signatureKey)) {
          if (methodSymbol->kind == SymbolKind::Method) {
            context.recordReference(methodSymbol, node->memberLocation,
                                    symbolNameLength(node->member));
            return methodSymbol->type;
          }
        }
      }
    }
    if (node->member == "Length") {
      return NType::makeInt();
    }
  } else if (objectType->kind == TypeKind::Enum) {
    if (context.types().enumContains(objectType->name, node->member)) {
      return objectType;
    }
  } else if (objectType->kind == TypeKind::Module) {
    std::string moduleName = objectType->name;
    if (node->object != nullptr &&
        node->object->type == ASTNodeType::Identifier) {
      moduleName = static_cast<IdentifierNode *>(node->object.get())->name;
    }

    if (auto memberRecord =
            context.types().findModuleMember(moduleName, node->member);
        memberRecord.has_value()) {
      if (Symbol *symbol = context.symbols().lookupGlobal(node->member)) {
        if (symbol->kind == memberRecord->kind) {
          context.recordReference(symbol, node->memberLocation,
                                  symbolNameLength(node->member));
        }
      }
      return memberRecord->type;
    }

    NTypePtr nativeMember = context.types().moduleCppMemberType(
        moduleName, node->member, context.symbols(), context.diagnostics(),
        node->location);
    if (!nativeMember->isError() && !nativeMember->isUnknown()) {
      return nativeMember;
    }
    if (nativeMember->isError()) {
      return NType::makeError();
    }
    return NType::makeUnknown();
  } else if (objectType->kind == TypeKind::Array ||
             objectType->kind == TypeKind::Tensor) {
    if (node->member == "Length") {
      return NType::makeInt();
    }
    if (node->member == "Random") {
      return objectType;
    }
  }

  return NType::makeUnknown();
}

NTypePtr ExpressionAnalyzer::inferIndexExpr(IndexExprNode *node) {
  NTypePtr objectType = m_driver.inferExpression(node->object.get());
  if (!m_driver.flow().validateNonNullExpression(node->object.get(), objectType,
                                                 node->location, "indexing")) {
    return NType::makeError();
  }
  for (auto &index : node->indices) {
    m_driver.inferExpression(index.get());
  }

  if ((objectType->kind == TypeKind::Array ||
       objectType->kind == TypeKind::Tensor) &&
      !objectType->genericArgs.empty()) {
    return objectType->genericArgs[0];
  }
  return NType::makeUnknown();
}

NTypePtr ExpressionAnalyzer::inferSliceExpr(SliceExprNode *node) {
  if (node == nullptr) {
    return NType::makeUnknown();
  }

  NTypePtr objectType = m_driver.inferExpression(node->object.get());
  if (!m_driver.flow().validateNonNullExpression(node->object.get(), objectType,
                                                 node->location, "slicing")) {
    return NType::makeError();
  }
  if (node->start) {
    m_driver.inferExpression(node->start.get());
  }
  if (node->end) {
    m_driver.inferExpression(node->end.get());
  }

  if (objectType->kind == TypeKind::Array ||
      objectType->kind == TypeKind::Tensor) {
    return objectType;
  }
  return NType::makeUnknown();
}

NTypePtr ExpressionAnalyzer::inferTypeofExpr(TypeofExprNode *node) {
  if (node != nullptr && node->expression != nullptr) {
    m_driver.inferExpression(node->expression.get());
  }
  return NType::makeString();
}

NTypePtr ExpressionAnalyzer::inferBinaryExpr(BinaryExprNode *node) {
  NTypePtr left = m_driver.inferExpression(node->left.get());
  NTypePtr right = m_driver.inferExpression(node->right.get());

  if (operatorRequiresNonNullOperands(node->op)) {
    if (!m_driver.flow().validateNonNullExpression(node->left.get(), left,
                                                   node->left->location,
                                                   "binary operator")) {
      return NType::makeError();
    }
    if (!m_driver.flow().validateNonNullExpression(node->right.get(), right,
                                                   node->right->location,
                                                   "binary operator")) {
      return NType::makeError();
    }
  }

  if (node->op == TokenType::Plus || node->op == TokenType::Minus ||
      node->op == TokenType::Star || node->op == TokenType::Slash) {
    if (node->op == TokenType::Plus &&
        (left->isString() || right->isString())) {
      return NType::makeString();
    }
    if (left->isDynamic() || right->isDynamic()) {
      return NType::makeDynamic();
    }
    if (left->isNumeric() && right->isNumeric()) {
      if (left->kind == TypeKind::Double || right->kind == TypeKind::Double) {
        return NType::makeDouble();
      }
      if (left->kind == TypeKind::Float || right->kind == TypeKind::Float) {
        return NType::makeFloat();
      }
      return NType::makeInt();
    }
    if (left->kind == TypeKind::Tensor) {
      return left;
    }
  }

  if (node->op == TokenType::Caret || node->op == TokenType::CaretCaret) {
    if (left->isDynamic() || right->isDynamic()) {
      return NType::makeDynamic();
    }
    if (node->op == TokenType::CaretCaret &&
        right->kind != TypeKind::Int) {
      m_driver.context().error(node->right->location,
                               "^^ right-hand side must be a positive integer");
      return NType::makeError();
    }
    if (isVectorLikeType(left) && right->isNumeric()) {
      return left;
    }
    if (left->isNumeric() && right->isNumeric()) {
      return left->kind == TypeKind::Double ? NType::makeDouble()
                                            : NType::makeFloat();
    }
  }

  if (left->isDynamic() || right->isDynamic()) {
    return NType::makeDynamic();
  }

  if (node->op == TokenType::EqualEqual || node->op == TokenType::NotEqual ||
      node->op == TokenType::Greater || node->op == TokenType::Less ||
      node->op == TokenType::GreaterEqual || node->op == TokenType::LessEqual ||
      node->op == TokenType::And || node->op == TokenType::Or) {
    return NType::makeBool();
  }

  return left;
}

NTypePtr ExpressionAnalyzer::inferUnaryExpr(UnaryExprNode *node) {
  NTypePtr operand = m_driver.inferExpression(node->operand.get());
  if (node->op == TokenType::AddressOf) {
    return NType::makePointer(operand);
  }
  if (node->op == TokenType::ValueOf) {
    if (!m_driver.flow().validateNonNullExpression(node->operand.get(), operand,
                                                   node->location,
                                                   "dereference")) {
      return NType::makeError();
    }
    return operand->kind == TypeKind::Pointer ? operand->pointeeType
                                              : NType::makeError();
  }
  return operand;
}

} // namespace neuron::sema_detail
