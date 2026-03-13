#include "neuronc/cli/PackageManager.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <system_error>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#endif

namespace neuron {
namespace {
namespace fs = std::filesystem;

constexpr const char *kManifestFileName = "neuron.toml";
constexpr const char *kLockFileName = "neuron.lock";
constexpr const char *kInstalledMetadataFileName = ".neuron-package.lock";

const std::array<const char *, 12> kBuiltinModules = {
    "system",   "math",   "io",      "time",    "random", "logger",
    "tensor",   "nn",     "dataset", "image",   "resource", "graphics",
};

struct CommandResult {
  int exitCode = 0;
  std::string output;
};

struct SemVer {
  int major = 0;
  int minor = 0;
  int patch = 0;
  std::string original;
};

struct SourceSpec {
  std::string raw;
  std::string url;
  std::string sourceId;
  std::string packageName;
  bool remote = false;
};

struct CachedPackage {
  LockedPackage locked;
  ProjectConfig config;
  fs::path cachePath;
};

struct ResolveContext {
  fs::path projectRoot;
  PackageLock existingLock;
  PackageLock outputLock;
  std::unordered_map<std::string, fs::path> cachePaths;
  std::unordered_set<std::string> activePackages;
};

std::string trimCopy(const std::string &text) {
  std::size_t begin = 0;
  std::size_t end = text.size();
  while (begin < end &&
         std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
    ++begin;
  }
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return text.substr(begin, end - begin);
}

std::string toLowerCopy(const std::string &text) {
  std::string lowered = text;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  return lowered;
}

bool isExactSemVerString(const std::string &text, SemVer *outVersion = nullptr) {
  std::string value = trimCopy(text);
  if (value.empty()) {
    return false;
  }
  if (value.front() == 'v' || value.front() == 'V') {
    value.erase(value.begin());
  }

  std::vector<int> parts;
  std::stringstream stream(value);
  std::string token;
  while (std::getline(stream, token, '.')) {
    if (token.empty()) {
      return false;
    }
    for (char ch : token) {
      if (!std::isdigit(static_cast<unsigned char>(ch))) {
        return false;
      }
    }
    parts.push_back(std::stoi(token));
  }
  if (parts.size() != 3u) {
    return false;
  }
  if (outVersion != nullptr) {
    outVersion->major = parts[0];
    outVersion->minor = parts[1];
    outVersion->patch = parts[2];
    outVersion->original = text;
  }
  return true;
}

bool parseSemVerTag(const std::string &tag, SemVer *outVersion) {
  return isExactSemVerString(tag, outVersion);
}

int compareSemVer(const SemVer &lhs, const SemVer &rhs) {
  if (lhs.major != rhs.major) {
    return lhs.major < rhs.major ? -1 : 1;
  }
  if (lhs.minor != rhs.minor) {
    return lhs.minor < rhs.minor ? -1 : 1;
  }
  if (lhs.patch != rhs.patch) {
    return lhs.patch < rhs.patch ? -1 : 1;
  }
  return 0;
}

bool matchesVersionConstraint(const std::string &constraint,
                             const std::string &versionText) {
  if (constraint.empty()) {
    return true;
  }

  SemVer version;
  if (!isExactSemVerString(versionText, &version)) {
    return false;
  }

  std::string trimmed = trimCopy(constraint);
  if (trimmed.empty()) {
    return true;
  }

  if (trimmed.front() == '^') {
    SemVer floor;
    if (!isExactSemVerString(trimmed.substr(1), &floor)) {
      return false;
    }
    if (version.major != floor.major) {
      return false;
    }
    return compareSemVer(version, floor) >= 0;
  }

  if (trimmed.front() == '~') {
    SemVer floor;
    if (!isExactSemVerString(trimmed.substr(1), &floor)) {
      return false;
    }
    if (version.major != floor.major || version.minor != floor.minor) {
      return false;
    }
    return compareSemVer(version, floor) >= 0;
  }

  SemVer exact;
  if (!isExactSemVerString(trimmed, &exact)) {
    return false;
  }
  return compareSemVer(version, exact) == 0;
}

std::string hexString(std::uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << value;
  return out.str();
}

std::string escapeTomlString(const std::string &text) {
  std::string escaped;
  escaped.reserve(text.size() + 8);
  for (char ch : text) {
    switch (ch) {
    case '\\':
      escaped += "\\\\";
      break;
    case '"':
      escaped += "\\\"";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      escaped.push_back(ch);
      break;
    }
  }
  return escaped;
}

std::string quoteToml(const std::string &text) {
  return "\"" + escapeTomlString(text) + "\"";
}

std::string quoteShell(const std::string &text) {
  std::string escaped;
  escaped.reserve(text.size() + 8);
  for (char ch : text) {
    if (ch == '"') {
      escaped += "\\\"";
    } else {
      escaped.push_back(ch);
    }
  }
  return "\"" + escaped + "\"";
}

std::string quoteShell(const fs::path &path) {
  return quoteShell(path.string());
}

std::string uniqueSuffix() {
  static std::uint64_t counter = 0;
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  ++counter;
  return std::to_string(now) + "_" + std::to_string(counter);
}

CommandResult runCommand(const std::string &command) {
  CommandResult result;
  const fs::path outputPath =
      fs::temp_directory_path() / ("neuron_pkg_" + uniqueSuffix() + ".txt");
  const std::string redirected = command + " > " + quoteShell(outputPath) +
                                 " 2>&1";
  result.exitCode = std::system(redirected.c_str());

  std::ifstream file(outputPath, std::ios::binary);
  if (file.is_open()) {
    std::ostringstream buffer;
    buffer << file.rdbuf();
    result.output = buffer.str();
  }

  std::error_code ec;
  fs::remove(outputPath, ec);
  return result;
}

fs::path globalPackageRoot() {
#ifdef _WIN32
  wchar_t buffer[MAX_PATH] = {0};
  DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
  if (length != 0 && length < MAX_PATH) {
    return fs::path(buffer) / "Neuron" / "packages";
  }
  const char *fallback = std::getenv("LOCALAPPDATA");
  if (fallback != nullptr && *fallback != '\0') {
    return fs::path(fallback) / "Neuron" / "packages";
  }
  return fs::temp_directory_path() / "Neuron" / "packages";
#else
  const char *xdgCache = std::getenv("XDG_CACHE_HOME");
  if (xdgCache != nullptr && *xdgCache != '\0') {
    return fs::path(xdgCache) / "Neuron" / "packages";
  }
  const char *home = std::getenv("HOME");
  if (home != nullptr && *home != '\0') {
    return fs::path(home) / ".cache" / "Neuron" / "packages";
  }
  return fs::temp_directory_path() / "Neuron" / "packages";
#endif
}

fs::path manifestPathForProject(const fs::path &projectRoot) {
  return projectRoot / kManifestFileName;
}

fs::path lockPathForProject(const fs::path &projectRoot) {
  return projectRoot / kLockFileName;
}

fs::path installedMetadataPath(const fs::path &packageRoot) {
  return packageRoot / kInstalledMetadataFileName;
}

bool writeTextFile(const fs::path &path, const std::string &content,
                   std::string *outError) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to create directory '" + path.parent_path().string() +
                  "': " + ec.message();
    }
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to write file '" + path.string() + "'";
    }
    return false;
  }
  out << content;
  if (!out.good()) {
    if (outError != nullptr) {
      *outError = "failed to flush file '" + path.string() + "'";
    }
    return false;
  }
  return true;
}

bool readTextFile(const fs::path &path, std::string *outContent,
                  std::string *outError) {
  if (outContent == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null text output";
    }
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to open file '" + path.string() + "'";
    }
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  *outContent = buffer.str();
  return true;
}

std::string normalizeSourceId(const std::string &source) {
  std::string id = trimCopy(source);
  if (id.size() > 4 && id.substr(id.size() - 4) == ".git") {
    id.resize(id.size() - 4);
  }
  if (!id.empty() && id.back() == '/') {
    id.pop_back();
  }
  const std::string lower = toLowerCopy(id);
  const std::string githubPrefix = "https://github.com/";
  const std::string githubPrefixHttp = "http://github.com/";
  const std::string githubSshPrefix = "git@github.com:";
  if (lower.rfind(githubPrefix, 0) == 0) {
    return id.substr(githubPrefix.size());
  }
  if (lower.rfind(githubPrefixHttp, 0) == 0) {
    return id.substr(githubPrefixHttp.size());
  }
  if (lower.rfind(githubSshPrefix, 0) == 0) {
    return id.substr(githubSshPrefix.size());
  }
  return id;
}

