#include "neuronc/cli/ProjectConfig.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace neuron {

namespace {

std::string toLowerCopy(const std::string &text) {
  std::string lowered = text;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lowered;
}

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

bool parseOptimizeLevel(const std::string &value, BuildOptimizeLevel *outLevel) {
  if (outLevel == nullptr) {
    return false;
  }
  const std::string lowered = toLowerCopy(value);
  if (lowered == "o0") {
    *outLevel = BuildOptimizeLevel::O0;
    return true;
  }
  if (lowered == "o1") {
    *outLevel = BuildOptimizeLevel::O1;
    return true;
  }
  if (lowered == "o2") {
    *outLevel = BuildOptimizeLevel::O2;
    return true;
  }
  if (lowered == "o3") {
    *outLevel = BuildOptimizeLevel::O3;
    return true;
  }
  if (lowered == "oz") {
    *outLevel = BuildOptimizeLevel::Oz;
    return true;
  }
  if (lowered == "aggressive") {
    *outLevel = BuildOptimizeLevel::Aggressive;
    return true;
  }
  return false;
}

bool parseEmitIRMode(const std::string &value, BuildEmitIR *outMode) {
  if (outMode == nullptr) {
    return false;
  }
  const std::string lowered = toLowerCopy(value);
  if (lowered == "optimized") {
    *outMode = BuildEmitIR::Optimized;
    return true;
  }
  if (lowered == "both") {
    *outMode = BuildEmitIR::Both;
    return true;
  }
  if (lowered == "none") {
    *outMode = BuildEmitIR::None;
    return true;
  }
  return false;
}

bool parseTargetCPU(const std::string &value, BuildTargetCPU *outTarget) {
  if (outTarget == nullptr) {
    return false;
  }
  const std::string lowered = toLowerCopy(value);
  if (lowered == "native") {
    *outTarget = BuildTargetCPU::Native;
    return true;
  }
  if (lowered == "generic") {
    *outTarget = BuildTargetCPU::Generic;
    return true;
  }
  return false;
}

bool parseTensorProfile(const std::string &value,
                        BuildTensorProfile *outProfile) {
  if (outProfile == nullptr) {
    return false;
  }
  const std::string lowered = toLowerCopy(value);
  if (lowered == "balanced") {
    *outProfile = BuildTensorProfile::Balanced;
    return true;
  }
  if (lowered == "gemm_parity" || lowered == "gemmparity") {
    *outProfile = BuildTensorProfile::GemmParity;
    return true;
  }
  if (lowered == "ai_fused" || lowered == "aifused") {
    *outProfile = BuildTensorProfile::AIFused;
    return true;
  }
  return false;
}

bool parsePackageKind(const std::string &value, PackageKind *outKind) {
  if (outKind == nullptr) {
    return false;
  }
  const std::string lowered = toLowerCopy(value);
  if (lowered == "app" || lowered == "application") {
    *outKind = PackageKind::Application;
    return true;
  }
  if (lowered == "lib" || lowered == "library") {
    *outKind = PackageKind::Library;
    return true;
  }
  return false;
}

bool parseBool(const std::string &value, bool *outValue) {
  if (outValue == nullptr) {
    return false;
  }
  const std::string lowered = toLowerCopy(value);
  if (lowered == "true" || lowered == "1" || lowered == "yes" ||
      lowered == "on") {
    *outValue = true;
    return true;
  }
  if (lowered == "false" || lowered == "0" || lowered == "no" ||
      lowered == "off") {
    *outValue = false;
    return true;
  }
  return false;
}

bool parseInt32(const std::string &value, int32_t minValue, int32_t maxValue,
                int32_t *outValue) {
  if (outValue == nullptr) {
    return false;
  }
  errno = 0;
  char *end = nullptr;
  const long parsed = std::strtol(value.c_str(), &end, 10);
  if (errno != 0 || end == value.c_str() || *end != '\0') {
    return false;
  }
  if (parsed < static_cast<long>(minValue) ||
      parsed > static_cast<long>(maxValue)) {
    return false;
  }
  *outValue = static_cast<int32_t>(parsed);
  return true;
}

bool parseNconNetworkPolicy(const std::string &value,
                            NconNetworkPolicy *outPolicy) {
  if (outPolicy == nullptr) {
    return false;
  }
  const std::string lowered = toLowerCopy(value);
  if (lowered == "deny") {
    *outPolicy = NconNetworkPolicy::Deny;
    return true;
  }
  if (lowered == "allow") {
    *outPolicy = NconNetworkPolicy::Allow;
    return true;
  }
  return false;
}

bool parseStringArray(const std::string &value,
                      std::vector<std::string> *outValues) {
  if (outValues == nullptr) {
    return false;
  }
  outValues->clear();

  const std::string trimmed = trimCopy(value);
  if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
    return false;
  }

