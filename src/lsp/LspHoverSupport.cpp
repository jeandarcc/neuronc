#include "LspHoverSupport.h"

#include "LspAst.h"
#include "LspPath.h"
#include "LspText.h"

#include <optional>
#include <sstream>

namespace neuron::lsp::detail {

namespace {

int nodePriority(const ASTNode *node) {
  if (node == nullptr) {
    return -1;
  }
  switch (node->type) {
  case ASTNodeType::BindingDecl:
    return 100;
  case ASTNodeType::MethodDecl:
    return 90;
  case ASTNodeType::ClassDecl:
  case ASTNodeType::EnumDecl:
  case ASTNodeType::ShaderDecl:
    return 80;
  case ASTNodeType::ModuleDecl:
  case ASTNodeType::ModuleCppDecl:
    return 70;
  case ASTNodeType::TypeSpec:
    return 60;
  case ASTNodeType::MemberAccessExpr:
    return 55;
  case ASTNodeType::Identifier:
    return 50;
  default:
    return 10;
  }
}

} // namespace

std::string typeToString(const NTypePtr &type) {
  return type ? type->toString() : std::string("<unknown>");
}

std::string formatMethodSignature(const MethodDeclNode *method) {
  std::ostringstream out;
  out << method->name << " is method(";
  for (std::size_t i = 0; i < method->parameters.size(); ++i) {
    const auto &param = method->parameters[i];
    out << param.name;
    if (param.typeSpec != nullptr && param.typeSpec->type == ASTNodeType::TypeSpec) {
      out << " as "
          << static_cast<const TypeSpecNode *>(param.typeSpec.get())->typeName;
    }
    if (i + 1 < method->parameters.size()) {
      out << ", ";
    }
  }
  out << ")";
  if (method->returnType != nullptr &&
      method->returnType->type == ASTNodeType::TypeSpec) {
    out << " as "
        << static_cast<const TypeSpecNode *>(method->returnType.get())->typeName;
  }
  return out.str();
}

const ASTNode *findBestNodeAt(const DocumentState &document, int line, int column,
                              const TokenMatch &match) {
  const ASTNode *best = match.memberAccess;
  int bestPriority = nodePriority(best);

  const auto considerNode = [&](const ASTNode *node) {
    if (node == nullptr || node->location.line != line ||
        node->location.column != column) {
      return;
    }
    const int priority = nodePriority(node);
    if (priority > bestPriority) {
      best = node;
      bestPriority = priority;
    }
  };

  for (const auto &chunk : document.chunks) {
    if (chunk.parseResult.program == nullptr) {
      continue;
    }
    walkAst(chunk.parseResult.program.get(), considerNode);
  }
  return best;
}

TokenMatch findTokenAt(const DocumentState &document, int line, int character) {
  const int targetLine = line + 1;
  const int targetColumn = character + 1;
  TokenMatch match;

  for (const auto &chunk : document.chunks) {
    for (const auto &token : chunk.parseResult.tokens) {
      if (token.type == TokenType::Eof || token.location.line != targetLine) {
        continue;
      }
      const int tokenLength = std::max(
          1, static_cast<int>(token.value.empty() ? 1 : token.value.size()));
      if (targetColumn >= token.location.column &&
          targetColumn < token.location.column + tokenLength) {
        match.token = &token;
        break;
      }
    }
    if (match.token != nullptr) {
      break;
    }
  }

  if (match.token == nullptr || match.token->type != TokenType::Identifier) {
    return match;
  }

  const auto memberMatches = [&](const ASTNode *node) {
    if (node == nullptr || node->type != ASTNodeType::MemberAccessExpr) {
      return;
    }
    const auto *member = static_cast<const MemberAccessNode *>(node);
    if (member->member == match.token->value &&
        member->memberLocation.line == match.token->location.line &&
        member->memberLocation.column == match.token->location.column) {
      match.memberAccess = node;
    }
  };

  for (const auto &chunk : document.chunks) {
    if (chunk.parseResult.program == nullptr) {
      continue;
    }
    walkAst(chunk.parseResult.program.get(), memberMatches);
    if (match.memberAccess != nullptr) {
      break;
    }
  }

  return match;
}

std::optional<std::string> buildHoverMarkup(const ASTNode *node,
                                            const SemanticAnalyzer *analyzer) {
  if (node == nullptr) {
    return std::nullopt;
  }

  switch (node->type) {
  case ASTNodeType::BindingDecl: {
    const auto *binding = static_cast<const BindingDeclNode *>(node);
    std::string value = binding->name;
    if (binding->isConst) {
      value = "const " + value;
    }
    if (analyzer != nullptr) {
      value += ": " + typeToString(analyzer->getInferredType(node));
    }
    return "```npp\n" + value + "\n```";
  }
  case ASTNodeType::MethodDecl:
    return "```npp\n" +
           formatMethodSignature(static_cast<const MethodDeclNode *>(node)) +
           "\n```";
  case ASTNodeType::ClassDecl: {
    const auto *classDecl = static_cast<const ClassDeclNode *>(node);
    std::string kind = "class";
    if (classDecl->kind == ClassKind::Struct) {
      kind = "struct";
    } else if (classDecl->kind == ClassKind::Interface) {
      kind = "interface";
    }
    return "```npp\n" + classDecl->name + " is " + kind + "\n```";
  }
  case ASTNodeType::EnumDecl:
    return "```npp\n" + static_cast<const EnumDeclNode *>(node)->name +
           " is enum\n```";
  case ASTNodeType::ShaderDecl:
    return "```npp\n" + static_cast<const ShaderDeclNode *>(node)->name +
           " is shader\n```";
  case ASTNodeType::ModuleDecl:
    return "```npp\nmodule " +
           static_cast<const ModuleDeclNode *>(node)->moduleName + ";\n```";
  case ASTNodeType::ModuleCppDecl:
    return "```npp\nmodulecpp " +
           static_cast<const ModuleCppDeclNode *>(node)->moduleName + ";\n```";
  case ASTNodeType::Identifier: {
    const auto *identifier = static_cast<const IdentifierNode *>(node);
    if (identifier->name == "__missing__") {
      return std::nullopt;
    }
    if (analyzer != nullptr) {
      return "```npp\n" + identifier->name + ": " +
             typeToString(analyzer->getInferredType(node)) + "\n```";
    }
    break;
  }
  case ASTNodeType::MemberAccessExpr: {
    const auto *member = static_cast<const MemberAccessNode *>(node);
    if (analyzer != nullptr) {
      return "```npp\n" + member->member + ": " +
             typeToString(analyzer->getInferredType(node)) + "\n```";
    }
    break;
  }
  case ASTNodeType::TypeSpec:
    return "```npp\ntype " +
           static_cast<const TypeSpecNode *>(node)->typeName + "\n```";
  default:
    break;
  }

  if (analyzer != nullptr) {
    if (NTypePtr type = analyzer->getInferredType(node)) {
      return "```npp\n" + typeToString(type) + "\n```";
    }
  }
  return std::nullopt;
}

std::optional<VisibleSymbolInfo>
resolveSymbolAtPosition(const DocumentState &document, int line, int character,
                        SourceLocation *outLocation, const ASTNode **outNode) {
  if (document.analyzer == nullptr) {
    return std::nullopt;
  }

  const TokenMatch tokenMatch = findTokenAt(document, line, character);
  const ASTNode *node =
      findBestNodeAt(document, line + 1, character + 1, tokenMatch);
  if (outNode != nullptr) {
    *outNode = node;
  }

  SourceLocation lookupLocation = {line + 1, character + 1, document.path.string()};
  if (tokenMatch.memberAccess != nullptr) {
    lookupLocation =
        static_cast<const MemberAccessNode *>(tokenMatch.memberAccess)->memberLocation;
  } else if (tokenMatch.token != nullptr &&
             tokenMatch.token->type == TokenType::Identifier) {
    lookupLocation = tokenMatch.token->location;
  } else if (node != nullptr) {
    lookupLocation = node->location;
  }

  if (outLocation != nullptr) {
    *outLocation = lookupLocation;
  }

  return document.analyzer->getResolvedSymbol(lookupLocation);
}

} // namespace neuron::lsp::detail
