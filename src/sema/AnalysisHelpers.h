#pragma once

#include "neuronc/parser/AST.h"
#include "neuronc/sema/TypeSystem.h"

#include <string>
#include <string_view>
#include <vector>

namespace neuron::sema_detail {

struct InputChainInfo {
  TypeSpecNode *inputTypeSpec = nullptr;
  std::vector<CallExprNode *> calls;
  std::vector<std::string> methods;
};

bool isUpperSnakeCase(const std::string &value);
std::string resolveCallName(ASTNode *callee);
std::string normalizeModuleName(const std::string &name);
int symbolNameLength(std::string_view name);
std::string typeDisplayName(const NTypePtr &type);
SourceLocation blockEndLocation(const BlockNode *block);
SourceLocation nodeEndLocation(const ASTNode *node);
CanvasEventKind canvasEventFromName(const std::string &name);
bool collectInputCallChain(CallExprNode *node, InputChainInfo *info);

} // namespace neuron::sema_detail
