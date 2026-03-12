#include "InputAnalyzer.h"

#include "AnalysisHelpers.h"
#include "SemanticDriver.h"

#include <vector>

namespace neuron::sema_detail {

namespace {

struct InputStageView {
  std::string method;
  std::vector<ASTNode *> arguments;
  SourceLocation location;
};

NTypePtr inferInputValueType(SemanticDriver &driver, const SourceLocation &loc,
                             const std::vector<ASTNode *> &typeArguments) {
  AnalysisContext &context = driver.context();
  if (typeArguments.size() > 1) {
    context.error(loc, "Input<T> requires exactly one generic type argument.");
    return NType::makeError();
  }

  auto resolveGenericArg = [&](ASTNode *argNode) -> NTypePtr {
    if (argNode == nullptr) {
      return NType::makeError();
    }
    if (argNode->type == ASTNodeType::Identifier) {
      return context.resolveType(static_cast<IdentifierNode *>(argNode)->name);
    }
    if (argNode->type == ASTNodeType::TypeSpec) {
      return context.resolveType(argNode);
    }
    return NType::makeError();
  };

  NTypePtr inputValueType =
      typeArguments.empty() ? NType::makeString()
                            : resolveGenericArg(typeArguments.front());
  if (inputValueType->isError() || inputValueType->isUnknown()) {
    context.error(loc, "Input<T> has unsupported generic type parameter.");
    return NType::makeError();
  }

  const bool supportedInputType =
      inputValueType->kind == TypeKind::Int ||
      inputValueType->kind == TypeKind::Float ||
      inputValueType->kind == TypeKind::Double ||
      inputValueType->kind == TypeKind::Bool ||
      inputValueType->kind == TypeKind::String ||
      inputValueType->kind == TypeKind::Enum;
  if (!supportedInputType) {
    context.error(
        loc,
        "Input<T> only supports int, float, double, bool, string, and enum "
        "types.");
    return NType::makeError();
  }

  return inputValueType;
}

NTypePtr inferInputExpression(SemanticDriver &driver, const SourceLocation &loc,
                              const std::vector<ASTNode *> &typeArguments,
                              const std::vector<ASTNode *> &promptArguments,
                              const std::vector<InputStageView> &stages) {
  AnalysisContext &context = driver.context();
  NTypePtr inputValueType = inferInputValueType(driver, loc, typeArguments);
  if (inputValueType->isError()) {
    return inputValueType;
  }

  if (promptArguments.size() != 1) {
    context.error(loc, "Input<T> expects exactly one prompt argument.");
  } else {
    NTypePtr promptType = driver.inferExpression(promptArguments.front());
    if (!promptType->isString() && !promptType->isUnknown() &&
        !promptType->isDynamic()) {
      context.error(promptArguments.front()->location,
                    "Input<T> prompt argument must be string.");
    }
  }

  for (const auto &stage : stages) {
    if (stage.method == "Min" || stage.method == "Max") {
      if (!inputValueType->isNumeric()) {
        context.error(stage.location,
                      "Input<T>." + stage.method +
                          "() is only valid for numeric T.");
      }
      if (stage.arguments.size() != 1) {
        context.error(stage.location, "Input<T>." + stage.method +
                                          "() expects exactly one argument.");
        continue;
      }
      NTypePtr argType = driver.inferExpression(stage.arguments.front());
      if (!argType->isNumeric() && !argType->isUnknown() &&
          !argType->isDynamic()) {
        context.error(stage.arguments.front()->location,
                      "Input<T>." + stage.method +
                          "() argument must be numeric.");
      }
      continue;
    }

    if (stage.method == "Default") {
      if (stage.arguments.size() != 1) {
        context.error(stage.location,
                      "Input<T>.Default() expects exactly one argument.");
        continue;
      }
      NTypePtr argType = driver.inferExpression(stage.arguments.front());
      if (!driver.typeChecker().canAssignType(inputValueType, argType) &&
          !argType->isUnknown()) {
        context.error(stage.arguments.front()->location,
                      "Input<T>.Default() argument type mismatch for T = '" +
                          inputValueType->toString() + "'");
      }
      continue;
    }

    if (stage.method == "TimeoutMs") {
      if (stage.arguments.size() != 1) {
        context.error(stage.location,
                      "Input<T>.TimeoutMs() expects exactly one argument.");
        continue;
      }
      NTypePtr argType = driver.inferExpression(stage.arguments.front());
      if (argType->kind != TypeKind::Int && !argType->isUnknown() &&
          !argType->isDynamic()) {
        context.error(stage.arguments.front()->location,
                      "Input<T>.TimeoutMs() argument must be int.");
      }
      continue;
    }

    if (stage.method == "Secret") {
      if (!inputValueType->isString()) {
        context.error(stage.location,
                      "Input<T>.Secret() is only valid for T = string.");
      }
      if (!stage.arguments.empty()) {
        context.error(stage.location, "Input<T>.Secret() takes no arguments.");
        for (ASTNode *arg : stage.arguments) {
          driver.inferExpression(arg);
        }
      }
      continue;
    }

    context.error(stage.location,
                  "Unsupported Input<T> fluent method: " + stage.method);
    for (ASTNode *arg : stage.arguments) {
      driver.inferExpression(arg);
    }
  }

  return inputValueType;
}

} // namespace

InputAnalyzer::InputAnalyzer(SemanticDriver &driver) : m_driver(driver) {}

NTypePtr InputAnalyzer::infer(InputExprNode *node) {
  if (node == nullptr) {
    return NType::makeError();
  }

  std::vector<ASTNode *> typeArguments;
  typeArguments.reserve(node->typeArguments.size());
  for (const auto &arg : node->typeArguments) {
    typeArguments.push_back(arg.get());
  }

  std::vector<ASTNode *> promptArguments;
  promptArguments.reserve(node->promptArguments.size());
  for (const auto &arg : node->promptArguments) {
    promptArguments.push_back(arg.get());
  }

  std::vector<InputStageView> stages;
  stages.reserve(node->stages.size());
  for (const auto &stage : node->stages) {
    InputStageView view;
    view.method = stage.method;
    view.location = stage.location;
    for (const auto &arg : stage.arguments) {
      view.arguments.push_back(arg.get());
    }
    stages.push_back(std::move(view));
  }

  const SourceLocation typeLocation =
      !node->typeArguments.empty() ? node->typeArguments.front()->location
                                   : node->location;
  return inferInputExpression(m_driver, typeLocation, typeArguments,
                              promptArguments, stages);
}

NTypePtr InputAnalyzer::tryInferCallChain(CallExprNode *node) {
  InputChainInfo info;
  if (!collectInputCallChain(node, &info) || info.calls.empty()) {
    return nullptr;
  }

  std::vector<ASTNode *> typeArguments;
  if (info.inputTypeSpec != nullptr) {
    typeArguments.reserve(info.inputTypeSpec->genericArgs.size());
    for (const auto &arg : info.inputTypeSpec->genericArgs) {
      typeArguments.push_back(arg.get());
    }
  }

  std::vector<ASTNode *> promptArguments;
  promptArguments.reserve(info.calls.front()->arguments.size());
  for (const auto &arg : info.calls.front()->arguments) {
    promptArguments.push_back(arg.get());
  }

  std::vector<InputStageView> stages;
  stages.reserve(info.methods.size());
  for (std::size_t i = 1; i < info.calls.size(); ++i) {
    InputStageView view;
    view.method = info.methods[i - 1];
    view.location = info.calls[i]->location;
    for (const auto &arg : info.calls[i]->arguments) {
      view.arguments.push_back(arg.get());
    }
    stages.push_back(std::move(view));
  }

  const SourceLocation typeLocation =
      info.inputTypeSpec != nullptr ? info.inputTypeSpec->location
                                    : node->location;
  return inferInputExpression(m_driver, typeLocation, typeArguments,
                              promptArguments, stages);
}

} // namespace neuron::sema_detail
