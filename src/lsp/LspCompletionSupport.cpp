#include "LspCompletionSupport.h"

#include "LspPath.h"
#include "LspText.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace neuron::lsp::detail {

namespace {

struct CompletionContext {
  std::string prefix;
  bool isMemberAccess = false;
  std::vector<std::string> receiverChain;
};

struct CallFrame {
  std::vector<std::string> callableChain;
  std::size_t argumentIndex = 0;
  int bracketDepth = 0;
  int braceDepth = 0;
};

bool isIdentifierChar(char ch) {
  const unsigned char value = static_cast<unsigned char>(ch);
  return std::isalnum(value) || ch == '_';
}

std::vector<std::string> extractIdentifierChainBeforeOffset(
    const std::string &text, std::size_t endOffset) {
  std::vector<std::string> reversed;
  std::size_t cursor = std::min(endOffset, text.size());

  while (true) {
    while (cursor > 0 &&
           std::isspace(static_cast<unsigned char>(text[cursor - 1]))) {
      --cursor;
    }

    const std::size_t identEnd = cursor;
    while (cursor > 0 && isIdentifierChar(text[cursor - 1])) {
      --cursor;
    }
    if (cursor == identEnd) {
      break;
    }

    reversed.push_back(text.substr(cursor, identEnd - cursor));
    while (cursor > 0 &&
           std::isspace(static_cast<unsigned char>(text[cursor - 1]))) {
      --cursor;
    }
    if (cursor == 0 || text[cursor - 1] != '.') {
      break;
    }
    --cursor;
  }

  std::reverse(reversed.begin(), reversed.end());
  return reversed;
}

CompletionContext buildCompletionContext(const std::string &text,
                                         std::size_t cursorOffset) {
  CompletionContext context;
  const std::size_t cursor = std::min(cursorOffset, text.size());
  std::size_t prefixStart = cursor;

  while (prefixStart > 0 && isIdentifierChar(text[prefixStart - 1])) {
    --prefixStart;
  }
  context.prefix = text.substr(prefixStart, cursor - prefixStart);

  std::size_t lookBehind = prefixStart;
  while (lookBehind > 0 &&
         std::isspace(static_cast<unsigned char>(text[lookBehind - 1]))) {
    --lookBehind;
  }
  if (lookBehind > 0 && text[lookBehind - 1] == '.') {
    context.isMemberAccess = true;
    context.receiverChain =
        extractIdentifierChainBeforeOffset(text, lookBehind - 1);
  }

  return context;
}

std::optional<VisibleSymbolInfo>
findVisibleSymbol(const std::vector<VisibleSymbolInfo> &symbols,
                  std::string_view name) {
  auto it = std::find_if(symbols.begin(), symbols.end(),
                         [&](const VisibleSymbolInfo &symbol) {
                           return symbol.name == name;
                         });
  if (it == symbols.end()) {
    return std::nullopt;
  }
  return *it;
}

std::optional<VisibleSymbolInfo> resolveVisibleSymbolChain(
    const SemanticAnalyzer *analyzer,
    const std::vector<VisibleSymbolInfo> &visibleSymbols,
    const std::vector<std::string> &chain) {
  if (analyzer == nullptr || chain.empty()) {
    return std::nullopt;
  }

  std::optional<VisibleSymbolInfo> current =
      findVisibleSymbol(visibleSymbols, chain.front());
  if (!current.has_value()) {
    return std::nullopt;
  }

  for (std::size_t index = 1; index < chain.size(); ++index) {
    const std::vector<VisibleSymbolInfo> members =
        analyzer->getTypeMembers(current->type);
    current = findVisibleSymbol(members, chain[index]);
    if (!current.has_value()) {
      return std::nullopt;
    }
  }

  return current;
}

} // namespace

std::vector<CallableSignatureInfo>
resolveCallableSignatures(const SemanticAnalyzer *analyzer,
                          const std::vector<VisibleSymbolInfo> &visibleSymbols,
                          const std::vector<std::string> &callableChain) {
  if (analyzer == nullptr || callableChain.empty()) {
    return {};
  }

  const std::optional<VisibleSymbolInfo> symbol =
      resolveVisibleSymbolChain(analyzer, visibleSymbols, callableChain);
  if (!symbol.has_value()) {
    return {};
  }

  std::string signatureKey = symbol->signatureKey;
  if (signatureKey.empty()) {
    signatureKey = symbol->name;
  }

  std::vector<CallableSignatureInfo> signatures =
      analyzer->getCallableSignatures(signatureKey);
  if (!signatures.empty()) {
    return signatures;
  }

  if (symbol->kind == SymbolKind::Class) {
    return analyzer->getCallableSignatures(symbol->name);
  }
  return {};
}