bool parseSourceSpec(const std::string &rawSpec, SourceSpec *outSpec) {
  if (outSpec == nullptr) {
    return false;
  }

  SourceSpec spec;
  spec.raw = trimCopy(rawSpec);
  if (spec.raw.empty()) {
    return false;
  }

  std::error_code ec;
  const fs::path localPath(spec.raw);
  if ((fs::exists(localPath, ec) && !ec) ||
      (spec.raw.size() > 7 && spec.raw.rfind("file://", 0) == 0)) {
    fs::path resolved = localPath;
    if (spec.raw.rfind("file://", 0) == 0) {
      resolved = fs::path(spec.raw.substr(7));
    }
    if (!resolved.is_absolute()) {
      resolved = fs::absolute(resolved, ec);
      if (ec) {
        resolved = fs::path(spec.raw);
      }
    }
    spec.url = resolved.string();
    spec.sourceId = spec.url;
    spec.packageName = resolved.filename().string();
    spec.remote = false;
    *outSpec = std::move(spec);
    return true;
  }

  const std::string lowered = toLowerCopy(spec.raw);
  if (lowered.rfind("https://github.com/", 0) == 0 ||
      lowered.rfind("http://github.com/", 0) == 0 ||
      lowered.rfind("git@github.com:", 0) == 0) {
    spec.url = spec.raw;
    spec.sourceId = normalizeSourceId(spec.raw);
    spec.remote = true;
  } else if (spec.raw.find('/') != std::string::npos &&
             spec.raw.find("://") == std::string::npos &&
             spec.raw.find(' ') == std::string::npos) {
    spec.sourceId = normalizeSourceId(spec.raw);
    spec.url = "https://github.com/" + spec.sourceId + ".git";
    spec.remote = true;
  } else if (spec.raw.find("://") != std::string::npos) {
    spec.url = spec.raw;
    spec.sourceId = normalizeSourceId(spec.raw);
    spec.remote = true;
  } else {
    spec.packageName = spec.raw;
    spec.sourceId = spec.raw;
    *outSpec = std::move(spec);
    return true;
  }

  std::string sourceId = normalizeSourceId(spec.sourceId.empty() ? spec.url : spec.sourceId);
  spec.sourceId = sourceId;
  const std::size_t slash = sourceId.find_last_of("/:");
  spec.packageName = slash == std::string::npos ? sourceId : sourceId.substr(slash + 1);
  if (spec.packageName.size() > 4 &&
      spec.packageName.substr(spec.packageName.size() - 4) == ".git") {
    spec.packageName.resize(spec.packageName.size() - 4);
  }

  *outSpec = std::move(spec);
  return true;
}

bool dependencyHasLocator(const DependencySpec &spec) {
  return !spec.github.empty() || !spec.commit.empty() || !spec.tag.empty() ||
         !spec.version.empty();
}

std::string serializeProjectConfig(const ProjectConfig &config) {
  std::ostringstream out;
  out << "[project]\n";
  out << "name = " << quoteToml(config.name) << "\n";
  out << "version = " << quoteToml(config.version) << "\n\n";

  if (config.package.enabled || config.package.kind == PackageKind::Library ||
      !config.package.description.empty() || !config.package.repository.empty() ||
      !config.package.license.empty() || config.package.sourceDir != "src") {
    out << "[package]\n";
    out << "kind = "
        << quoteToml(config.package.kind == PackageKind::Library ? "library"
                                                                  : "application")
        << "\n";
    if (!config.package.description.empty()) {
      out << "description = " << quoteToml(config.package.description) << "\n";
    }
    if (!config.package.repository.empty()) {
      out << "repository = " << quoteToml(config.package.repository) << "\n";
    }
    if (!config.package.license.empty()) {
      out << "license = " << quoteToml(config.package.license) << "\n";
    }
    out << "source_dir = " << quoteToml(config.package.sourceDir) << "\n\n";
  }

  out << "[build]\n";
  out << "main = " << quoteToml(config.mainFile) << "\n";
  out << "build_dir = " << quoteToml(config.buildDir) << "\n";
  const auto optimizeText = [level = config.optimizeLevel]() {
    switch (level) {
    case BuildOptimizeLevel::O0:
      return "O0";
    case BuildOptimizeLevel::O1:
      return "O1";
    case BuildOptimizeLevel::O2:
      return "O2";
    case BuildOptimizeLevel::O3:
      return "O3";
    case BuildOptimizeLevel::Oz:
      return "Oz";
    case BuildOptimizeLevel::Aggressive:
      return "aggressive";
    }
    return "aggressive";
  }();
  const auto emitIrText = [mode = config.emitIR]() {
    switch (mode) {
    case BuildEmitIR::Optimized:
      return "optimized";
    case BuildEmitIR::Both:
      return "both";
    case BuildEmitIR::None:
      return "none";
    }
    return "optimized";
  }();
  const auto targetCpuText =
      config.targetCPU == BuildTargetCPU::Native ? "native" : "generic";
  const auto tensorProfileText = [profile = config.tensorProfile]() {
    switch (profile) {
    case BuildTensorProfile::Balanced:
      return "balanced";
    case BuildTensorProfile::GemmParity:
      return "gemm_parity";
    case BuildTensorProfile::AIFused:
      return "ai_fused";
    }
    return "balanced";
  }();
  out << "optimize = " << quoteToml(optimizeText) << "\n";
  out << "emit_ir = " << quoteToml(emitIrText) << "\n";
  out << "target_cpu = " << quoteToml(targetCpuText) << "\n";
  out << "tensor_profile = " << quoteToml(tensorProfileText) << "\n";
  out << "tensor_autotune = " << (config.tensorAutotune ? "true" : "false")
      << "\n";
  out << "tensor_kernel_cache = " << quoteToml(config.tensorKernelCache)
      << "\n\n";

  out << "[dependencies]\n";
  std::vector<std::string> dependencyNames;
  dependencyNames.reserve(config.dependencies.size());
  for (const auto &entry : config.dependencies) {
    dependencyNames.push_back(entry.first);
  }
  std::sort(dependencyNames.begin(), dependencyNames.end());
  for (const auto &name : dependencyNames) {
    const DependencySpec &dep = config.dependencies.at(name);
    if (dep.github.empty() && dep.tag.empty() && dep.commit.empty() &&
        dep.legacyShorthand) {
      out << name << " = " << quoteToml(dep.version) << "\n";
      continue;
    }

    out << name << " = { ";
    bool firstField = true;
    auto appendField = [&](const std::string &fieldName,
                           const std::string &fieldValue) {
      if (fieldValue.empty()) {
        return;
      }
      if (!firstField) {
        out << ", ";
      }
      firstField = false;
      out << fieldName << " = " << quoteToml(fieldValue);
    };
    appendField("github", dep.github);
    appendField("version", dep.version);
    appendField("tag", dep.tag);
    appendField("commit", dep.commit);
    out << " }\n";
  }
  out << "\n";

  if (config.moduleCppEnabled || !config.moduleCppModules.empty()) {
    out << "[modulecpp]\n";
    out << "enabled = " << (config.moduleCppEnabled ? "true" : "false")
        << "\n\n";

    std::vector<std::string> moduleNames;
    moduleNames.reserve(config.moduleCppModules.size());
    for (const auto &entry : config.moduleCppModules) {
      moduleNames.push_back(entry.first);
    }
    std::sort(moduleNames.begin(), moduleNames.end());
    for (const auto &name : moduleNames) {
      const ModuleCppConfig &module = config.moduleCppModules.at(name);
      out << "[modulecpp." << name << "]\n";
      if (!module.manifestPath.empty()) {
        out << "manifest = " << quoteToml(module.manifestPath) << "\n";
      }
      if (!module.buildSystem.empty()) {
        out << "build_system = " << quoteToml(module.buildSystem) << "\n";
      }
      if (!module.sourceDir.empty()) {
        out << "source_dir = " << quoteToml(module.sourceDir) << "\n";
      }
      if (!module.cmakeTarget.empty()) {
        out << "cmake_target = " << quoteToml(module.cmakeTarget) << "\n";
      }
      if (!module.artifactWindowsX64.empty()) {
        out << "artifact_windows_x64 = "
            << quoteToml(module.artifactWindowsX64) << "\n";
      }
      if (!module.artifactLinuxX64.empty()) {
        out << "artifact_linux_x64 = "
            << quoteToml(module.artifactLinuxX64) << "\n";
      }
      if (!module.artifactMacosArm64.empty()) {
        out << "artifact_macos_arm64 = "
            << quoteToml(module.artifactMacosArm64) << "\n";
      }
      out << "\n";
    }
  }

  if (config.ncon.enabled || !config.ncon.outputPath.empty() ||
      !config.ncon.includeDebugMap || config.ncon.hotReload) {
    out << "[ncon]\n";
    out << "enabled = " << (config.ncon.enabled ? "true" : "false") << "\n";
    if (!config.ncon.outputPath.empty()) {
      out << "output = " << quoteToml(config.ncon.outputPath) << "\n";
    }
    out << "include_debug_map = "
        << (config.ncon.includeDebugMap ? "true" : "false") << "\n";
    out << "hot_reload = " << (config.ncon.hotReload ? "true" : "false")
        << "\n\n";
  }

  if (!config.ncon.permissions.fsRead.empty() ||
      !config.ncon.permissions.fsWrite.empty() ||
      config.ncon.permissions.network != NconNetworkPolicy::Deny ||
      config.ncon.permissions.processSpawnAllowed) {
    out << "[ncon.permissions]\n";
    const auto writeArray = [&](const std::string &key,
                                const std::vector<std::string> &values) {
      out << key << " = [";
      for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
          out << ", ";
        }
        out << quoteToml(values[i]);
      }
      out << "]\n";
    };
    if (!config.ncon.permissions.fsRead.empty()) {
      writeArray("fs_read", config.ncon.permissions.fsRead);
    }
    if (!config.ncon.permissions.fsWrite.empty()) {
      writeArray("fs_write", config.ncon.permissions.fsWrite);
    }
    out << "network = "
        << quoteToml(config.ncon.permissions.network == NconNetworkPolicy::Allow
                         ? "allow"
                         : "deny")
        << "\n";
    out << "process_spawn = "
        << (config.ncon.permissions.processSpawnAllowed ? "true" : "false")
        << "\n\n";
  }

  if (!config.ncon.resources.empty()) {
    out << "[ncon.resources]\n";
    for (const auto &resource : config.ncon.resources) {
      out << quoteToml(resource.logicalId) << " = "
          << quoteToml(resource.sourcePath) << "\n";
    }
    out << "\n";
  }

  out << "[web]\n";
  out << "canvas_id = " << quoteToml(config.web.canvasId) << "\n";
  out << "wgsl_cache = " << (config.web.wgslCache ? "true" : "false")
      << "\n";
  out << "dev_server_port = " << config.web.devServerPort << "\n";
  out << "enable_shared_array = "
      << (config.web.enableSharedArray ? "true" : "false") << "\n";
  out << "initial_memory_mb = " << config.web.initialMemoryMb << "\n";
  out << "maximum_memory_mb = " << config.web.maximumMemoryMb << "\n";
  out << "wasm_simd = " << (config.web.wasmSimd ? "true" : "false")
      << "\n";

  return out.str();
}

