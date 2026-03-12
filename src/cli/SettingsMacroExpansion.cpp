#include "SettingsMacroInternal.h"

#include "neuronc/lexer/Lexer.h"

#include <algorithm>
#include <functional>
#include <sstream>

namespace neuron::cli {

namespace {

std::vector<Token> tokenizeMacroSnippet(const EffectiveMacroEntry &macro,
                                        std::vector<std::string> *outErrors) {
  Lexer lexer(macro.rawSnippet, macro.originFile.string());
  auto tokens = lexer.tokenize();
  if (!lexer.errors().empty() && outErrors != nullptr) {
    for (const auto &error : lexer.errors()) {
      outErrors->push_back(makeConfigError(
          macro.originFile, macro.originLine, macro.originColumn,
          "Invalid macro body for '" + macro.section + "." + macro.name +
              "': " + error));
    }
  }
  if (!tokens.empty() && tokens.back().type == TokenType::Eof) {
    tokens.pop_back();
  }
  return tokens;
}

std::string renderTokensForTrace(const std::vector<Token> &tokens) {
  std::string rendered;
  auto needsSpaceBefore = [](TokenType type) {
    switch (type) {
    case TokenType::RightParen:
    case TokenType::RightBracket:
    case TokenType::RightBrace:
    case TokenType::Comma:
    case TokenType::Semicolon:
    case TokenType::Colon:
    case TokenType::Dot:
    case TokenType::DotDot:
      return false;
    default:
      return true;
    }
  };
  auto needsSpaceAfter = [](TokenType type) {
    switch (type) {
    case TokenType::LeftParen:
    case TokenType::LeftBracket:
    case TokenType::LeftBrace:
    case TokenType::Dot:
    case TokenType::DotDot:
      return false;
    default:
      return true;
    }
  };

  bool havePrevious = false;
  TokenType previousType = TokenType::Eof;
  for (const Token &token : tokens) {
    if (token.type == TokenType::Eof || token.value.empty()) {
      continue;
    }
    if (havePrevious && !rendered.empty() && needsSpaceAfter(previousType) &&
        needsSpaceBefore(token.type)) {
      rendered.push_back(' ');
    }
    rendered += token.value;
    previousType = token.type;
    havePrevious = true;
  }
  return rendered;
}

bool expandTokenStream(const EffectiveMacroSet &effective,
                       const std::vector<Token> &tokens,
                       std::vector<std::string> *outErrors,
                       std::vector<std::string> *expansionStack,
                       std::vector<MacroExpansionTrace> *outTraces,
                       std::vector<Token> *outTokens);

bool expandMacroInvocation(const EffectiveMacroSet &effective,
                           const EffectiveMacroEntry &macro,
                           const SourceLocation &useLoc,
                           int useLength,
                           std::vector<std::string> *outErrors,
                           std::vector<std::string> *expansionStack,
                           std::vector<MacroExpansionTrace> *outTraces,
                           std::vector<Token> *outTokens) {
  const std::string qualified =
      qualifiedMacroKey(macro.normalizedSection, macro.name);
  if (std::find(expansionStack->begin(), expansionStack->end(), qualified) !=
      expansionStack->end()) {
    if (outErrors != nullptr) {
      outErrors->push_back(makeConfigError(
          useLoc.file, useLoc.line, useLoc.column,
          "Recursive macro expansion detected for '" + macro.section + "." +
              macro.name + "'."));
    }
    return false;
  }

  expansionStack->push_back(qualified);
  std::vector<Token> snippetTokens = tokenizeMacroSnippet(macro, outErrors);
  if (outErrors != nullptr && !outErrors->empty()) {
    expansionStack->pop_back();
    return false;
  }

  std::vector<Token> expandedSnippet;
  if (!expandTokenStream(effective, snippetTokens, outErrors, expansionStack,
                         outTraces, &expandedSnippet)) {
    expansionStack->pop_back();
    return false;
  }

  if (outTraces != nullptr) {
    MacroExpansionTrace trace;
    trace.name = macro.name;
    trace.qualifiedName = macro.section + "." + macro.name;
    trace.useLocation = useLoc;
    trace.useLength = std::max(1, useLength);
    trace.expansion = renderTokensForTrace(expandedSnippet);
    outTraces->push_back(std::move(trace));
  }

  outTokens->push_back(Token(TokenType::LeftParen, "(", useLoc));
  for (auto token : expandedSnippet) {
    token.location = useLoc;
    outTokens->push_back(std::move(token));
  }
  outTokens->push_back(Token(TokenType::RightParen, ")", useLoc));
  expansionStack->pop_back();
  return true;
}

bool expandTokenStream(const EffectiveMacroSet &effective,
                       const std::vector<Token> &tokens,
                       std::vector<std::string> *outErrors,
                       std::vector<std::string> *expansionStack,
                       std::vector<MacroExpansionTrace> *outTraces,
                       std::vector<Token> *outTokens) {
  outTokens->clear();
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    const Token &token = tokens[i];
    if (token.type == TokenType::Eof) {
      continue;
    }

    if (token.type == TokenType::Identifier) {
      if ((i + 2) < tokens.size() && tokens[i + 1].type == TokenType::Dot &&
          tokens[i + 2].type == TokenType::Identifier) {
        const std::string qualified =
            qualifiedMacroKey(lowerAscii(token.value), tokens[i + 2].value);
        const auto qualifiedIt = effective.qualified.find(qualified);
        if (qualifiedIt != effective.qualified.end()) {
          const int useLength = static_cast<int>(
              token.value.size() + tokens[i + 1].value.size() +
              tokens[i + 2].value.size());
          if (!expandMacroInvocation(effective, qualifiedIt->second,
                                     token.location, useLength, outErrors,
                                     expansionStack, outTraces, outTokens)) {
            return false;
          }
          i += 2;
          continue;
        }
      }

      const bool bareAllowed =
          !isDeclarationLikeContext(tokens, i) &&
          (i == 0 || tokens[i - 1].type != TokenType::Dot) &&
          ((i + 1) >= tokens.size() || tokens[i + 1].type != TokenType::Dot);
      if (bareAllowed) {
        if (const auto ambiguousIt = effective.ambiguous.find(token.value);
            ambiguousIt != effective.ambiguous.end()) {
          if (outErrors != nullptr) {
            std::ostringstream message;
            message << "Ambiguous macro '" << token.value
                    << "'. Use an explicit section prefix such as ";
            for (std::size_t sectionIndex = 0;
                 sectionIndex < ambiguousIt->second.size(); ++sectionIndex) {
              if (sectionIndex != 0) {
                message << ", ";
              }
              message << ambiguousIt->second[sectionIndex] << "." << token.value;
            }
            outErrors->push_back(makeConfigError(token.location.file,
                                                 token.location.line,
                                                 token.location.column,
                                                 message.str()));
          }
          return false;
        }

        const auto bareIt = effective.bare.find(token.value);
        if (bareIt != effective.bare.end()) {
          if (!expandMacroInvocation(
                  effective, bareIt->second, token.location,
                  static_cast<int>(token.value.size()), outErrors, expansionStack,
                  outTraces, outTokens)) {
            return false;
          }
          continue;
        }
      }
    }

    outTokens->push_back(token);
  }
  return true;
}

} // namespace

