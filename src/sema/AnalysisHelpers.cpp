#include "AnalysisHelpers.h"

#include <algorithm>
#include <cctype>

namespace neuron::sema_detail {

bool isUpperSnakeCase(const std::string &value) {
  if (value.empty()) {
    return false;
  }

  bool hasLetter = false;
  for (char ch : value) {
    const unsigned char c = static_cast<unsigned char>(ch);
    if (std::isupper(c)) {
      hasLetter = true;
      continue;
    }
    if (std::isdigit(c) || ch == '_') {
      continue;
    }
    return false;
  }

  return hasLetter;
}

std::string resolveCallName(ASTNode *callee) {
  if (callee == nullptr) {
    return "";
  }
  if (callee->type == ASTNodeType::Identifier) {
    return static_cast<IdentifierNode *>(callee)->name;
  }
  if (callee->type == ASTNodeType::TypeSpec) {
    return static_cast<TypeSpecNode *>(callee)->typeName;
  }
  if (callee->type == ASTNodeType::MemberAccessExpr) {
    auto *member = static_cast<MemberAccessNode *>(callee);
    std::string base = resolveCallName(member->object.get());
    if (base.empty()) {
      return "";
    }
    return base + "." + member->member;
  }
  return "";
}

std::string normalizeModuleName(const std::string &name) {
  std::string normalized = name;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  return normalized;
}

int symbolNameLength(std::string_view name) {
  return std::max(1, static_cast<int>(name.size()));
}

std::string typeDisplayName(const NTypePtr &type) {
  return type ? type->toString() : std::string("<unknown>");
}

SourceLocation blockEndLocation(const BlockNode *block) {
  if (block == nullptr) {
    return {};
  }
  return {block->endLine, block->endColumn, block->location.file};
}

SourceLocation nodeEndLocation(const ASTNode *node) {
  if (node == nullptr) {
    return {};
  }
  if (node->type == ASTNodeType::Block) {
    return blockEndLocation(static_cast<const BlockNode *>(node));
  }
  if (node->type == ASTNodeType::MethodDecl) {
    const auto *method = static_cast<const MethodDeclNode *>(node);
    if (method->body != nullptr && method->body->type == ASTNodeType::Block) {
      return blockEndLocation(static_cast<const BlockNode *>(method->body.get()));
    }
  }
  return node->location;
}

CanvasEventKind canvasEventFromName(const std::string &name) {
  const std::string lowered = normalizeModuleName(name);
  if (lowered == "onopen") {
    return CanvasEventKind::OnOpen;
  }
  if (lowered == "onframe") {
    return CanvasEventKind::OnFrame;
  }
  if (lowered == "onresize") {
    return CanvasEventKind::OnResize;
  }
  if (lowered == "onclose") {
    return CanvasEventKind::OnClose;
  }
  return CanvasEventKind::Unknown;
}

bool collectInputCallChain(CallExprNode *node, InputChainInfo *info) {
  if (node == nullptr || info == nullptr || node->callee == nullptr) {
    return false;
  }

  if (node->callee->type == ASTNodeType::Identifier) {
    auto *identifier = static_cast<IdentifierNode *>(node->callee.get());
    if (identifier->name != "Input") {
      return false;
    }
    info->calls.push_back(node);
    return true;
  }

  if (node->callee->type == ASTNodeType::TypeSpec) {
    auto *typeSpec = static_cast<TypeSpecNode *>(node->callee.get());
    if (typeSpec->typeName != "Input") {
      return false;
    }
    info->inputTypeSpec = typeSpec;
    info->calls.push_back(node);
    return true;
  }

  if (node->callee->type != ASTNodeType::MemberAccessExpr) {
    return false;
  }

  auto *member = static_cast<MemberAccessNode *>(node->callee.get());
  if (member->object == nullptr ||
      member->object->type != ASTNodeType::CallExpr) {
    return false;
  }

  if (!collectInputCallChain(static_cast<CallExprNode *>(member->object.get()),
                             info)) {
    return false;
  }

  info->calls.push_back(node);
  info->methods.push_back(member->member);
  return true;
}

} // namespace neuron::sema_detail
