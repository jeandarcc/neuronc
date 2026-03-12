#pragma once

#include "neuronc/lexer/Token.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace neuron::cli {

enum class SettingsMacroKind {
  Expression,
  Method,
};

struct MacroExpansionTrace {
  std::string name;
  std::string qualifiedName;
  SourceLocation useLocation;
  int useLength = 1;
  std::string expansion;
};

class SettingsMacroProcessor {
public:
  SettingsMacroProcessor(std::filesystem::path toolRoot,
                         std::filesystem::path entrySourcePath);
  ~SettingsMacroProcessor();

  bool initialize(std::vector<std::string> *outErrors);

  bool expandSourceTokens(const std::filesystem::path &sourcePath,
                          const std::vector<neuron::Token> &inputTokens,
                          std::vector<neuron::Token> *outTokens,
                          std::vector<std::string> *outErrors) const;

  bool expandSourceTokensWithTrace(
      const std::filesystem::path &sourcePath,
      const std::vector<neuron::Token> &inputTokens,
      std::vector<neuron::Token> *outTokens,
      std::vector<MacroExpansionTrace> *outTraces,
      std::vector<std::string> *outErrors) const;

private:
  std::filesystem::path m_toolRoot;
  std::filesystem::path m_entrySourcePath;

  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace neuron::cli