bool SettingsMacroProcessor::expandSourceTokens(
    const std::filesystem::path &sourcePath,
    const std::vector<neuron::Token> &inputTokens,
    std::vector<neuron::Token> *outTokens,
    std::vector<std::string> *outErrors) const {
  return expandSourceTokensWithTrace(sourcePath, inputTokens, outTokens, nullptr,
                                     outErrors);
}

bool SettingsMacroProcessor::expandSourceTokensWithTrace(
    const std::filesystem::path &sourcePath,
    const std::vector<neuron::Token> &inputTokens,
    std::vector<neuron::Token> *outTokens,
    std::vector<MacroExpansionTrace> *outTraces,
    std::vector<std::string> *outErrors) const {
  if (outTokens == nullptr || m_impl == nullptr) {
    return false;
  }
  outTokens->clear();
  if (outTraces != nullptr) {
    outTraces->clear();
  }
  if (outErrors != nullptr) {
    outErrors->clear();
  }

  const std::vector<std::string> chain =
      sourceRootChain(sourcePath, m_impl->ctx.roots);
  if (chain.empty()) {
    *outTokens = inputTokens;
    return true;
  }

  const std::string ownerKey = chain.back();
  const auto effectiveIt = m_impl->ctx.effectiveByOwnerRoot.find(ownerKey);
  if (effectiveIt == m_impl->ctx.effectiveByOwnerRoot.end()) {
    *outTokens = inputTokens;
    return true;
  }

  std::vector<std::string> expansionStack;
  if (!expandTokenStream(effectiveIt->second, inputTokens, outErrors,
                         &expansionStack, outTraces, outTokens)) {
    return false;
  }

  if (!inputTokens.empty() && inputTokens.back().type == TokenType::Eof) {
    outTokens->push_back(inputTokens.back());
  }
  return outErrors == nullptr || outErrors->empty();
}

} // namespace neuron::cli
