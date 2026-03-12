#pragma once

#include "LspTypes.h"

#include <optional>
#include <string>
#include <string_view>

namespace neuron::lsp::detail {

std::string trimCopy(std::string text);
std::string toLowerCopy(std::string_view text);
bool startsWith(std::string_view value, std::string_view prefix);
bool containsCaseInsensitive(std::string_view text, std::string_view needle);
std::optional<std::string> readFileText(const fs::path &path);
std::string normalizeModuleName(std::string value);
fs::path normalizePath(const fs::path &path);
std::string pathKey(const fs::path &path);
bool samePath(const fs::path &lhs, const fs::path &rhs);
bool sameLocation(const SourceLocation &lhs, const SourceLocation &rhs);
bool sameSymbolLocation(const SymbolLocation &lhs, const SymbolLocation &rhs);
bool isValidIdentifier(std::string_view name);
fs::path uriToPath(const std::string &uri);
std::string pathToUri(const fs::path &path);
std::string fileStem(const fs::path &path);

} // namespace neuron::lsp::detail
