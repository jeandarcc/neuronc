#include "BindingAnalyzer.h"

#include "AnalysisHelpers.h"
#include "SemanticDriver.h"

namespace neuron::sema_detail {

namespace {

std::string bindingCallReceiverName(ASTNode *expr) {
  if (expr == nullptr || expr->type != ASTNodeType::CallExpr) {
    return {};
  }
  auto *call = static_cast<CallExprNode *>(expr);
  if (call->callee == nullptr ||
      call->callee->type != ASTNodeType::MemberAccessExpr) {
    return {};
  }
  auto *member = static_cast<MemberAccessNode *>(call->callee.get());
  if (member->object == nullptr || member->object->type != ASTNodeType::Identifier) {
    return {};
  }
  return static_cast<IdentifierNode *>(member->object.get())->name;
}

} // namespace

BindingAnalyzer::BindingAnalyzer(SemanticDriver &driver) : m_driver(driver) {}

void BindingAnalyzer::visit(BindingDeclNode *node) {
  if (node == nullptr) {
    return;
  }

  AnalysisContext &context = m_driver.context();
  if (node->name != "__assign__" && node->name != "__deref__") {
    Symbol *existingSymbol = context.currentScope()->lookup(node->name);
    if (existingSymbol != nullptr &&
        existingSymbol->kind == SymbolKind::Method) {
      context.error(node->location,
                    "Identifier '" + node->name +
                        "' refers to a method. Call it like '" + node->name +
                        "(...)'; if you meant to declare a variable, start the "
                        "name with a lowercase letter or '_'.");
      context.rememberType(node, NType::makeError());
      return;
    }
    m_driver.rules().validateVariableName(node->name, node->location,
                                          node->isConst);
  }

  Symbol *assignmentTarget = nullptr;
  NTypePtr assignmentTargetType = nullptr;
  bool isExistingBindingUpdate = false;
  if (node->name == "__assign__" && node->target != nullptr &&
      node->target->type == ASTNodeType::Identifier) {
    auto *targetId = static_cast<IdentifierNode *>(node->target.get());
    assignmentTarget = context.currentScope()->lookup(targetId->name);
    if (assignmentTarget != nullptr) {
      context.recordReference(assignmentTarget, targetId->location,
                              symbolNameLength(targetId->name));
    }
    if (assignmentTarget != nullptr && assignmentTarget->isConst) {
      context.error(node->location,
                    "Cannot assign to const variable: " + targetId->name);
    }
  } else if (node->name == "__assign__" && node->target != nullptr) {
    assignmentTargetType = m_driver.inferExpression(node->target.get());
  } else if (node->name != "__assign__" && node->name != "__deref__" &&
             context.currentScope() != context.globalScope()) {
    Symbol *existing = context.currentScope()->lookup(node->name);
    if (existing != nullptr &&
        (existing->kind == SymbolKind::Variable ||
         existing->kind == SymbolKind::Parameter)) {
      assignmentTarget = existing;
      isExistingBindingUpdate = true;
      context.recordReference(assignmentTarget, node->location,
                              symbolNameLength(node->name));
      if (assignmentTarget->isConst) {
        context.error(node->location,
                      "Cannot assign to const variable: " + node->name);
      }
    }
  }

  NTypePtr type = assignmentTarget != nullptr
                      ? assignmentTarget->type
                      : (assignmentTargetType != nullptr ? assignmentTargetType
                                                         : NType::makeAuto());

  if (node->typeAnnotation) {
    NTypePtr annotatedType = context.resolveType(node->typeAnnotation.get());
    if (assignmentTarget != nullptr) {
      if (!m_driver.typeChecker().canAssignType(assignmentTarget->type,
                                                annotatedType)) {
        context.error(node->location,
                      "Type mismatch: cannot assign '" +
                          annotatedType->toString() + "' to '" +
                          assignmentTarget->type->toString() + "'");
      }
    } else {
      type = annotatedType;
    }
  }

  if (node->value) {
    NTypePtr inferred = m_driver.inferExpression(node->value.get());

    if (node->kind == BindingKind::AddressOf) {
      inferred = NType::makePointer(inferred);
    } else if (node->kind == BindingKind::ValueOf) {
      if (inferred->kind == TypeKind::Pointer) {
        inferred = inferred->pointeeType;
      } else {
        context.error(node->location,
                      "Cannot use 'value of' on a non-pointer type: " +
                          inferred->toString());
        inferred = NType::makeError();
      }
    } else if (node->kind == BindingKind::MoveFrom) {
      if (node->value->type == ASTNodeType::Identifier) {
        auto *idNode = static_cast<IdentifierNode *>(node->value.get());
        Symbol *sourceSym = context.currentScope()->lookup(idNode->name);
        if (sourceSym != nullptr) {
          context.recordReference(sourceSym, idNode->location,
                                  symbolNameLength(idNode->name));
          sourceSym->isMoved = true;
        }
      } else {
        context.error(node->location, "Can only move from named variables");
      }
    }

    if (node->value->type == ASTNodeType::NullLiteral) {
      if (type->isAuto()) {
        type = NType::makeNullable(NType::makeUnknown());
      } else if (!type->isNullable() && !type->isDynamic() &&
                 !type->isUnknown()) {
        context.error(node->location,
                      "Null cannot be assigned to non-nullable type '" +
                          type->toString() + "'");
      }
    }

    if (type->isAuto()) {
      type = inferred;
    } else if (!type->isError() &&
               !m_driver.typeChecker().canAssignType(type, inferred) &&
               !inferred->isUnknown()) {
      context.error(node->location,
                    "Type mismatch: cannot assign '" + inferred->toString() +
                        "' to '" + type->toString() + "'");
    }
  }

  std::string materialShaderName;
  if (node->value != nullptr && node->value->type == ASTNodeType::CallExpr) {
    auto *call = static_cast<CallExprNode *>(node->value.get());
    if (call->callee != nullptr && call->callee->type == ASTNodeType::TypeSpec) {
      auto *typeSpec = static_cast<TypeSpecNode *>(call->callee.get());
      if (typeSpec->typeName == "Material" && !typeSpec->genericArgs.empty()) {
        ASTNode *genericArg = typeSpec->genericArgs.front().get();
        if (genericArg != nullptr) {
          if (genericArg->type == ASTNodeType::Identifier) {
            materialShaderName =
                static_cast<IdentifierNode *>(genericArg)->name;
          } else if (genericArg->type == ASTNodeType::TypeSpec) {
            materialShaderName =
                static_cast<TypeSpecNode *>(genericArg)->typeName;
          }
        }
      }
    }
  }
  if (!materialShaderName.empty()) {
    type = NType::makeDescriptor(materialShaderName);
  }

  if (node->name != "__assign__" && node->name != "__deref__") {
    if (type->isAuto() && node->value == nullptr) {
      type = NType::makeDynamic();
    }
    if (!isExistingBindingUpdate) {
      Symbol symbol(node->name, SymbolKind::Variable, type);
      symbol.isConst = node->isConst;
      symbol.isMutable = !node->isConst;
      Symbol *defined = context.defineSymbol(context.currentScope(), node->name,
                                             std::move(symbol), &node->location,
                                             symbolNameLength(node->name));
      if (defined == nullptr) {
        context.error(node->location,
                      "Variable already defined in scope: " + node->name);
      } else {
        m_driver.flow().declareSymbol(defined, node->value != nullptr,
                                      node->value.get(), type);
        if (!materialShaderName.empty()) {
          context.registerMaterialShader(context.currentScope(), node->name,
                                         materialShaderName);
        }
        if (type->kind == TypeKind::Class && type->name == "Entity") {
          context.registerEntityBinding(context.currentScope(), node->name);
        } else if (type->kind == TypeKind::Class && type->name == "Transform") {
          const std::string entityName = bindingCallReceiverName(node->value.get());
          if (!entityName.empty()) {
            context.registerTransformBinding(context.currentScope(), node->name,
                                             entityName);
          }
        }
      }
    } else if (assignmentTarget != nullptr) {
      m_driver.flow().assignSymbol(assignmentTarget, node->value.get(), type);
      if (!materialShaderName.empty()) {
        context.registerMaterialShader(context.currentScope(),
                                       assignmentTarget->name,
                                       materialShaderName);
      }
      if (type->kind == TypeKind::Class && type->name == "Entity") {
        context.registerEntityBinding(context.currentScope(), assignmentTarget->name);
      } else if (type->kind == TypeKind::Class && type->name == "Transform") {
        const std::string entityName = bindingCallReceiverName(node->value.get());
        if (!entityName.empty()) {
          context.registerTransformBinding(context.currentScope(),
                                           assignmentTarget->name, entityName);
        }
      }
    }
  }

  context.rememberType(node, type);
}

} // namespace neuron::sema_detail