bool loadProjectConfigFile(const fs::path &projectRoot, ProjectConfig *outConfig,
                           std::vector<std::string> *outErrors) {
  if (outConfig == nullptr) {
    if (outErrors != nullptr) {
      outErrors->push_back("internal error: null project config output");
    }
    return false;
  }
  ProjectConfigParser parser;
  if (!parser.parseFile(manifestPathForProject(projectRoot).string(), outConfig)) {
    if (outErrors != nullptr) {
      outErrors->insert(outErrors->end(), parser.errors().begin(),
                        parser.errors().end());
    }
    return false;
  }
  return true;
}

bool loadPackageLockFileOrEmpty(const fs::path &projectRoot, PackageLock *outLock,
                                std::vector<std::string> *outErrors) {
  if (outLock == nullptr) {
    if (outErrors != nullptr) {
      outErrors->push_back("internal error: null package lock output");
    }
    return false;
  }
  *outLock = PackageLock{};
  const fs::path path = lockPathForProject(projectRoot);
  if (!fs::exists(path)) {
    return true;
  }
  PackageLockParser parser;
  if (!parser.parseFile(path.string(), outLock)) {
    if (outErrors != nullptr) {
      outErrors->insert(outErrors->end(), parser.errors().begin(),
                        parser.errors().end());
    }
    return false;
  }
  return true;
}

bool writeInstalledMetadata(const fs::path &packageRoot, const LockedPackage &locked,
                           std::string *outError) {
  PackageLock lock;
  lock.packages[locked.name] = locked;
  return writePackageLockFile(installedMetadataPath(packageRoot).string(), lock,
                              outError);
}

bool readInstalledMetadata(const fs::path &packageRoot, LockedPackage *outLocked,
                          std::string *outError) {
  if (outLocked == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null installed metadata output";
    }
    return false;
  }
  PackageLock lock;
  PackageLockParser parser;
  const fs::path path = installedMetadataPath(packageRoot);
  if (!parser.parseFile(path.string(), &lock)) {
    if (outError != nullptr) {
      std::ostringstream buffer;
      for (size_t i = 0; i < parser.errors().size(); ++i) {
        if (i != 0) {
          buffer << '\n';
        }
        buffer << parser.errors()[i];
      }
      *outError = buffer.str();
    }
    return false;
  }
  if (lock.packages.empty()) {
    if (outError != nullptr) {
      *outError = "installed package metadata is empty";
    }
    return false;
  }
  *outLocked = lock.packages.begin()->second;
  return true;
}

bool shouldSkipSnapshotPath(const fs::path &relativePath) {
  if (relativePath.empty()) {
    return false;
  }
  const std::string first = relativePath.begin()->string();
  return first == ".git" || first == ".github" || first == "build" ||
         first == "modules" || first == ".gitignore" ||
         first == kLockFileName;
}

bool copySnapshotTree(const fs::path &sourceRoot, const fs::path &targetRoot,
                      std::string *outError) {
  std::error_code ec;
  fs::remove_all(targetRoot, ec);
  ec.clear();
  fs::create_directories(targetRoot, ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to create directory '" + targetRoot.string() +
                  "': " + ec.message();
    }
    return false;
  }

  for (fs::recursive_directory_iterator it(sourceRoot, ec), end; it != end;
       it.increment(ec)) {
    if (ec) {
      if (outError != nullptr) {
        *outError = "failed to traverse '" + sourceRoot.string() +
                    "': " + ec.message();
      }
      return false;
    }
    const fs::path relative = fs::relative(it->path(), sourceRoot, ec);
    if (ec) {
      if (outError != nullptr) {
        *outError = "failed to compute relative path for '" +
                    it->path().string() + "'";
      }
      return false;
    }
    if (shouldSkipSnapshotPath(relative)) {
      if (it->is_directory()) {
        it.disable_recursion_pending();
      }
      continue;
    }

    const fs::path destination = targetRoot / relative;
    if (it->is_directory()) {
      fs::create_directories(destination, ec);
      if (ec) {
        if (outError != nullptr) {
          *outError = "failed to create directory '" + destination.string() +
                      "': " + ec.message();
        }
        return false;
      }
      continue;
    }
    if (!it->is_regular_file()) {
      continue;
    }

    fs::create_directories(destination.parent_path(), ec);
    if (ec) {
      if (outError != nullptr) {
        *outError = "failed to create directory '" +
                    destination.parent_path().string() + "': " + ec.message();
      }
      return false;
    }
    fs::copy_file(it->path(), destination, fs::copy_options::overwrite_existing,
                  ec);
    if (ec) {
      if (outError != nullptr) {
        *outError = "failed to copy '" + it->path().string() + "' to '" +
                    destination.string() + "': " + ec.message();
      }
      return false;
    }
  }

  return true;
}

