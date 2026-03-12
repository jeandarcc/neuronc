#pragma once

#include "neuronc/ncon/Permissions.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace neuron {

enum class BuildOptimizeLevel { O0, O1, O2, O3, Oz, Aggressive };
enum class BuildEmitIR { Optimized, Both, None };
enum class BuildTargetCPU { Native, Generic };
enum class BuildTensorProfile { Balanced, GemmParity, AIFused };
enum class PackageKind { Application, Library };

struct NconResourceBinding {
  std::string logicalId;
  std::string sourcePath;
};

struct ModuleCppConfig {
  std::string manifestPath;
  std::string buildSystem;
  std::string sourceDir;
  std::string cmakeTarget;
  std::string artifactWindowsX64;
  std::string artifactLinuxX64;
  std::string artifactMacosArm64;
};

using NconModuleCppConfig = ModuleCppConfig;

struct NconNativeConfig {
  bool enabled = false;
  std::unordered_map<std::string, ModuleCppConfig> modules;
};

struct NconConfig {
  bool enabled = false;
  std::string outputPath;
  bool includeDebugMap = true;
  bool hotReload = false;
  std::vector<NconResourceBinding> resources;
  NconPermissionConfig permissions;
  NconNativeConfig native;
};

struct WebConfig {
  std::string canvasId = "neuron-canvas";
  bool wgslCache = true;
  int32_t devServerPort = 8080;
  bool enableSharedArray = true;
  int32_t initialMemoryMb = 64;
  int32_t maximumMemoryMb = 512;
  bool wasmSimd = true;
};

struct PackageConfig {
  bool enabled = false;
  PackageKind kind = PackageKind::Application;
  std::string description;
  std::string repository;
  std::string license;
  std::string sourceDir = "src";
};

struct DependencySpec {
  std::string name;
  std::string github;
  std::string version;
  std::string tag;
  std::string commit;
  bool legacyShorthand = false;

  bool empty() const {
    return github.empty() && version.empty() && tag.empty() && commit.empty();
  }
};

struct ProjectConfig {
  std::string name;
  std::string version = "0.1.0";
  std::string mainFile = "src/Main.npp";
  std::string buildDir = "build";
  BuildOptimizeLevel optimizeLevel = BuildOptimizeLevel::Aggressive;
  BuildEmitIR emitIR = BuildEmitIR::Optimized;
  BuildTargetCPU targetCPU = BuildTargetCPU::Native;
  BuildTensorProfile tensorProfile = BuildTensorProfile::Balanced;
  bool tensorAutotune = true;
  std::string tensorKernelCache = "build/.neuron_cache/tensor/";
  std::unordered_map<std::string, DependencySpec> dependencies;
  PackageConfig package;
  bool moduleCppEnabled = false;
  std::unordered_map<std::string, ModuleCppConfig> moduleCppModules;
  NconConfig ncon;
  WebConfig web;
};

class ProjectConfigParser {
public:
  bool parseFile(const std::string &path, ProjectConfig *outConfig);
  bool parseString(const std::string &content, const std::string &sourceName,
                   ProjectConfig *outConfig);

  const std::vector<std::string> &errors() const { return m_errors; }

private:
  enum class Section {
    None,
    Project,
    Build,
    Dependencies,
    Package,
    ModuleCpp,
    Ncon,
    NconNative,
    NconModuleCpp,
    NconPermissions,
    NconResources,
    Web,
    Unknown,
  };

  static std::string trim(std::string_view text);
  static std::string stripComment(const std::string &line);

  bool parseLine(const std::string &line, std::size_t lineNumber,
                 Section *section, ProjectConfig *outConfig);
  bool parseKeyValue(const std::string &line, std::size_t lineNumber,
                     std::string *outKey, std::string *outValue);
  bool parseValue(const std::string &rawValue, std::size_t lineNumber,
                  std::string *outValue);
  bool parseInlineTable(const std::string &rawValue, std::size_t lineNumber,
                        std::unordered_map<std::string, std::string> *outValues);
  void addError(std::size_t lineNumber, const std::string &message);

  std::string m_sourceName;
  std::vector<std::string> m_errors;
  std::string m_activeNconModuleCpp;
};

} // namespace neuron
