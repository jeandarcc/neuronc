#include "LspFeaturePayloads.h"

#include "LspAst.h"
#include "LspHoverSupport.h"
#include "LspPath.h"
#include "LspProtocol.h"

#include <algorithm>

namespace neuron::lsp::detail {

namespace {

int toLspTypeHierarchyKind(ClassKind kind) {
  switch (kind) {
  case ClassKind::Interface:
    return 11;
  case ClassKind::Struct:
    return 23;
  default:
    return 5;
  }
}

std::string classKindText(ClassKind kind) {
  switch (kind) {
  case ClassKind::Interface:
    return "interface";
  case ClassKind::Struct:
    return "struct";
  default:
    return "class";
  }
}

int toLspCompletionItemKind(const VisibleSymbolInfo &symbol) {
  switch (symbol.kind) {
  case SymbolKind::Method:
    return 3;
  case SymbolKind::Constructor:
    return 4;
  case SymbolKind::Field:
    return symbol.isConst ? 21 : 5;
  case SymbolKind::Variable:
    return symbol.isConst ? 21 : 6;
  case SymbolKind::Class:
    return 7;
  case SymbolKind::Module:
    return 9;
  case SymbolKind::Enum:
    return 13;
  case SymbolKind::Parameter:
    return 6;
  case SymbolKind::GenericParameter:
    return 25;
  case SymbolKind::Shader:
    return 7;
  case SymbolKind::Descriptor:
    return 7;
  default:
    return 1;
  }
}

std::string completionDetail(const VisibleSymbolInfo &symbol,
                             const SemanticAnalyzer *analyzer) {
  if (analyzer != nullptr &&
      (symbol.kind == SymbolKind::Method ||
       symbol.kind == SymbolKind::Constructor ||
       symbol.kind == SymbolKind::Class)) {
    std::string signatureKey =
        symbol.signatureKey.empty() ? symbol.name : symbol.signatureKey;
    const std::vector<CallableSignatureInfo> signatures =
        analyzer->getCallableSignatures(signatureKey);
    if (!signatures.empty()) {
      return signatures.front().label;
    }
  }
  return typeToString(symbol.type);
}

std::size_t chooseActiveSignatureIndex(
    const std::vector<CallableSignatureInfo> &signatures,
    std::size_t activeParameter) {
  for (std::size_t index = 0; index < signatures.size(); ++index) {
    if (activeParameter < signatures[index].parameters.size()) {
      return index;
    }
  }
  return 0;
}

} // namespace

std::optional<int> toSemanticTokenType(SymbolKind kind) {
  switch (kind) {
  case SymbolKind::Module:
    return static_cast<int>(LspSemanticTokenType::Namespace);
  case SymbolKind::Class:
  case SymbolKind::Enum:
  case SymbolKind::Shader:
  case SymbolKind::Descriptor:
  case SymbolKind::GenericParameter:
    return static_cast<int>(LspSemanticTokenType::Type);
  case SymbolKind::Method:
  case SymbolKind::Constructor:
    return static_cast<int>(LspSemanticTokenType::Function);
  case SymbolKind::Parameter:
    return static_cast<int>(LspSemanticTokenType::Parameter);
  case SymbolKind::Variable:
    return static_cast<int>(LspSemanticTokenType::Variable);
  case SymbolKind::Field:
    return static_cast<int>(LspSemanticTokenType::Property);
  default:
    return std::nullopt;
  }
}

void appendSemanticToken(std::vector<SemanticTokenEntry> *tokens, int line,
                         int character, int length, int tokenType) {
  if (tokens == nullptr || line < 0 || character < 0 || length <= 0 ||
      tokenType < 0) {
    return;
  }
  tokens->push_back({line, character, length, tokenType, 0});
}

llvm::json::Value encodeSemanticTokens(
    std::vector<SemanticTokenEntry> semanticTokens) {
  std::sort(semanticTokens.begin(), semanticTokens.end(),
            [](const SemanticTokenEntry &lhs, const SemanticTokenEntry &rhs) {
              if (lhs.line != rhs.line) {
                return lhs.line < rhs.line;
              }
              if (lhs.character != rhs.character) {
                return lhs.character < rhs.character;
              }
              if (lhs.length != rhs.length) {
                return lhs.length < rhs.length;
              }
              return lhs.tokenType < rhs.tokenType;
            });
  semanticTokens.erase(
      std::unique(semanticTokens.begin(), semanticTokens.end(),
                  [](const SemanticTokenEntry &lhs,
                     const SemanticTokenEntry &rhs) {
                    return lhs.line == rhs.line &&
                           lhs.character == rhs.character &&
                           lhs.length == rhs.length &&
                           lhs.tokenType == rhs.tokenType;
                  }),
      semanticTokens.end());

  llvm::json::Array data;
  int previousLine = 0;
  int previousCharacter = 0;
  bool first = true;
  for (const SemanticTokenEntry &token : semanticTokens) {
    const int deltaLine = first ? token.line : token.line - previousLine;
    const int deltaStart =
        first || deltaLine != 0 ? token.character
                                : token.character - previousCharacter;
    data.push_back(deltaLine);
    data.push_back(deltaStart);
    data.push_back(token.length);
    data.push_back(token.tokenType);
    data.push_back(token.tokenModifiers);
    previousLine = token.line;
    previousCharacter = token.character;
    first = false;
  }

  llvm::json::Object result;
  result["data"] = std::move(data);
  return result;
}

int toLspSymbolKind(SymbolKind kind) {
  switch (kind) {
  case SymbolKind::Module:
    return 2;
  case SymbolKind::Class:
    return 5;
  case SymbolKind::Method:
    return 6;
  case SymbolKind::Field:
    return 8;
  case SymbolKind::Constructor:
    return 9;
  case SymbolKind::Enum:
    return 10;
  case SymbolKind::Variable:
    return 13;
  case SymbolKind::Parameter:
  case SymbolKind::GenericParameter:
    return 26;
  case SymbolKind::Shader:
  case SymbolKind::Descriptor:
    return 5;
  default:
    return 13;
  }
}

llvm::json::Value makeLspDocumentSymbol(const DocumentSymbolInfo &symbol) {
  llvm::json::Object object;
  object["name"] = symbol.name;
  object["kind"] = toLspSymbolKind(symbol.kind);
  object["range"] = makeLspRange(symbol.range);
  object["selectionRange"] = makeLspRange(symbol.selectionRange);
  llvm::json::Array children;
  for (const auto &child : symbol.children) {
    children.push_back(makeLspDocumentSymbol(child));
  }
  object["children"] = std::move(children);
  return object;
}

llvm::json::Value makeLspWorkspaceSymbol(const WorkspaceSymbolEntry &symbol) {
  llvm::json::Object object;
  object["name"] = symbol.name;
  object["kind"] = toLspSymbolKind(symbol.kind);
  if (!symbol.containerName.empty()) {
    object["containerName"] = symbol.containerName;
  }
  object["location"] = llvm::json::Object{
      {"uri", pathToUri(fs::path(symbol.range.start.file))},
      {"range", makeLspRange(symbol.range)}};
  return object;
}

llvm::json::Value makeLspDiagnostic(const frontend::Diagnostic &diagnostic) {
  llvm::json::Object object;
  object["range"] = makeLspRange(diagnostic.range);
  object["severity"] =
      diagnostic.severity == frontend::DiagnosticSeverity::Warning ? 2 : 1;
  object["code"] = diagnostic.code;
  object["source"] = "neuron-lsp";
  object["message"] = diagnostic.message;
  return object;
}

llvm::json::Object makeHierarchyItemData(const SymbolLocation &location,
                                         std::string_view name) {
  llvm::json::Object data;
  data["uri"] = pathToUri(fs::path(location.location.file));
  data["line"] = location.location.line;
  data["character"] = location.location.column;
  data["length"] = std::max(1, location.length);
  data["name"] = std::string(name);
  return data;
}

std::optional<SymbolLocation>
parseHierarchyItemLocation(const llvm::json::Object *itemObject) {
  if (itemObject == nullptr) {
    return std::nullopt;
  }

  const auto *data = itemObject->getObject("data");
  if (data == nullptr) {
    return std::nullopt;
  }

  const std::optional<llvm::StringRef> uri = data->getString("uri");
  const std::optional<int64_t> line = data->getInteger("line");
  const std::optional<int64_t> character = data->getInteger("character");
  if (!uri.has_value() || !line.has_value() || !character.has_value()) {
    return std::nullopt;
  }

  SymbolLocation location;
  location.location.file = uriToPath(uri->str()).string();
  location.location.line = static_cast<int>(*line);
  location.location.column = static_cast<int>(*character);
  location.length = static_cast<int>(data->getInteger("length").value_or(1));
  return location;
}

llvm::json::Object makeCallHierarchyItem(const VisibleSymbolInfo &symbol,
                                         const SymbolLocation &definition,
                                         const ASTNode *root,
                                         const SemanticAnalyzer *analyzer) {
  SymbolRange selectionRange =
      makePointSymbolRange(definition.location, definition.length);
  SymbolRange range = selectionRange;
  std::string detail = completionDetail(symbol, analyzer);

  if (symbol.kind == SymbolKind::Method || symbol.kind == SymbolKind::Constructor) {
    if (const auto *method = findMethodByLocation(root, definition.location)) {
      selectionRange = makePointSymbolRange(
          method->location, std::max(1, static_cast<int>(method->name.size())));
      range = makeNodeSymbolRange(
          method, std::max(1, static_cast<int>(method->name.size())));
      if (detail.empty()) {
        detail = formatMethodSignature(method);
      }
    }
  } else if (symbol.kind == SymbolKind::Class) {
    if (const auto *classDecl = findClassByLocation(root, definition.location)) {
      selectionRange = makePointSymbolRange(
          classDecl->location,
          std::max(1, static_cast<int>(classDecl->name.size())));
      range = makeNodeSymbolRange(
          classDecl, std::max(1, static_cast<int>(classDecl->name.size())));
      if (detail.empty()) {
        detail = classKindText(classDecl->kind);
      }
    }
  }

  llvm::json::Object object;
  object["name"] = symbol.name;
  object["kind"] = toLspSymbolKind(symbol.kind);
  object["detail"] = detail;
  object["uri"] = pathToUri(fs::path(definition.location.file));
  object["range"] = makeLspRange(range);
  object["selectionRange"] = makeLspRange(selectionRange);
  object["data"] = makeHierarchyItemData(definition, symbol.name);
  return object;
}

llvm::json::Object makeTypeHierarchyItem(const WorkspaceTypeRecord &record) {
  const SymbolLocation definition = {
      {record.location.line, record.location.column, record.path.string()},
      std::max(1, static_cast<int>(record.name.size()))};
  const SymbolRange selectionRange =
      makePointSymbolRange(definition.location, definition.length);

  llvm::json::Object object;
  object["name"] = record.name;
  object["kind"] = toLspTypeHierarchyKind(record.kind);
  object["detail"] = classKindText(record.kind);
  object["uri"] = pathToUri(record.path);
  object["range"] = makeLspRange(selectionRange);
  object["selectionRange"] = makeLspRange(selectionRange);
  object["data"] = makeHierarchyItemData(definition, record.name);
  return object;
}

llvm::json::Value makeLspCompletionItem(const VisibleSymbolInfo &symbol,
                                        const SemanticAnalyzer *analyzer) {
  llvm::json::Object object;
  object["label"] = symbol.name;
  object["kind"] = toLspCompletionItemKind(symbol);
  const std::string detail = completionDetail(symbol, analyzer);
  if (!detail.empty()) {
    object["detail"] = detail;
  }
  return object;
}

llvm::json::Value makeLspSignatureHelp(
    const std::vector<CallableSignatureInfo> &signatures,
    std::size_t activeParameter) {
  llvm::json::Object object;
  llvm::json::Array signatureArray;
  const std::size_t activeSignature =
      chooseActiveSignatureIndex(signatures, activeParameter);

  for (const auto &signature : signatures) {
    llvm::json::Object signatureObject;
    signatureObject["label"] = signature.label;

    llvm::json::Array parameterArray;
    for (const auto &parameter : signature.parameters) {
      std::string label = parameter.name;
      if (!parameter.typeName.empty()) {
        label += " as " + parameter.typeName;
      }
      parameterArray.push_back(llvm::json::Object{{"label", label}});
    }
    signatureObject["parameters"] = std::move(parameterArray);
    signatureArray.push_back(std::move(signatureObject));
  }

  object["signatures"] = std::move(signatureArray);
  object["activeSignature"] = static_cast<int>(activeSignature);
  if (signatures[activeSignature].parameters.empty()) {
    object["activeParameter"] = 0;
  } else {
    object["activeParameter"] = static_cast<int>(
        std::min(activeParameter, signatures[activeSignature].parameters.size() - 1));
  }
  return object;
}

llvm::json::Object makeServerCapabilities() {
  llvm::json::Object capabilities;
  capabilities["textDocumentSync"] =
      llvm::json::Object{{"openClose", true}, {"change", 2}};
  capabilities["hoverProvider"] = true;
  capabilities["definitionProvider"] = true;
  capabilities["referencesProvider"] = true;
  capabilities["renameProvider"] = true;
  capabilities["callHierarchyProvider"] = true;
  capabilities["typeHierarchyProvider"] = true;
  capabilities["inlayHintProvider"] = true;
  capabilities["codeLensProvider"] =
      llvm::json::Object{{"resolveProvider", false}};
  capabilities["documentSymbolProvider"] = true;
  capabilities["workspaceSymbolProvider"] = true;
  capabilities["codeActionProvider"] =
      llvm::json::Object{{"codeActionKinds", llvm::json::Array{"quickfix"}}};
  llvm::json::Array semanticTokenTypes;
  for (const char *tokenType : kSemanticTokenLegend) {
    semanticTokenTypes.push_back(tokenType);
  }
  capabilities["semanticTokensProvider"] = llvm::json::Object{
      {"legend", llvm::json::Object{{"tokenTypes", std::move(semanticTokenTypes)},
                                    {"tokenModifiers", llvm::json::Array{}}}},
      {"full", true}};
  capabilities["completionProvider"] = llvm::json::Object{
      {"resolveProvider", false},
      {"triggerCharacters", llvm::json::Array{"."}}};
  capabilities["signatureHelpProvider"] = llvm::json::Object{
      {"triggerCharacters", llvm::json::Array{"(", ","}},
      {"retriggerCharacters", llvm::json::Array{","}}};
  return capabilities;
}

} // namespace neuron::lsp::detail