std::string computeContentHash(const fs::path &packageRoot, std::string *outError) {
  std::vector<fs::path> files;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(packageRoot, ec), end; it != end;
       it.increment(ec)) {
    if (ec) {
      if (outError != nullptr) {
        *outError = "failed to traverse package tree '" + packageRoot.string() +
                    "': " + ec.message();
      }
      return std::string();
    }
    if (!it->is_regular_file()) {
      continue;
    }
    const fs::path relative = fs::relative(it->path(), packageRoot, ec);
    if (ec) {
      if (outError != nullptr) {
        *outError = "failed to compute relative path for '" +
                    it->path().string() + "'";
      }
      return std::string();
    }
    if (shouldSkipSnapshotPath(relative)) {
      continue;
    }
    files.push_back(relative);
  }

  std::sort(files.begin(), files.end());
  std::uint64_t hash = 1469598103934665603ull;
  auto updateHash = [&](const void *data, size_t size) {
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (size_t i = 0; i < size; ++i) {
      hash ^= static_cast<std::uint64_t>(bytes[i]);
      hash *= 1099511628211ull;
    }
  };

  for (const auto &relative : files) {
    const std::string relativeText = relative.generic_string();
    updateHash(relativeText.data(), relativeText.size());

    std::ifstream in(packageRoot / relative, std::ios::binary);
    if (!in.is_open()) {
      if (outError != nullptr) {
        *outError = "failed to hash file '" + (packageRoot / relative).string() +
                    "'";
      }
      return std::string();
    }
    std::array<char, 4096> buffer{};
    while (in.good()) {
      in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      const std::streamsize read = in.gcount();
      if (read > 0) {
        updateHash(buffer.data(), static_cast<size_t>(read));
      }
    }
  }

  return hexString(hash);
}

bool collectPackageSourceFiles(const fs::path &packageRoot,
                               const std::string &sourceDir,
                               std::vector<fs::path> *outFiles,
                               std::string *outError) {
  if (outFiles == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null package source output";
    }
    return false;
  }
  outFiles->clear();
  const fs::path sourceRoot = (packageRoot / sourceDir).lexically_normal();
  if (!fs::exists(sourceRoot) || !fs::is_directory(sourceRoot)) {
    if (outError != nullptr) {
      *outError = "package source directory not found: " + sourceRoot.string();
    }
    return false;
  }

  std::error_code ec;
  for (fs::recursive_directory_iterator it(sourceRoot, ec), end; it != end;
       it.increment(ec)) {
    if (ec) {
      if (outError != nullptr) {
        *outError = "failed to traverse source directory '" +
                    sourceRoot.string() + "': " + ec.message();
      }
      return false;
    }
    if (!it->is_regular_file() || it->path().extension() != ".nr") {
      continue;
    }
    outFiles->push_back(it->path());
  }
  std::sort(outFiles->begin(), outFiles->end());
  return true;
}

bool collectExportedModules(const fs::path &packageRoot,
                            const std::string &sourceDir,
                            std::vector<std::string> *outModules,
                            std::string *outError) {
  if (outModules == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null exported module output";
    }
    return false;
  }

  std::vector<fs::path> sourceFiles;
  if (!collectPackageSourceFiles(packageRoot, sourceDir, &sourceFiles, outError)) {
    return false;
  }
  if (sourceFiles.empty()) {
    if (outError != nullptr) {
      *outError = "package source directory contains no .nr files";
    }
    return false;
  }

  std::unordered_set<std::string> seen;
  std::unordered_set<std::string> builtins;
  for (const char *builtin : kBuiltinModules) {
    builtins.insert(toLowerCopy(builtin));
  }

  outModules->clear();
  for (const auto &file : sourceFiles) {
    const std::string moduleName = file.stem().string();
    const std::string normalized = toLowerCopy(moduleName);
    if (!seen.insert(normalized).second) {
      if (outError != nullptr) {
        *outError = "duplicate exported module name: " + moduleName;
      }
      return false;
    }
    if (builtins.count(normalized) != 0u) {
      if (outError != nullptr) {
        *outError = "exported module name collides with builtin module: " +
                    moduleName;
      }
      return false;
    }
    outModules->push_back(moduleName);
  }
  std::sort(outModules->begin(), outModules->end());
  return true;
}

bool validateModuleCppConfig(const fs::path &packageRoot,
                             const std::unordered_map<std::string, ModuleCppConfig> &modules,
                             std::string *outError) {
  for (const auto &entry : modules) {
    const ModuleCppConfig &module = entry.second;
    if (module.manifestPath.empty()) {
      if (outError != nullptr) {
        *outError = "modulecpp '" + entry.first + "' is missing manifest";
      }
      return false;
    }
    if (!fs::exists((packageRoot / module.manifestPath).lexically_normal())) {
      if (outError != nullptr) {
        *outError = "modulecpp manifest not found: " + module.manifestPath;
      }
      return false;
    }
    const bool hasSourceBuild = !module.sourceDir.empty() && !module.cmakeTarget.empty();
    const bool hasArtifact = !module.artifactWindowsX64.empty() ||
                             !module.artifactLinuxX64.empty() ||
                             !module.artifactMacosArm64.empty();
    if (!hasSourceBuild && !hasArtifact) {
      if (outError != nullptr) {
        *outError = "modulecpp '" + entry.first +
                    "' requires source_dir+cmake_target or packaged artifacts";
      }
      return false;
    }
  }
  return true;
}

bool validatePackageManifest(const fs::path &packageRoot, const ProjectConfig &config,
                             bool requireLibraryPackage,
                             std::vector<std::string> *outModules,
                             std::string *outError) {
  if (config.name.empty()) {
    if (outError != nullptr) {
      *outError = "package is missing [project].name";
    }
    return false;
  }
  if (!isExactSemVerString(config.version)) {
    if (outError != nullptr) {
      *outError = "package version must be semver (X.Y.Z)";
    }
    return false;
  }
  if (!config.package.enabled) {
    if (outError != nullptr) {
      *outError = "package is missing [package] section";
    }
    return false;
  }
  if (requireLibraryPackage && config.package.kind != PackageKind::Library) {
    if (outError != nullptr) {
      *outError = "package.kind must be library for published packages";
    }
    return false;
  }
  if (config.package.description.empty()) {
    if (outError != nullptr) {
      *outError = "package is missing [package].description";
    }
    return false;
  }
  if (config.package.repository.empty()) {
    if (outError != nullptr) {
      *outError = "package is missing [package].repository";
    }
    return false;
  }
  if (config.package.license.empty()) {
    if (outError != nullptr) {
      *outError = "package is missing [package].license";
    }
    return false;
  }
  if (!fs::exists(packageRoot / "README.md")) {
    if (outError != nullptr) {
      *outError = "package root is missing README.md";
    }
    return false;
  }
  if (!collectExportedModules(packageRoot, config.package.sourceDir, outModules,
                              outError)) {
    return false;
  }
  if ((config.moduleCppEnabled || !config.moduleCppModules.empty()) &&
      !validateModuleCppConfig(packageRoot, config.moduleCppModules, outError)) {
    return false;
  }
  return true;
}

bool loadCachedPackage(const fs::path &packageRoot, CachedPackage *outPackage,
                       std::string *outError) {
  if (outPackage == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null cached package output";
    }
    return false;
  }

  LockedPackage locked;
  if (!readInstalledMetadata(packageRoot, &locked, outError)) {
    return false;
  }

  ProjectConfig config;
  std::vector<std::string> errors;
  if (!loadProjectConfigFile(packageRoot, &config, &errors)) {
    if (outError != nullptr) {
      std::ostringstream buffer;
      for (size_t i = 0; i < errors.size(); ++i) {
        if (i != 0) {
          buffer << '\n';
        }
        buffer << errors[i];
      }
      *outError = buffer.str();
    }
    return false;
  }

  outPackage->locked = std::move(locked);
  outPackage->config = std::move(config);
  outPackage->cachePath = packageRoot;
  return true;
}

bool matchesPackageQuery(const std::string &query,
                         const LockedPackage &locked) {
  const std::string trimmed = trimCopy(query);
  if (trimmed.empty()) {
    return false;
  }
  if (toLowerCopy(trimmed) == toLowerCopy(locked.name)) {
    return true;
  }
  return normalizeSourceId(trimmed) == normalizeSourceId(locked.github);
}

bool isBetterQueryCandidate(const CachedPackage &candidate,
                            const CachedPackage *bestPackage) {
  if (bestPackage == nullptr) {
    return true;
  }

  SemVer candidateVersion;
  SemVer bestVersion;
  const bool candidateHasVersion =
      isExactSemVerString(candidate.locked.packageVersion, &candidateVersion);
  const bool bestHasVersion =
      isExactSemVerString(bestPackage->locked.packageVersion, &bestVersion);
  if (candidateHasVersion && bestHasVersion) {
    return compareSemVer(candidateVersion, bestVersion) > 0;
  }
  if (candidateHasVersion != bestHasVersion) {
    return candidateHasVersion;
  }
  return toLowerCopy(candidate.locked.name) < toLowerCopy(bestPackage->locked.name);
}

