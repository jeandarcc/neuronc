#include "TypeChecker.h"

#include "AnalysisContext.h"

namespace neuron::sema_detail {

namespace {

enum class CastCompatibility {
  Safe,
  RuntimeChecked,
  Invalid,
};

NTypePtr unwrapNullableType(const NTypePtr &type) {
  if (type && type->isNullable() && type->nullableBase()) {
    return type->nullableBase();
  }
  return type;
}

bool isNullableType(const NTypePtr &type) {
  return type && type->isNullable();
}

bool isNullLiteralType(const NTypePtr &type) {
  return type && type->isNullable() && type->nullableBase() != nullptr &&
         type->nullableBase()->isUnknown();
}

NTypePtr makeNullableIfNeeded(NTypePtr type, bool nullable) {
  if (!nullable || !type || type->isNullable()) {
    return type;
  }
  return NType::makeNullable(type);
}

bool stringLiteralCanConvert(ASTNode *expr, const NTypePtr &target) {
  if (expr == nullptr || expr->type != ASTNodeType::StringLiteral || !target) {
    return true;
  }

  const std::string &value = static_cast<StringLiteralNode *>(expr)->value;
  try {
    switch (target->kind) {
    case TypeKind::Int:
      (void)std::stoll(value);
      return true;
    case TypeKind::Float:
    case TypeKind::Double:
      (void)std::stod(value);
      return true;
    case TypeKind::Bool:
      return value == "true" || value == "false";
    default:
      return true;
    }
  } catch (...) {
    return false;
  }
}

CastCompatibility classifyCast(const NTypePtr &source, const NTypePtr &target) {
  if (!source || !target) {
    return CastCompatibility::RuntimeChecked;
  }
  if (source->isError() || target->isError()) {
    return CastCompatibility::Invalid;
  }
  if (source->isUnknown() || target->isUnknown() || source->isAuto() ||
      target->isAuto()) {
    return CastCompatibility::RuntimeChecked;
  }

  const bool sourceNullable = isNullableType(source);
  const bool targetNullable = isNullableType(target);
  NTypePtr sourceBase = unwrapNullableType(source);
  NTypePtr targetBase = unwrapNullableType(target);
  if (!sourceBase || !targetBase) {
    return CastCompatibility::RuntimeChecked;
  }

  if (targetBase->isDynamic()) {
    return CastCompatibility::Safe;
  }
  if (sourceBase->isDynamic()) {
    return CastCompatibility::RuntimeChecked;
  }
  if (sourceBase->equals(*targetBase)) {
    return sourceNullable && !targetNullable ? CastCompatibility::RuntimeChecked
                                             : CastCompatibility::Safe;
  }
  if (sourceBase->isNumeric() && targetBase->isNumeric()) {
    return CastCompatibility::Safe;
  }
  if (targetBase->isString()) {
    if (sourceBase->isNumeric() || sourceBase->isBool() ||
        sourceBase->kind == TypeKind::Enum) {
      return CastCompatibility::Safe;
    }
  }
  if (sourceBase->isString()) {
    if (targetBase->isNumeric() || targetBase->isBool() ||
        targetBase->kind == TypeKind::Enum) {
      return CastCompatibility::RuntimeChecked;
    }
  }
  if (sourceBase->kind == TypeKind::Enum &&
      (targetBase->kind == TypeKind::Int ||
       targetBase->kind == TypeKind::String)) {
    return CastCompatibility::Safe;
  }
  if (targetBase->kind == TypeKind::Enum &&
      (sourceBase->kind == TypeKind::Int || sourceBase->isString())) {
    return CastCompatibility::RuntimeChecked;
  }

  return CastCompatibility::Invalid;
}

} // namespace

bool TypeChecker::canAssignType(const NTypePtr &target,
                                const NTypePtr &source) const {
  if (!target || !source) {
    return true;
  }
  if (target->isDynamic() || source->isDynamic() || target->isUnknown() ||
      source->isUnknown() || target->isAuto() || source->isAuto()) {
    return true;
  }
  if (isNullLiteralType(source)) {
    return target->isNullable() || target->isDynamic() || target->isUnknown() ||
           target->isAuto();
  }
  if (target->equals(*source)) {
    return true;
  }
  if (target->kind == TypeKind::Descriptor &&
      source->kind == TypeKind::Class &&
      source->className == "Material") {
    return true;
  }
  if (target->isNullable()) {
    NTypePtr base = target->nullableBase();
    if (base && (base->equals(*source) ||
                 (source->isNullable() && source->nullableBase() &&
                  base->equals(*source->nullableBase())))) {
      return true;
    }
  }
  return false;
}

NTypePtr TypeChecker::applyCastPipeline(AnalysisContext &context,
                                        NTypePtr sourceType,
                                        const CastStmtNode *node,
                                        ASTNode *sourceExpr) const {
  if (node == nullptr || node->steps.empty()) {
    return sourceType ? sourceType : NType::makeUnknown();
  }

  NTypePtr currentType = sourceType ? sourceType : NType::makeUnknown();
  bool nullableResult = isNullableType(currentType);

  for (std::size_t i = 0; i < node->steps.size(); ++i) {
    const CastStepNode &step = node->steps[i];
    NTypePtr targetType = context.resolveType(step.typeSpec.get());
    if (targetType->isError()) {
      return NType::makeError();
    }

    const bool allowNull = node->pipelineNullable || step.allowNullOnFailure;
    const CastCompatibility compatibility =
        classifyCast(currentType, targetType);

    if (compatibility == CastCompatibility::Invalid) {
      if (!allowNull) {
        context.error(step.location, "Invalid cast step " +
                                         std::to_string(i + 1) +
                                         ": cannot cast '" +
                                         currentType->toString() + "' to '" +
                                         targetType->toString() + "'");
        return NType::makeError();
      }
      nullableResult = true;
    } else if (compatibility == CastCompatibility::RuntimeChecked) {
      if (!allowNull && !stringLiteralCanConvert(sourceExpr, targetType)) {
        context.error(step.location,
                      "Invalid cast step " + std::to_string(i + 1) +
                          ": literal cannot be converted to '" +
                          targetType->toString() + "'");
        return NType::makeError();
      }
      if (allowNull) {
        nullableResult = true;
      }
    }

    currentType = targetType;
  }

  return makeNullableIfNeeded(currentType, nullableResult);
}

} // namespace neuron::sema_detail