std::optional<ActiveCallContext> findActiveCallContext(const std::string &text,
                                                       std::size_t cursorOffset) {
  std::vector<CallFrame> stack;
  const std::size_t cursor = std::min(cursorOffset, text.size());

  bool inString = false;
  bool inLineComment = false;
  bool inBlockComment = false;
  bool escape = false;
  int bracketDepth = 0;
  int braceDepth = 0;

  for (std::size_t index = 0; index < cursor; ++index) {
    const char ch = text[index];
    const char next = index + 1 < cursor ? text[index + 1] : '\0';

    if (inLineComment) {
      if (ch == '\n') {
        inLineComment = false;
      }
      continue;
    }
    if (inBlockComment) {
      if (ch == '*' && next == '/') {
        inBlockComment = false;
        ++index;
      }
      continue;
    }
    if (inString) {
      if (escape) {
        escape = false;
      } else if (ch == '\\') {
        escape = true;
      } else if (ch == '"') {
        inString = false;
      }
      continue;
    }

    if (ch == '/' && next == '/') {
      inLineComment = true;
      ++index;
      continue;
    }
    if (ch == '/' && next == '*') {
      inBlockComment = true;
      ++index;
      continue;
    }
    if (ch == '"') {
      inString = true;
      continue;
    }

    switch (ch) {
    case '[':
      ++bracketDepth;
      break;
    case ']':
      if (bracketDepth > 0) {
        --bracketDepth;
      }
      break;
    case '{':
      ++braceDepth;
      break;
    case '}':
      if (braceDepth > 0) {
        --braceDepth;
      }
      break;
    case '(':
      stack.push_back({extractIdentifierChainBeforeOffset(text, index), 0,
                       bracketDepth, braceDepth});
      break;
    case ')':
      if (!stack.empty()) {
        stack.pop_back();
      }
      break;
    case ',':
      if (!stack.empty() &&
          bracketDepth == stack.back().bracketDepth &&
          braceDepth == stack.back().braceDepth) {
        ++stack.back().argumentIndex;
      }
      break;
    default:
      break;
    }
  }

  while (!stack.empty()) {
    if (!stack.back().callableChain.empty()) {
      return ActiveCallContext{stack.back().callableChain,
                               stack.back().argumentIndex};
    }
    stack.pop_back();
  }
  return std::nullopt;
}

std::vector<VisibleSymbolInfo> collectCompletionSymbols(const DocumentState &document,
                                                        int line, int character) {
  if (document.analyzer == nullptr) {
    return {};
  }

  const std::size_t cursorOffset = positionToOffset(document.text, line, character);
  const CompletionContext context =
      buildCompletionContext(document.text, cursorOffset);
  const SourceLocation location = {line + 1, character + 1, document.path.string()};
  const std::vector<VisibleSymbolInfo> visibleSymbols =
      document.analyzer->getScopeSnapshot(location);

  std::vector<VisibleSymbolInfo> candidates;
  if (context.isMemberAccess) {
    const std::optional<VisibleSymbolInfo> receiver =
        resolveVisibleSymbolChain(document.analyzer.get(), visibleSymbols,
                                  context.receiverChain);
    if (!receiver.has_value()) {
      return {};
    }
    candidates = document.analyzer->getTypeMembers(receiver->type);
  } else {
    candidates = visibleSymbols;
  }

  std::vector<VisibleSymbolInfo> filtered;
  filtered.reserve(candidates.size());
  std::unordered_set<std::string> seenNames;
  for (const auto &candidate : candidates) {
    if (!startsWith(candidate.name, context.prefix) ||
        !seenNames.insert(candidate.name).second) {
      continue;
    }
    filtered.push_back(candidate);
  }

  std::sort(filtered.begin(), filtered.end(),
            [](const VisibleSymbolInfo &lhs, const VisibleSymbolInfo &rhs) {
              return lhs.name < rhs.name;
            });
  return filtered;
}

} // namespace neuron::lsp::detail