bool specMatchesLockedPackage(const DependencySpec &spec,
                              const LockedPackage &locked) {
  if (!spec.github.empty() &&
      normalizeSourceId(spec.github) != normalizeSourceId(locked.github)) {
    return false;
  }
  if (!spec.commit.empty() && spec.commit != locked.resolvedCommit) {
    return false;
  }
  if (!spec.tag.empty() && spec.tag != locked.resolvedTag) {
    return false;
  }
  if (!spec.version.empty() &&
      !matchesVersionConstraint(spec.version, locked.packageVersion)) {
    return false;
  }
  return true;
}

bool findBestCachedGlobalPackage(const std::string &name, const DependencySpec &spec,
                                 CachedPackage *outPackage) {
  const fs::path root = globalPackageRoot();
  if (!fs::exists(root) || !fs::is_directory(root)) {
    return false;
  }

  bool found = false;
  SemVer bestVersion{};
  CachedPackage bestPackage;
  std::error_code ec;
  for (const auto &packageDir : fs::directory_iterator(root, ec)) {
    if (ec || !packageDir.is_directory()) {
      continue;
    }
    for (const auto &revisionDir : fs::directory_iterator(packageDir.path(), ec)) {
      if (ec || !revisionDir.is_directory()) {
        continue;
      }
      if (!fs::exists(installedMetadataPath(revisionDir.path()))) {
        continue;
      }
      std::string error;
      CachedPackage candidate;
      if (!loadCachedPackage(revisionDir.path(), &candidate, &error)) {
        continue;
      }
      if (toLowerCopy(candidate.locked.name) != toLowerCopy(name)) {
        continue;
      }
      if (!specMatchesLockedPackage(spec, candidate.locked)) {
        continue;
      }
      SemVer candidateVersion;
      const bool hasVersion = isExactSemVerString(candidate.locked.packageVersion,
                                                  &candidateVersion);
      if (!found) {
        found = true;
        bestPackage = std::move(candidate);
        bestVersion = hasVersion ? candidateVersion : SemVer{};
        continue;
      }
      if (hasVersion && compareSemVer(candidateVersion, bestVersion) > 0) {
        bestPackage = std::move(candidate);
        bestVersion = candidateVersion;
      }
    }
  }

  if (!found) {
    return false;
  }
  if (outPackage != nullptr) {
    *outPackage = std::move(bestPackage);
  }
  return true;
}

bool selectBestTag(const std::vector<std::string> &tags,
                   const std::string &constraint, std::string *outTag) {
  if (outTag == nullptr) {
    return false;
  }
  bool found = false;
  SemVer bestVersion{};
  std::string bestTag;
  for (const auto &tag : tags) {
    SemVer version;
    if (!parseSemVerTag(tag, &version)) {
      continue;
    }
    if (!constraint.empty() && !matchesVersionConstraint(constraint, tag)) {
      continue;
    }
    if (!found || compareSemVer(version, bestVersion) > 0) {
      found = true;
      bestVersion = version;
      bestTag = tag;
    }
  }
  if (!found) {
    return false;
  }
  *outTag = bestTag;
  return true;
}

bool listGitTags(const fs::path &repoRoot, std::vector<std::string> *outTags,
                 std::string *outError) {
  if (outTags == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null git tag output";
    }
    return false;
  }
  const CommandResult result =
      runCommand("git -C " + quoteShell(repoRoot) + " tag");
  if (result.exitCode != 0) {
    if (outError != nullptr) {
      *outError = trimCopy(result.output);
    }
    return false;
  }
  outTags->clear();
  std::istringstream stream(result.output);
  std::string line;
  while (std::getline(stream, line)) {
    line = trimCopy(line);
    if (!line.empty()) {
      outTags->push_back(line);
    }
  }
  return true;
}

bool checkoutResolvedRevision(const fs::path &repoRoot, const DependencySpec &spec,
                              const LockedPackage *hint, std::string *outTag,
                              std::string *outCommit, std::string *outError) {
  if (outTag != nullptr) {
    outTag->clear();
  }

  if (!spec.commit.empty()) {
    const CommandResult checkout = runCommand("git -C " + quoteShell(repoRoot) +
                                              " checkout --quiet " + spec.commit);
    if (checkout.exitCode != 0) {
      if (outError != nullptr) {
        *outError = "failed to checkout commit '" + spec.commit + "': " +
                    trimCopy(checkout.output);
      }
      return false;
    }
  } else {
    std::string targetTag = spec.tag;
    if (targetTag.empty()) {
      if (!spec.version.empty()) {
        std::vector<std::string> tags;
        if (!listGitTags(repoRoot, &tags, outError)) {
          return false;
        }
        if (!selectBestTag(tags, spec.version, &targetTag)) {
          if (outError != nullptr) {
            *outError = "no git tag satisfies version constraint '" +
                        spec.version + "'";
          }
          return false;
        }
      } else if (hint != nullptr && !hint->resolvedTag.empty()) {
        targetTag = hint->resolvedTag;
      } else if (hint != nullptr && !hint->resolvedCommit.empty()) {
        const CommandResult checkout =
            runCommand("git -C " + quoteShell(repoRoot) +
                       " checkout --quiet " + hint->resolvedCommit);
        if (checkout.exitCode != 0) {
          if (outError != nullptr) {
            *outError = "failed to checkout locked commit '" +
                        hint->resolvedCommit + "': " + trimCopy(checkout.output);
          }
          return false;
        }
      } else {
        std::vector<std::string> tags;
        std::string tagError;
        if (listGitTags(repoRoot, &tags, &tagError)) {
          selectBestTag(tags, std::string(), &targetTag);
        }
      }
    }

    if (!targetTag.empty()) {
      const CommandResult checkout = runCommand("git -C " + quoteShell(repoRoot) +
                                                " checkout --quiet " +
                                                quoteShell(targetTag));
      if (checkout.exitCode != 0) {
        if (outError != nullptr) {
          *outError = "failed to checkout tag '" + targetTag + "': " +
                      trimCopy(checkout.output);
        }
        return false;
      }
      if (outTag != nullptr) {
        *outTag = targetTag;
      }
    }
  }

  const CommandResult revParse =
      runCommand("git -C " + quoteShell(repoRoot) + " rev-parse HEAD");
  if (revParse.exitCode != 0) {
    if (outError != nullptr) {
      *outError = "failed to resolve git commit: " + trimCopy(revParse.output);
    }
    return false;
  }
  if (outCommit != nullptr) {
    *outCommit = trimCopy(revParse.output);
  }
  return true;
}

bool resolveFromGitSource(const SourceSpec &sourceSpec, const DependencySpec &spec,
                          const LockedPackage *hint, CachedPackage *outPackage,
                          std::string *outError) {
  if (outPackage == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null package resolution output";
    }
    return false;
  }
  if (sourceSpec.url.empty()) {
    if (outError != nullptr) {
      *outError =
          "package source must be a github owner/repo, URL, or local git path";
    }
    return false;
  }

  if (hint != nullptr && !hint->resolvedCommit.empty()) {
    const fs::path hintedCache = globalPackageRoot() / hint->name / hint->resolvedCommit;
    if (fs::exists(hintedCache) && fs::exists(installedMetadataPath(hintedCache))) {
      CachedPackage cached;
      std::string cacheError;
      if (loadCachedPackage(hintedCache, &cached, &cacheError) &&
          specMatchesLockedPackage(spec, cached.locked)) {
        *outPackage = std::move(cached);
        return true;
      }
    }
  }

  const fs::path cloneRoot =
      fs::temp_directory_path() / ("neuron_clone_" + uniqueSuffix());

  const CommandResult clone = runCommand("git clone --quiet " +
                                         quoteShell(sourceSpec.url) + " " +
                                         quoteShell(cloneRoot));
  if (clone.exitCode != 0) {
    if (outError != nullptr) {
      *outError = "failed to clone package source '" + sourceSpec.url +
                  "': " + trimCopy(clone.output);
    }
    std::error_code ec;
    fs::remove_all(cloneRoot, ec);
    return false;
  }

  std::string resolvedTag;
  std::string resolvedCommit;
  if (!checkoutResolvedRevision(cloneRoot, spec, hint, &resolvedTag,
                                &resolvedCommit, outError)) {
    std::error_code ec;
    fs::remove_all(cloneRoot, ec);
    return false;
  }

  ProjectConfig config;
  std::vector<std::string> parseErrors;
  if (!loadProjectConfigFile(cloneRoot, &config, &parseErrors)) {
    if (outError != nullptr) {
      std::ostringstream buffer;
      for (size_t i = 0; i < parseErrors.size(); ++i) {
        if (i != 0) {
          buffer << '\n';
        }
        buffer << parseErrors[i];
      }
      *outError = buffer.str();
    }
    std::error_code ec;
    fs::remove_all(cloneRoot, ec);
    return false;
  }

  std::vector<std::string> exportedModules;
  if (!validatePackageManifest(cloneRoot, config, true, &exportedModules,
                               outError)) {
    std::error_code ec;
    fs::remove_all(cloneRoot, ec);
    return false;
  }

  LockedPackage locked;
  locked.name = config.name;
  locked.github = sourceSpec.sourceId.empty() ? sourceSpec.url : sourceSpec.sourceId;
  locked.versionConstraint = spec.version;
  locked.resolvedTag = resolvedTag;
  locked.resolvedCommit = resolvedCommit;
  locked.packageVersion = config.version;
  locked.sourceDir = config.package.sourceDir;
  locked.exportedModules = exportedModules;
  locked.moduleCppEnabled = config.moduleCppEnabled;
  locked.moduleCppModules = config.moduleCppModules;
  locked.contentHash = computeContentHash(cloneRoot, outError);
  if (locked.contentHash.empty()) {
    std::error_code ec;
    fs::remove_all(cloneRoot, ec);
    return false;
  }

  const fs::path cachePath = globalPackageRoot() / locked.name / locked.resolvedCommit;
  if (!fs::exists(cachePath)) {
    if (!copySnapshotTree(cloneRoot, cachePath, outError)) {
      std::error_code ec;
      fs::remove_all(cloneRoot, ec);
      return false;
    }
  }
  if (!writeInstalledMetadata(cachePath, locked, outError)) {
    std::error_code ec;
    fs::remove_all(cloneRoot, ec);
    return false;
  }

  outPackage->locked = std::move(locked);
  outPackage->config = std::move(config);
  outPackage->cachePath = cachePath;

  std::error_code ec;
  fs::remove_all(cloneRoot, ec);
  return true;
}