  size_t cursor = 1;
  while (cursor + 1 < trimmed.size()) {
    while (cursor + 1 < trimmed.size() &&
           (std::isspace(static_cast<unsigned char>(trimmed[cursor])) != 0 ||
            trimmed[cursor] == ',')) {
      ++cursor;
    }
    if (cursor + 1 >= trimmed.size()) {
      break;
    }
    if (trimmed[cursor] != '"') {
      return false;
    }

    ++cursor;
    std::string entry;
    bool escaped = false;
    while (cursor + 1 < trimmed.size()) {
      const char c = trimmed[cursor++];
      if (escaped) {
        entry.push_back(c);
        escaped = false;
        continue;
      }
      if (c == '\\') {
        escaped = true;
        continue;
      }
      if (c == '"') {
        break;
      }
      entry.push_back(c);
    }
    if (escaped || cursor > trimmed.size()) {
      outValues->clear();
      return false;
    }
    outValues->push_back(std::move(entry));
  }
  return true;
}

bool decodeQuotedKey(std::string *key) {
  if (key == nullptr) {
    return false;
  }
  if (key->size() < 2 || key->front() != '"' || key->back() != '"') {
    return true;
  }
  *key = key->substr(1, key->size() - 2);
  return true;
}

} // namespace

bool ProjectConfigParser::parseFile(const std::string &path,
                                    ProjectConfig *outConfig) {
  std::ifstream file(path);
  if (!file.is_open()) {
    m_errors.clear();
    m_sourceName = path;
    addError(0, "could not open file");
    return false;
  }

  std::ostringstream content;
  content << file.rdbuf();
  return parseString(content.str(), path, outConfig);
}

bool ProjectConfigParser::parseString(const std::string &content,
                                      const std::string &sourceName,
                                      ProjectConfig *outConfig) {
  m_sourceName = sourceName;
  m_errors.clear();
  m_activeNconModuleCpp.clear();

  if (outConfig == nullptr) {
    addError(0, "output config pointer is null");
    return false;
  }

  *outConfig = ProjectConfig{};
  Section section = Section::None;

  std::istringstream stream(content);
  std::string line;
  std::size_t lineNumber = 0;

  while (std::getline(stream, line)) {
    ++lineNumber;
    parseLine(line, lineNumber, &section, outConfig);
  }

  if (outConfig->moduleCppEnabled) {
    outConfig->ncon.native.enabled = true;
    outConfig->ncon.native.modules = outConfig->moduleCppModules;
  } else if (outConfig->ncon.native.enabled &&
             !outConfig->ncon.native.modules.empty()) {
    outConfig->moduleCppEnabled = true;
    outConfig->moduleCppModules = outConfig->ncon.native.modules;
  }

  if (outConfig->name.empty()) {
    addError(0, "missing required key: [project].name");
  }

  return m_errors.empty();
}

std::string ProjectConfigParser::trim(std::string_view text) {
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

  return std::string(text.substr(begin, end - begin));
}

std::string ProjectConfigParser::stripComment(const std::string &line) {
  bool inString = false;
  bool escaped = false;

  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];

    if (escaped) {
      escaped = false;
      continue;
    }

    if (c == '\\' && inString) {
      escaped = true;
      continue;
    }

    if (c == '"') {
      inString = !inString;
      continue;
    }

    if (c == '#' && !inString) {
      return line.substr(0, i);
    }
  }

  return line;
}

