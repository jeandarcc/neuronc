#include "SettingsMacroInternal.h"

#include "neuronc/cli/ProjectConfig.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace neuron::cli {

namespace {

std::vector<std::string> collectModuleNamesFromDir(const fs::path &dir) {
  std::vector<std::string> names;
  std::error_code ec;
  if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
    return names;
  }

  for (fs::recursive_directory_iterator it(dir, ec), end; it != end;
       it.increment(ec)) {
    if (ec || !it->is_regular_file() || it->path().extension() != ".npp") {
      continue;
    }
    names.push_back(lowerAscii(it->path().stem().string()));
  }

  return names;
}

std::vector<fs::path> builtinSettingsCandidates(const fs::path &toolRoot) {
  std::vector<fs::path> candidates;
  if (!toolRoot.empty()) {
    candidates.push_back(
        normalizePath(toolRoot / "config" / "builtin.modulesettings"));
  }
  candidates.push_back(
      normalizePath(fs::current_path() / "config" / "builtin.modulesettings"));
  return candidates;
}

} // namespace

std::string lowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

std::string trimCopy(const std::string &value) {
  std::size_t begin = 0;
  std::size_t end = value.size();
  while (begin < end &&
         std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
    ++begin;
  }
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }
  return value.substr(begin, end - begin);
}

fs::path normalizePath(const fs::path &path) {
  if (path.empty()) {
    return path;
  }
  std::error_code ec;
  const fs::path absolute = path.is_absolute() ? path : fs::absolute(path, ec);
  if (!ec) {
    return absolute.lexically_normal();
  }
  return path.lexically_normal();
}

std::string pathKey(const fs::path &path) {
  std::string key = normalizePath(path).generic_string();
#ifdef _WIN32
  key = lowerAscii(key);
#endif
  return key;
}

bool pathStartsWith(const fs::path &path, const fs::path &prefix) {
  const fs::path lhs = normalizePath(path);
  const fs::path rhs = normalizePath(prefix);
  auto lhsIt = lhs.begin();
  auto rhsIt = rhs.begin();
  for (; rhsIt != rhs.end(); ++rhsIt, ++lhsIt) {
    if (lhsIt == lhs.end()) {
      return false;
    }
    std::string lhsPart = lhsIt->string();
    std::string rhsPart = rhsIt->string();
#ifdef _WIN32
    lhsPart = lowerAscii(lhsPart);
    rhsPart = lowerAscii(rhsPart);
#endif
    if (lhsPart != rhsPart) {
      return false;
    }
  }
  return true;
}

fs::path detectEntryProjectRoot(const fs::path &entrySourcePath) {
  fs::path probe = normalizePath(entrySourcePath).parent_path();
  if (probe.empty()) {
    return fs::current_path();
  }
  for (;;) {
    if (fs::exists(probe / "neuron.toml")) {
      return probe;
    }
    const fs::path parent = probe.parent_path();
    if (parent == probe || parent.empty()) {
      break;
    }
    probe = parent;
  }
  return normalizePath(entrySourcePath).parent_path();
}

std::vector<fs::path> discoverProjectRoots(const fs::path &entryRoot) {
  std::vector<fs::path> roots;
  std::unordered_set<std::string> seen;
  const fs::path normalizedEntryRoot = normalizePath(entryRoot);
  roots.push_back(normalizedEntryRoot);
  seen.insert(pathKey(normalizedEntryRoot));

  const fs::path modulesRoot = normalizedEntryRoot / "modules";
  std::error_code ec;
  if (!fs::exists(modulesRoot, ec) || !fs::is_directory(modulesRoot, ec)) {
    return roots;
  }

  for (fs::recursive_directory_iterator it(modulesRoot, ec), end; it != end;
       it.increment(ec)) {
    if (ec || !it->is_regular_file() || it->path().filename() != "neuron.toml") {
      continue;
    }
    const fs::path root = normalizePath(it->path().parent_path());
    if (seen.insert(pathKey(root)).second) {
      roots.push_back(root);
    }
  }

  std::sort(roots.begin(), roots.end(), [](const fs::path &lhs,
                                           const fs::path &rhs) {
    return lhs.generic_string().size() < rhs.generic_string().size();
  });
  return roots;
}