bool ensureLocalMaterialized(const fs::path &projectRoot,
                             const CachedPackage &package,
                             std::string *outError) {
  const fs::path destination = projectRoot / "modules" / package.locked.name;
  return copySnapshotTree(package.cachePath, destination, outError);
}

bool compatibleWithResolved(const LockedPackage &locked, const DependencySpec &spec,
                            std::string *outError) {
  if (spec.github.empty() && spec.commit.empty() && spec.tag.empty() &&
      spec.version.empty()) {
    return true;
  }
  if (!specMatchesLockedPackage(spec, locked)) {
    if (outError != nullptr) {
      *outError = "dependency conflict for package '" + locked.name + "'";
    }
    return false;
  }
  return true;
}

bool resolveDependencyRecursive(const std::string &requestedName,
                                const DependencySpec &requestedSpec,
                                ResolveContext *context,
                                std::string *outError) {
  if (context == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null dependency resolution context";
    }
    return false;
  }

  DependencySpec spec = requestedSpec;
  if (spec.name.empty()) {
    spec.name = requestedName;
  }

  auto existingIt = context->outputLock.packages.find(spec.name);
  if (existingIt != context->outputLock.packages.end()) {
    return compatibleWithResolved(existingIt->second, spec, outError);
  }
  if (context->activePackages.count(spec.name) != 0u) {
    return true;
  }

  const LockedPackage *hint = nullptr;
  auto hintIt = context->existingLock.packages.find(spec.name);
  if (hintIt != context->existingLock.packages.end()) {
    hint = &hintIt->second;
    if (spec.github.empty()) {
      spec.github = hint->github;
    }
  }

  CachedPackage package;
  bool resolvedFromCache = false;
  if (spec.github.empty()) {
    resolvedFromCache = findBestCachedGlobalPackage(spec.name, spec, &package);
    if (!resolvedFromCache && hint != nullptr && !hint->github.empty()) {
      spec.github = hint->github;
    }
  }

  if (!resolvedFromCache) {
    if (spec.github.empty()) {
      if (outError != nullptr) {
        *outError = "dependency '" + spec.name +
                    "' has no github source and no matching global package";
      }
      return false;
    }
    SourceSpec sourceSpec;
    if (!parseSourceSpec(spec.github, &sourceSpec)) {
      if (outError != nullptr) {
        *outError = "invalid package source: " + spec.github;
      }
      return false;
    }
    if (!resolveFromGitSource(sourceSpec, spec, hint, &package, outError)) {
      return false;
    }
  }

  if (!compatibleWithResolved(package.locked, spec, outError)) {
    return false;
  }

  context->activePackages.insert(package.locked.name);
  context->outputLock.packages[package.locked.name] = package.locked;
  context->cachePaths[package.locked.name] = package.cachePath;

  std::set<std::string> childNames;
  for (const auto &dependency : package.config.dependencies) {
    DependencySpec childSpec = dependency.second;
    if (childSpec.name.empty()) {
      childSpec.name = dependency.first;
    }
    if (!resolveDependencyRecursive(dependency.first, childSpec, context,
                                    outError)) {
      context->activePackages.erase(package.locked.name);
      return false;
    }
    auto childIt = context->outputLock.packages.find(childSpec.name);
    if (childIt != context->outputLock.packages.end()) {
      childNames.insert(childIt->second.name);
    } else {
      childNames.insert(dependency.first);
    }
  }

  LockedPackage &locked = context->outputLock.packages[package.locked.name];
  locked.transitiveDependencies.assign(childNames.begin(), childNames.end());
  locked.moduleCppEnabled = package.config.moduleCppEnabled;
  locked.moduleCppModules = package.config.moduleCppModules;
  context->activePackages.erase(package.locked.name);
  return true;
}

std::optional<LockedPackage> findUniqueGlobalModuleProvider(const std::string &moduleName) {
  const fs::path root = globalPackageRoot();
  if (!fs::exists(root) || !fs::is_directory(root)) {
    return std::nullopt;
  }

  std::optional<LockedPackage> match;
  std::error_code ec;
  for (const auto &packageDir : fs::directory_iterator(root, ec)) {
    if (ec || !packageDir.is_directory()) {
      continue;
    }
    for (const auto &revisionDir : fs::directory_iterator(packageDir.path(), ec)) {
      if (ec || !revisionDir.is_directory()) {
        continue;
      }
      if (!fs::exists(installedMetadataPath(revisionDir.path()))) {
        continue;
      }
      LockedPackage locked;
      std::string error;
      if (!readInstalledMetadata(revisionDir.path(), &locked, &error)) {
        continue;
      }
      bool exportsModule = false;
      for (const auto &exportedModule : locked.exportedModules) {
        if (toLowerCopy(exportedModule) == toLowerCopy(moduleName)) {
          exportsModule = true;
          break;
        }
      }
      if (!exportsModule) {
        continue;
      }
      if (match.has_value()) {
        return std::nullopt;
      }
      match = std::move(locked);
    }
  }
  return match;
}

} // namespace

bool PackageManager::loadProjectConfig(const std::string &projectRoot,
                                       ProjectConfig *config,
                                       std::vector<std::string> *outErrors) {
  return loadProjectConfigFile(fs::path(projectRoot), config, outErrors);
}

bool PackageManager::writeProjectConfig(const std::string &projectRoot,
                                        const ProjectConfig &config,
                                        std::string *outMessage) {
  std::string error;
  if (!writeTextFile(manifestPathForProject(fs::path(projectRoot)),
                     serializeProjectConfig(config), &error)) {
    if (outMessage != nullptr) {
      *outMessage = error;
    }
    return false;
  }
  if (outMessage != nullptr) {
    *outMessage = "Updated neuron.toml";
  }
  return true;
}

bool PackageManager::loadPackageLock(const std::string &projectRoot,
                                     PackageLock *lock,
                                     std::vector<std::string> *outErrors) {
  return loadPackageLockFileOrEmpty(fs::path(projectRoot), lock, outErrors);
}

bool PackageManager::writePackageLock(const std::string &projectRoot,
                                      const PackageLock &lock,
                                      std::string *outMessage) {
  const fs::path lockPath = lockPathForProject(fs::path(projectRoot));
  std::error_code ec;
  if (lock.packages.empty()) {
    fs::remove(lockPath, ec);
    if (outMessage != nullptr) {
      *outMessage = "Removed neuron.lock";
    }
    return true;
  }
  std::string error;
  if (!writePackageLockFile(lockPath.string(), lock, &error)) {
    if (outMessage != nullptr) {
      *outMessage = error;
    }
    return false;
  }
  if (outMessage != nullptr) {
    *outMessage = "Updated neuron.lock";
  }
  return true;
}