bool ProjectConfigParser::parseLine(const std::string &line,
                                    std::size_t lineNumber, Section *section,
                                    ProjectConfig *outConfig) {
  const std::string clean = trim(stripComment(line));
  if (clean.empty()) {
    return true;
  }

  if (clean.front() == '[') {
    if (clean.back() != ']') {
      addError(lineNumber, "invalid section header");
      return false;
    }

    const std::string rawSectionName = trim(
        std::string_view(clean.data() + 1, clean.size() - 2));
    const std::string sectionName = toLowerCopy(rawSectionName);
    m_activeNconModuleCpp.clear();
    if (sectionName == "project") {
      *section = Section::Project;
    } else if (sectionName == "build") {
      *section = Section::Build;
    } else if (sectionName == "package") {
      outConfig->package.enabled = true;
      *section = Section::Package;
    } else if (sectionName == "dependencies") {
      *section = Section::Dependencies;
    } else if (sectionName == "modulecpp") {
      outConfig->moduleCppEnabled = true;
      *section = Section::ModuleCpp;
    } else if (sectionName == "ncon") {
      *section = Section::Ncon;
    } else if (sectionName == "ncon.native") {
      *section = Section::NconNative;
    } else if (sectionName == "ncon.permissions") {
      *section = Section::NconPermissions;
    } else if (sectionName == "ncon.resources") {
      *section = Section::NconResources;
    } else if (sectionName == "web") {
      *section = Section::Web;
    } else if (sectionName.rfind("modulecpp.", 0) == 0) {
      const std::string moduleName =
          rawSectionName.substr(std::string("modulecpp.").size());
      if (moduleName.empty()) {
        addError(lineNumber, "empty modulecpp section name");
        return false;
      }
      m_activeNconModuleCpp = moduleName;
      outConfig->moduleCppEnabled = true;
      *section = Section::ModuleCpp;
    } else if (sectionName.rfind("ncon.modulecpp.", 0) == 0) {
      const std::string moduleName =
          rawSectionName.substr(std::string("ncon.modulecpp.").size());
      if (moduleName.empty()) {
        addError(lineNumber, "empty modulecpp section name");
        return false;
      }
      m_activeNconModuleCpp = moduleName;
      outConfig->moduleCppEnabled = true;
      *section = Section::NconModuleCpp;
    } else if (sectionName.rfind("ncon.", 0) == 0) {
      addError(lineNumber, "unknown ncon section: [" + sectionName + "]");
      return false;
    } else {
      *section = Section::Unknown;
    }

    return true;
  }

  std::string key;
  std::string value;
  if (!parseKeyValue(clean, lineNumber, &key, &value)) {
    return false;
  }
  decodeQuotedKey(&key);

  switch (*section) {
  case Section::Project:
    if (key == "name") {
      outConfig->name = value;
    } else if (key == "version") {
      outConfig->version = value;
    }
    break;
  case Section::Package:
    outConfig->package.enabled = true;
    if (key == "kind") {
      if (!parsePackageKind(value, &outConfig->package.kind)) {
        addError(lineNumber,
                 "invalid package.kind value (expected application/library)");
        return false;
      }
    } else if (key == "description") {
      outConfig->package.description = value;
    } else if (key == "repository") {
      outConfig->package.repository = value;
    } else if (key == "license") {
      outConfig->package.license = value;
    } else if (key == "source_dir") {
      outConfig->package.sourceDir = value;
    } else {
      addError(lineNumber, "unknown package key: " + key);
      return false;
    }
    break;
  case Section::Build:
    if (key == "main" || key == "entry") {
      outConfig->mainFile = value;
    } else if (key == "build_dir" || key == "out_dir") {
      outConfig->buildDir = value;
    } else if (key == "optimize" || key == "opt_level") {
      if (!parseOptimizeLevel(value, &outConfig->optimizeLevel)) {
        addError(lineNumber,
                 "invalid build.optimize value (expected O0/O1/O2/O3/Oz/aggressive)");
        return false;
      }
    } else if (key == "emit_ir" || key == "ir_mode") {
      if (!parseEmitIRMode(value, &outConfig->emitIR)) {
        addError(lineNumber,
                 "invalid build.emit_ir value (expected optimized/both/none)");
        return false;
      }
    } else if (key == "target_cpu" || key == "cpu") {
      if (!parseTargetCPU(value, &outConfig->targetCPU)) {
        addError(lineNumber,
                 "invalid build.target_cpu value (expected native/generic)");
        return false;
      }
    } else if (key == "tensor_profile") {
      if (!parseTensorProfile(value, &outConfig->tensorProfile)) {
        addError(lineNumber,
                 "invalid build.tensor_profile value (expected "
                 "balanced/gemm_parity/ai_fused)");
        return false;
      }
    } else if (key == "tensor_autotune") {
      if (!parseBool(value, &outConfig->tensorAutotune)) {
        addError(lineNumber,
                 "invalid build.tensor_autotune value (expected true/false)");
        return false;
      }
    } else if (key == "tensor_kernel_cache") {
      outConfig->tensorKernelCache = value;
    } else if (key == "ncon") {
      if (!parseBool(value, &outConfig->ncon.enabled)) {
        addError(lineNumber, "invalid build.ncon value (expected true/false)");
        return false;
      }
    }
    break;
  case Section::Dependencies:
    if (key.empty()) {
      addError(lineNumber, "empty dependency name");
      return false;
    }
    if (!value.empty() && value.front() == '{') {
      std::unordered_map<std::string, std::string> fields;
      if (!parseInlineTable(value, lineNumber, &fields)) {
        return false;
      }
      DependencySpec spec;
      spec.name = key;
      auto githubIt = fields.find("github");
      if (githubIt != fields.end()) {
        spec.github = githubIt->second;
      }
      auto versionIt = fields.find("version");
      if (versionIt != fields.end()) {
        spec.version = versionIt->second;
      }
      auto tagIt = fields.find("tag");
      if (tagIt != fields.end()) {
        spec.tag = tagIt->second;
      }
      auto commitIt = fields.find("commit");
      if (commitIt != fields.end()) {
        spec.commit = commitIt->second;
      }
      outConfig->dependencies[key] = std::move(spec);
    } else {
      DependencySpec spec;
      spec.name = key;
      spec.version = value;
      spec.legacyShorthand = true;
      outConfig->dependencies[key] = std::move(spec);
    }
    break;
  case Section::ModuleCpp:
    if (!m_activeNconModuleCpp.empty()) {
      auto &moduleConfig = outConfig->moduleCppModules[m_activeNconModuleCpp];
      if (key == "manifest") {
        moduleConfig.manifestPath = value;
      } else if (key == "build_system") {
        moduleConfig.buildSystem = value;
      } else if (key == "source_dir") {
        moduleConfig.sourceDir = value;
      } else if (key == "cmake_target") {
        moduleConfig.cmakeTarget = value;
      } else if (key == "artifact_windows_x64") {
        moduleConfig.artifactWindowsX64 = value;
      } else if (key == "artifact_linux_x64") {
        moduleConfig.artifactLinuxX64 = value;
      } else if (key == "artifact_macos_arm64") {
        moduleConfig.artifactMacosArm64 = value;
      } else {
        addError(lineNumber, "unknown modulecpp key: " + key);
        return false;
      }
      outConfig->moduleCppEnabled = true;
      outConfig->ncon.native.enabled = true;
      outConfig->ncon.native.modules = outConfig->moduleCppModules;
      break;
    }
    if (key == "enabled") {
      if (!parseBool(value, &outConfig->moduleCppEnabled)) {
        addError(lineNumber,
                 "invalid modulecpp.enabled value (expected true/false)");
        return false;
      }
      outConfig->ncon.native.enabled = outConfig->moduleCppEnabled;
    } else {
      addError(lineNumber, "unknown modulecpp key: " + key);
      return false;
    }
    break;
  case Section::Ncon:
    if (key == "enabled") {
      if (!parseBool(value, &outConfig->ncon.enabled)) {
        addError(lineNumber, "invalid ncon.enabled value (expected true/false)");
        return false;
      }
    } else if (key == "output") {
      outConfig->ncon.outputPath = value;
    } else if (key == "include_debug_map") {
      if (!parseBool(value, &outConfig->ncon.includeDebugMap)) {
        addError(lineNumber,
                 "invalid ncon.include_debug_map value (expected true/false)");
        return false;
      }
    } else if (key == "hot_reload") {
      if (!parseBool(value, &outConfig->ncon.hotReload)) {
        addError(lineNumber,
                 "invalid ncon.hot_reload value (expected true/false)");
        return false;
      }
    } else {
      addError(lineNumber, "unknown ncon key: " + key);
      return false;
    }
    break;
  case Section::NconNative:
    if (key == "enabled") {
      if (!parseBool(value, &outConfig->ncon.native.enabled)) {
        addError(lineNumber,
                 "invalid ncon.native.enabled value (expected true/false)");
        return false;
      }
      outConfig->moduleCppEnabled = outConfig->ncon.native.enabled;
    } else {
      addError(lineNumber, "unknown ncon.native key: " + key);
      return false;
    }
    break;
  case Section::NconModuleCpp: {
    if (m_activeNconModuleCpp.empty()) {
      addError(lineNumber, "modulecpp section missing active module name");
      return false;
    }
    auto &moduleConfig = outConfig->moduleCppModules[m_activeNconModuleCpp];
    if (key == "manifest") {
      moduleConfig.manifestPath = value;
    } else if (key == "build_system") {
      moduleConfig.buildSystem = value;
    } else if (key == "source_dir") {
      moduleConfig.sourceDir = value;
    } else if (key == "cmake_target") {
      moduleConfig.cmakeTarget = value;
    } else if (key == "artifact_windows_x64") {
      moduleConfig.artifactWindowsX64 = value;
    } else if (key == "artifact_linux_x64") {
      moduleConfig.artifactLinuxX64 = value;
    } else if (key == "artifact_macos_arm64") {
      moduleConfig.artifactMacosArm64 = value;
    } else {
      addError(lineNumber, "unknown ncon.modulecpp key: " + key);
      return false;
    }
    outConfig->ncon.native.modules = outConfig->moduleCppModules;
    break;
  }
  case Section::NconPermissions:
    if (key == "fs_read") {
      if (!parseStringArray(value, &outConfig->ncon.permissions.fsRead)) {
        addError(lineNumber,
                 "invalid ncon.permissions.fs_read value (expected [\"...\"])");
        return false;
      }
    } else if (key == "fs_write") {
      if (!parseStringArray(value, &outConfig->ncon.permissions.fsWrite)) {
        addError(
            lineNumber,
            "invalid ncon.permissions.fs_write value (expected [\"...\"])");
        return false;
      }
    } else if (key == "network") {
      if (!parseNconNetworkPolicy(value, &outConfig->ncon.permissions.network)) {
        addError(lineNumber,
                 "invalid ncon.permissions.network value (expected allow/deny)");
        return false;
      }
    } else if (key == "process_spawn") {
      bool allow = false;
      if (!parseBool(value, &allow)) {
        addError(lineNumber,
                 "invalid ncon.permissions.process_spawn value (expected true/false)");
        return false;
      }
      outConfig->ncon.permissions.processSpawnAllowed = allow;
    } else {
      addError(lineNumber, "unknown ncon.permissions key: " + key);
      return false;
    }
    break;
  case Section::NconResources:
    if (key.empty()) {
      addError(lineNumber, "empty resource id in [ncon.resources]");
      return false;
    }
    outConfig->ncon.resources.push_back({key, value});
    break;
  case Section::Web:
    if (key == "canvas_id") {
      outConfig->web.canvasId = value;
    } else if (key == "wgsl_cache") {
      if (!parseBool(value, &outConfig->web.wgslCache)) {
        addError(lineNumber, "invalid web.wgsl_cache value (expected true/false)");
        return false;
      }
    } else if (key == "dev_server_port") {
      if (!parseInt32(value, 1, 65535, &outConfig->web.devServerPort)) {
        addError(lineNumber,
                 "invalid web.dev_server_port value (expected 1..65535)");
        return false;
      }
    } else if (key == "enable_shared_array") {
      if (!parseBool(value, &outConfig->web.enableSharedArray)) {
        addError(lineNumber,
                 "invalid web.enable_shared_array value (expected true/false)");
        return false;
      }
    } else if (key == "initial_memory_mb") {
      if (!parseInt32(value, 1, 65536, &outConfig->web.initialMemoryMb)) {
        addError(lineNumber,
                 "invalid web.initial_memory_mb value (expected 1..65536)");
        return false;
      }
    } else if (key == "maximum_memory_mb") {
      if (!parseInt32(value, 1, 65536, &outConfig->web.maximumMemoryMb)) {
        addError(lineNumber,
                 "invalid web.maximum_memory_mb value (expected 1..65536)");
        return false;
      }
    } else if (key == "wasm_simd") {
      if (!parseBool(value, &outConfig->web.wasmSimd)) {
        addError(lineNumber, "invalid web.wasm_simd value (expected true/false)");
        return false;
      }
    } else {
      addError(lineNumber, "unknown web key: " + key);
      return false;
    }
    break;
  case Section::None:
    addError(lineNumber, "key-value pair found outside of a section");
    return false;
  case Section::Unknown:
    // Forward-compatible: ignore unknown sections.
    break;
  }

  return true;
}