std::unordered_set<std::string>
collectOwnedSectionsForRoot(const fs::path &root) {
  std::unordered_set<std::string> sections;
  const fs::path configPath = root / "neuron.toml";
  if (fs::exists(configPath)) {
    ProjectConfig config;
    ProjectConfigParser parser;
    const bool parsed = parser.parseFile(configPath.string(), &config);
    const std::string sourceDir =
        parsed && !config.package.sourceDir.empty() ? config.package.sourceDir
                                                    : std::string("src");
    for (const auto &name : collectModuleNamesFromDir(root / sourceDir)) {
      sections.insert(name);
    }
    if (parsed && !config.mainFile.empty()) {
      for (const auto &name :
           collectModuleNamesFromDir(root / fs::path(config.mainFile).parent_path())) {
        sections.insert(name);
      }
    }
    return sections;
  }

  for (const auto &name : collectModuleNamesFromDir(root / "src")) {
    sections.insert(name);
  }
  std::error_code ec;
  if (fs::exists(root, ec) && fs::is_directory(root, ec)) {
    for (const auto &entry : fs::directory_iterator(root, ec)) {
      if (ec || !entry.is_regular_file() || entry.path().extension() != ".npp") {
        continue;
      }
      sections.insert(lowerAscii(entry.path().stem().string()));
    }
  }
  return sections;
}

std::vector<std::string> sourceRootChain(const fs::path &sourcePath,
                                         const std::vector<fs::path> &roots) {
  std::vector<std::pair<std::size_t, std::string>> ordered;
  const fs::path normalizedSource = normalizePath(sourcePath);
  for (const auto &root : roots) {
    if (pathStartsWith(normalizedSource, root)) {
      ordered.emplace_back(root.generic_string().size(), pathKey(root));
    }
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const auto &lhs, const auto &rhs) { return lhs.first < rhs.first; });

  std::vector<std::string> chain;
  chain.reserve(ordered.size());
  for (const auto &entry : ordered) {
    chain.push_back(entry.second);
  }
  return chain;
}

std::string qualifiedMacroKey(const std::string &normalizedSection,
                              const std::string &name) {
  return normalizedSection + "." + name;
}

bool isDeclarationLikeContext(const std::vector<Token> &tokens,
                              std::size_t index) {
  const TokenType prev =
      index > 0 ? tokens[index - 1].type : TokenType::Eof;
  const TokenType next =
      (index + 1) < tokens.size() ? tokens[index + 1].type : TokenType::Eof;
  return prev == TokenType::Module || prev == TokenType::ModuleCpp ||
         prev == TokenType::As || next == TokenType::Is ||
         next == TokenType::Method || next == TokenType::Class ||
         next == TokenType::Struct || next == TokenType::Interface ||
         next == TokenType::Enum || next == TokenType::Colon ||
         next == TokenType::As;
}

bool readTextFile(const fs::path &path, std::string *outText) {
  if (outText == nullptr) {
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }
  std::ostringstream out;
  out << in.rdbuf();
  if (!in.good() && !in.eof()) {
    return false;
  }
  *outText = out.str();
  return true;
}

std::string makeConfigError(const fs::path &path, int line, int column,
                            const std::string &message) {
  std::ostringstream out;
  out << path.string() << ":" << std::max(1, line) << ":"
      << std::max(1, column) << ": " << message;
  return out.str();
}

std::optional<fs::path> resolveBuiltinSettingsPath(const fs::path &toolRoot) {
  for (const auto &candidate : builtinSettingsCandidates(toolRoot)) {
    std::error_code ec;
    if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
      return candidate;
    }
  }
  return std::nullopt;
}

} // namespace neuron::cli