std::vector<InstalledPackageRecord>
PackageManager::listInstalledPackages(const std::string &projectRoot,
                                      bool includeGlobal) {
  std::vector<InstalledPackageRecord> records;
  const auto collectFromRoot = [&records](const fs::path &root, bool global) {
    if (!fs::exists(root) || !fs::is_directory(root)) {
      return;
    }
    std::error_code ec;
    if (global) {
      for (const auto &packageDir : fs::directory_iterator(root, ec)) {
        if (ec || !packageDir.is_directory()) {
          continue;
        }
        for (const auto &revisionDir : fs::directory_iterator(packageDir.path(), ec)) {
          if (ec || !revisionDir.is_directory()) {
            continue;
          }
          LockedPackage locked;
          std::string error;
          if (!readInstalledMetadata(revisionDir.path(), &locked, &error)) {
            continue;
          }
          records.push_back({locked.name, locked.github, locked.resolvedTag,
                             locked.packageVersion, true,
                             locked.exportedModules});
        }
      }
      return;
    }

    for (const auto &entry : fs::directory_iterator(root, ec)) {
      if (ec || !entry.is_directory()) {
        continue;
      }
      LockedPackage locked;
      std::string error;
      if (!readInstalledMetadata(entry.path(), &locked, &error)) {
        continue;
      }
      records.push_back({locked.name, locked.github, locked.resolvedTag,
                         locked.packageVersion, false,
                         locked.exportedModules});
    }
  };

  collectFromRoot(fs::path(projectRoot) / "modules", false);
  if (includeGlobal) {
    collectFromRoot(globalPackageRoot(), true);
  }

  std::sort(records.begin(), records.end(),
            [](const InstalledPackageRecord &lhs,
               const InstalledPackageRecord &rhs) {
              if (lhs.global != rhs.global) {
                return lhs.global < rhs.global;
              }
              return toLowerCopy(lhs.name) < toLowerCopy(rhs.name);
            });
  return records;
}

bool PackageManager::inspectInstalledPackage(const std::string &projectRoot,
                                             const std::string &packageSource,
                                             bool includeGlobal,
                                             InstalledPackageDetails *outDetails,
                                             std::string *outMessage) {
  if (outDetails == nullptr) {
    if (outMessage != nullptr) {
      *outMessage = "internal error: null package details output";
    }
    return false;
  }

  CachedPackage bestLocal;
  bool foundLocal = false;
  std::error_code ec;
  const fs::path localRoot = fs::path(projectRoot) / "modules";
  if (fs::exists(localRoot, ec) && fs::is_directory(localRoot, ec)) {
    for (const auto &entry : fs::directory_iterator(localRoot, ec)) {
      if (ec || !entry.is_directory()) {
        continue;
      }
      CachedPackage candidate;
      std::string error;
      if (!loadCachedPackage(entry.path(), &candidate, &error)) {
        continue;
      }
      if (!matchesPackageQuery(packageSource, candidate.locked)) {
        continue;
      }
      if (!foundLocal || isBetterQueryCandidate(candidate, &bestLocal)) {
        bestLocal = std::move(candidate);
        foundLocal = true;
      }
    }
  }
  ec.clear();
  if (foundLocal) {
    outDetails->packageRoot = bestLocal.cachePath;
    outDetails->locked = std::move(bestLocal.locked);
    outDetails->config = std::move(bestLocal.config);
    outDetails->global = false;
    return true;
  }

  if (includeGlobal) {
    CachedPackage bestGlobal;
    bool foundGlobal = false;
    const fs::path globalRoot = globalPackageRoot();
    if (fs::exists(globalRoot, ec) && fs::is_directory(globalRoot, ec)) {
      for (const auto &packageDir : fs::directory_iterator(globalRoot, ec)) {
        if (ec || !packageDir.is_directory()) {
          continue;
        }
        for (const auto &revisionDir : fs::directory_iterator(packageDir.path(), ec)) {
          if (ec || !revisionDir.is_directory()) {
            continue;
          }
          CachedPackage candidate;
          std::string error;
          if (!loadCachedPackage(revisionDir.path(), &candidate, &error)) {
            continue;
          }
          if (!matchesPackageQuery(packageSource, candidate.locked)) {
            continue;
          }
          if (!foundGlobal || isBetterQueryCandidate(candidate, &bestGlobal)) {
            bestGlobal = std::move(candidate);
            foundGlobal = true;
          }
        }
      }
    }
    if (foundGlobal) {
      outDetails->packageRoot = bestGlobal.cachePath;
      outDetails->locked = std::move(bestGlobal.locked);
      outDetails->config = std::move(bestGlobal.config);
      outDetails->global = true;
      return true;
    }
  }

  if (outMessage != nullptr) {
    *outMessage = "package '" + packageSource +
                  "' is not installed locally or globally";
  }
  return false;
}

bool PackageManager::addDependency(const std::string &projectRoot,
                                   const std::string &packageSpec,
                                   const PackageInstallOptions &options,
                                   std::string *outMessage) {
  SourceSpec sourceSpec;
  if (!parseSourceSpec(packageSpec, &sourceSpec)) {
    if (outMessage != nullptr) {
      *outMessage = "invalid package spec";
    }
    return false;
  }

  DependencySpec dependency;
  dependency.name = sourceSpec.packageName;
  dependency.github = sourceSpec.sourceId.empty() ? sourceSpec.url : sourceSpec.sourceId;
  dependency.version = options.version;
  dependency.tag = options.tag;
  dependency.commit = options.commit;

  if (options.global) {
    CachedPackage resolved;
    if (dependency.github.empty()) {
      if (outMessage != nullptr) {
        *outMessage = "global installs require a github/url package source";
      }
      return false;
    }
    if (!resolveFromGitSource(sourceSpec, dependency, nullptr, &resolved,
                              outMessage)) {
      return false;
    }
    if (outMessage != nullptr) {
      *outMessage = "Installed global package '" + resolved.locked.name + "'";
    }
    return true;
  }

  ResolveContext context;
  context.projectRoot = fs::path(projectRoot);
  loadPackageLockFileOrEmpty(context.projectRoot, &context.existingLock, nullptr);

  CachedPackage resolved;
  const LockedPackage *hint = nullptr;
  if (!resolveFromGitSource(sourceSpec, dependency, hint, &resolved, outMessage)) {
    return false;
  }

  ProjectConfig config;
  std::vector<std::string> errors;
  if (!loadProjectConfigFile(context.projectRoot, &config, &errors)) {
    if (outMessage != nullptr) {
      std::ostringstream buffer;
      for (size_t i = 0; i < errors.size(); ++i) {
        if (i != 0) {
          buffer << '\n';
        }
        buffer << errors[i];
      }
      *outMessage = buffer.str();
    }
    return false;
  }

  dependency.name = resolved.locked.name;
  dependency.github = resolved.locked.github;
  config.dependencies[resolved.locked.name] = dependency;

  std::string writeMessage;
  if (!writeProjectConfig(projectRoot, config, &writeMessage)) {
    if (outMessage != nullptr) {
      *outMessage = writeMessage;
    }
    return false;
  }

  if (!installDependencies(projectRoot, outMessage)) {
    return false;
  }
  return true;
}