bool ProjectConfigParser::parseKeyValue(const std::string &line,
                                        std::size_t lineNumber,
                                        std::string *outKey,
                                        std::string *outValue) {
  bool inString = false;
  bool escaped = false;
  std::size_t separator = std::string::npos;

  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];

    if (escaped) {
      escaped = false;
      continue;
    }

    if (c == '\\' && inString) {
      escaped = true;
      continue;
    }

    if (c == '"') {
      inString = !inString;
      continue;
    }

    if (c == '=' && !inString) {
      separator = i;
      break;
    }
  }

  if (separator == std::string::npos) {
    addError(lineNumber, "expected '=' in key-value pair");
    return false;
  }

  *outKey = trim(line.substr(0, separator));
  if (outKey->empty()) {
    addError(lineNumber, "empty key in key-value pair");
    return false;
  }

  const std::string rawValue = trim(line.substr(separator + 1));
  if (!parseValue(rawValue, lineNumber, outValue)) {
    return false;
  }

  return true;
}

bool ProjectConfigParser::parseValue(const std::string &rawValue,
                                     std::size_t lineNumber,
                                     std::string *outValue) {
  if (rawValue.empty()) {
    addError(lineNumber, "empty value in key-value pair");
    return false;
  }

  if (rawValue.front() != '"') {
    *outValue = rawValue;
    return true;
  }

  if (rawValue.size() < 2 || rawValue.back() != '"') {
    addError(lineNumber, "unterminated string value");
    return false;
  }

  std::string decoded;
  decoded.reserve(rawValue.size() - 2);

  bool escaped = false;
  for (std::size_t i = 1; i + 1 < rawValue.size(); ++i) {
    const char c = rawValue[i];
    if (escaped) {
      switch (c) {
      case 'n':
        decoded.push_back('\n');
        break;
      case 'r':
        decoded.push_back('\r');
        break;
      case 't':
        decoded.push_back('\t');
        break;
      case '"':
        decoded.push_back('"');
        break;
      case '\\':
        decoded.push_back('\\');
        break;
      default:
        decoded.push_back(c);
        break;
      }
      escaped = false;
      continue;
    }

    if (c == '\\') {
      escaped = true;
    } else {
      decoded.push_back(c);
    }
  }

  if (escaped) {
    addError(lineNumber, "invalid trailing escape in string value");
    return false;
  }

  *outValue = std::move(decoded);
  return true;
}