bool PackageManager::installDependencies(const std::string &projectRoot,
                                         std::string *outMessage) {
  const fs::path root(projectRoot);
  ProjectConfig config;
  std::vector<std::string> errors;
  if (!loadProjectConfigFile(root, &config, &errors)) {
    if (outMessage != nullptr) {
      std::ostringstream buffer;
      for (size_t i = 0; i < errors.size(); ++i) {
        if (i != 0) {
          buffer << '\n';
        }
        buffer << errors[i];
      }
      *outMessage = buffer.str();
    }
    return false;
  }

  ResolveContext context;
  context.projectRoot = root;
  if (!loadPackageLockFileOrEmpty(root, &context.existingLock, &errors)) {
    if (outMessage != nullptr) {
      std::ostringstream buffer;
      for (size_t i = 0; i < errors.size(); ++i) {
        if (i != 0) {
          buffer << '\n';
        }
        buffer << errors[i];
      }
      *outMessage = buffer.str();
    }
    return false;
  }

  for (const auto &dependency : config.dependencies) {
    DependencySpec spec = dependency.second;
    if (spec.name.empty()) {
      spec.name = dependency.first;
    }
    if (!resolveDependencyRecursive(dependency.first, spec, &context,
                                    outMessage)) {
      return false;
    }
  }

  std::error_code ec;
  fs::create_directories(root / "modules", ec);
  if (ec) {
    if (outMessage != nullptr) {
      *outMessage = "failed to create modules directory: " + ec.message();
    }
    return false;
  }

  for (const auto &entry : fs::directory_iterator(root / "modules", ec)) {
    if (ec || !entry.is_directory()) {
      continue;
    }
    const std::string packageName = entry.path().filename().string();
    if (context.outputLock.packages.find(packageName) !=
        context.outputLock.packages.end()) {
      continue;
    }
    if (!fs::exists(installedMetadataPath(entry.path()))) {
      continue;
    }
    fs::remove_all(entry.path(), ec);
    if (ec) {
      if (outMessage != nullptr) {
        *outMessage = "failed to remove stale package directory '" +
                      entry.path().string() + "': " + ec.message();
      }
      return false;
    }
  }

  for (const auto &entry : context.outputLock.packages) {
    CachedPackage package;
    auto cacheIt = context.cachePaths.find(entry.first);
    if (cacheIt == context.cachePaths.end()) {
      continue;
    }
    std::string error;
    if (!loadCachedPackage(cacheIt->second, &package, &error)) {
      if (outMessage != nullptr) {
        *outMessage = error;
      }
      return false;
    }
    if (!ensureLocalMaterialized(root, package, outMessage)) {
      return false;
    }
  }

  if (!writePackageLock(projectRoot, context.outputLock, outMessage)) {
    return false;
  }
  if (outMessage != nullptr) {
    *outMessage = "Installed " + std::to_string(context.outputLock.packages.size()) +
                  " package(s)";
  }
  return true;
}

bool PackageManager::removeDependency(const std::string &projectRoot,
                                      const std::string &packageName,
                                      bool removeGlobal,
                                      std::string *outMessage) {
  const std::string normalized = toLowerCopy(packageName);
  if (removeGlobal) {
    const fs::path root = globalPackageRoot();
    bool removed = false;
    std::error_code ec;
    for (const auto &packageDir : fs::directory_iterator(root, ec)) {
      if (ec || !packageDir.is_directory()) {
        continue;
      }
      if (toLowerCopy(packageDir.path().filename().string()) != normalized) {
        continue;
      }
      fs::remove_all(packageDir.path(), ec);
      removed = !ec;
    }
    if (outMessage != nullptr) {
      *outMessage = removed ? "Removed global package '" + packageName + "'"
                            : "global package not found";
    }
    return removed;
  }

  ProjectConfig config;
  std::vector<std::string> errors;
  if (!loadProjectConfigFile(fs::path(projectRoot), &config, &errors)) {
    if (outMessage != nullptr) {
      std::ostringstream buffer;
      for (const auto &error : errors) {
        if (!buffer.str().empty()) {
          buffer << '\n';
        }
        buffer << error;
      }
      *outMessage = buffer.str();
    }
    return false;
  }

  bool erased = false;
  for (auto it = config.dependencies.begin(); it != config.dependencies.end();) {
    if (toLowerCopy(it->first) == normalized) {
      it = config.dependencies.erase(it);
      erased = true;
    } else {
      ++it;
    }
  }
  if (!erased) {
    if (outMessage != nullptr) {
      *outMessage = "dependency not found: " + packageName;
    }
    return false;
  }

  std::string writeMessage;
  if (!writeProjectConfig(projectRoot, config, &writeMessage)) {
    if (outMessage != nullptr) {
      *outMessage = writeMessage;
    }
    return false;
  }

  std::error_code ec;
  fs::remove_all(fs::path(projectRoot) / "modules" / packageName, ec);
  return installDependencies(projectRoot, outMessage);
}

bool PackageManager::updateDependency(const std::string &projectRoot,
                                      const std::optional<std::string> &packageName,
                                      std::string *outMessage) {
  if (packageName.has_value()) {
    ProjectConfig config;
    std::vector<std::string> errors;
    if (!loadProjectConfigFile(fs::path(projectRoot), &config, &errors)) {
      if (outMessage != nullptr) {
        std::ostringstream buffer;
        for (const auto &error : errors) {
          if (!buffer.str().empty()) {
            buffer << '\n';
          }
          buffer << error;
        }
        *outMessage = buffer.str();
      }
      return false;
    }
    if (config.dependencies.find(*packageName) == config.dependencies.end()) {
      if (outMessage != nullptr) {
        *outMessage = "dependency not found: " + *packageName;
      }
      return false;
    }
  }
  return installDependencies(projectRoot, outMessage);
}

bool PackageManager::autoAddMissingModule(const std::string &projectRoot,
                                          const std::string &moduleName,
                                          std::string *outMessage) {
  const auto match = findUniqueGlobalModuleProvider(moduleName);
  if (!match.has_value()) {
    if (outMessage != nullptr) {
      *outMessage = "no unique global package exports module '" + moduleName + "'";
    }
    return false;
  }

  ProjectConfig config;
  std::vector<std::string> errors;
  if (!loadProjectConfigFile(fs::path(projectRoot), &config, &errors)) {
    if (outMessage != nullptr) {
      std::ostringstream buffer;
      for (const auto &error : errors) {
        if (!buffer.str().empty()) {
          buffer << '\n';
        }
        buffer << error;
      }
      *outMessage = buffer.str();
    }
    return false;
  }

  DependencySpec spec;
  spec.name = match->name;
  spec.github = match->github;
  spec.commit = match->resolvedCommit;
  config.dependencies[match->name] = spec;

  std::string writeMessage;
  if (!writeProjectConfig(projectRoot, config, &writeMessage)) {
    if (outMessage != nullptr) {
      *outMessage = writeMessage;
    }
    return false;
  }
  return installDependencies(projectRoot, outMessage);
}

bool PackageManager::publishProject(const std::string &projectRoot,
                                    std::string *outArtifactPath,
                                    std::string *outMessage) {
  const fs::path root(projectRoot);
  ProjectConfig config;
  std::vector<std::string> errors;
  if (!loadProjectConfigFile(root, &config, &errors)) {
    if (outMessage != nullptr) {
      std::ostringstream buffer;
      for (const auto &error : errors) {
        if (!buffer.str().empty()) {
          buffer << '\n';
        }
        buffer << error;
      }
      *outMessage = buffer.str();
    }
    return false;
  }

  std::vector<std::string> exportedModules;
  if (!validatePackageManifest(root, config, true, &exportedModules, outMessage)) {
    return false;
  }

  const fs::path packagesRoot = root / "build" / "packages";
  const fs::path snapshotDir = packagesRoot / (config.name + "-" + config.version);
  if (!copySnapshotTree(root, snapshotDir, outMessage)) {
    return false;
  }

  LockedPackage locked;
  locked.name = config.name;
  locked.github = config.package.repository;
  locked.packageVersion = config.version;
  locked.sourceDir = config.package.sourceDir;
  locked.exportedModules = exportedModules;
  locked.moduleCppEnabled = config.moduleCppEnabled;
  locked.moduleCppModules = config.moduleCppModules;
  locked.contentHash = computeContentHash(snapshotDir, outMessage);
  if (locked.contentHash.empty()) {
    return false;
  }
  if (!writeInstalledMetadata(snapshotDir, locked, outMessage)) {
    return false;
  }

  std::ostringstream manifest;
  manifest << "name = " << config.name << "\n";
  manifest << "version = " << config.version << "\n";
  manifest << "repository = " << config.package.repository << "\n";
  manifest << "snapshot = " << snapshotDir.string() << "\n";
  manifest << "content_hash = " << locked.contentHash << "\n";
  manifest << "source_dir = " << config.package.sourceDir << "\n";
  manifest << "exported_modules = ";
  for (size_t i = 0; i < exportedModules.size(); ++i) {
    if (i != 0) {
      manifest << ',';
    }
    manifest << exportedModules[i];
  }
  manifest << "\n";

  const fs::path artifactPath = packagesRoot / (config.name + "-" + config.version + ".nrkg");
  if (!writeTextFile(artifactPath, manifest.str(), outMessage)) {
    return false;
  }

  if (outArtifactPath != nullptr) {
    *outArtifactPath = artifactPath.string();
  }
  if (outMessage != nullptr) {
    *outMessage = "Created package artifact at '" + artifactPath.string() + "'";
  }
  return true;
}

} // namespace neuron