bool ProjectConfigParser::parseInlineTable(
    const std::string &rawValue, std::size_t lineNumber,
    std::unordered_map<std::string, std::string> *outValues) {
  if (outValues == nullptr) {
    addError(lineNumber, "internal error: null inline-table output");
    return false;
  }
  outValues->clear();

  const std::string trimmed = trimCopy(rawValue);
  if (trimmed.size() < 2 || trimmed.front() != '{' || trimmed.back() != '}') {
    addError(lineNumber, "invalid dependency inline table");
    return false;
  }

  const std::string body = trimCopy(trimmed.substr(1, trimmed.size() - 2));
  if (body.empty()) {
    return true;
  }

  std::vector<std::string> items;
  std::string current;
  bool inString = false;
  bool escaped = false;
  for (char c : body) {
    if (escaped) {
      current.push_back(c);
      escaped = false;
      continue;
    }
    if (c == '\\' && inString) {
      current.push_back(c);
      escaped = true;
      continue;
    }
    if (c == '"') {
      current.push_back(c);
      inString = !inString;
      continue;
    }
    if (c == ',' && !inString) {
      items.push_back(trimCopy(current));
      current.clear();
      continue;
    }
    current.push_back(c);
  }
  if (!trimCopy(current).empty()) {
    items.push_back(trimCopy(current));
  }

  for (const auto &item : items) {
    std::string itemKey;
    std::string itemValue;
    if (!parseKeyValue(item, lineNumber, &itemKey, &itemValue)) {
      return false;
    }
    decodeQuotedKey(&itemKey);
    outValues->emplace(toLowerCopy(itemKey), itemValue);
  }

  return true;
}

void ProjectConfigParser::addError(std::size_t lineNumber,
                                   const std::string &message) {
  std::ostringstream error;
  if (lineNumber == 0) {
    error << m_sourceName << ": error: " << message;
  } else {
    error << m_sourceName << ":" << lineNumber << ": error: " << message;
  }
  m_errors.push_back(error.str());
}

} // namespace neuron
