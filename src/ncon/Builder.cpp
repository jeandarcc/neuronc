#include "neuronc/ncon/Builder.h"

#include "neuronc/ncon/Bytecode.h"
#include "neuronc/cli/ProjectConfig.h"
#include "neuronc/cli/ModuleResolver.h"
#include "neuronc/cli/ModuleCppSupport.h"
#include "neuronc/lexer/Lexer.h"
#include "neuronc/nir/NIRBuilder.h"
#include "neuronc/nir/Optimizer.h"
#include "neuronc/parser/Parser.h"
#include "neuronc/sema/SemanticAnalyzer.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/SHA256.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <system_error>
#include <unordered_set>

namespace neuron::ncon {

namespace fs = std::filesystem;

namespace {

std::string normalizeModuleNameLocal(std::string name) {
  std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return name;
}

struct ResolvedBuild {
  fs::path projectRoot;
  fs::path configPath;
  fs::path sourcePath;
  fs::path outputPath;
  std::optional<ProjectConfig> config;
  std::string configText;
  std::string appName;
  std::string appVersion = "0.1.0";
  std::string entryModule;
};

bool readTextFile(const fs::path &path, std::string *outText,
                  std::string *outError) {
  if (outText == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null text output";
    }
    return false;
  }

  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to open file: " + path.string();
    }
    return false;
  }

  std::ostringstream buffer;
  buffer << in.rdbuf();
  if (!in.good() && !in.eof()) {
    if (outError != nullptr) {
      *outError = "failed to read file: " + path.string();
    }
    return false;
  }

  *outText = buffer.str();
  return true;
}

bool readBinaryFile(const fs::path &path, std::vector<uint8_t> *outBytes,
                    std::string *outError) {
  if (outBytes == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null binary output";
    }
    return false;
  }

  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to open file: " + path.string();
    }
    return false;
  }

  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  in.seekg(0, std::ios::beg);
  if (size < 0) {
    if (outError != nullptr) {
      *outError = "failed to stat file: " + path.string();
    }
    return false;
  }

  outBytes->resize(static_cast<size_t>(size));
  if (!outBytes->empty()) {
    in.read(reinterpret_cast<char *>(outBytes->data()),
            static_cast<std::streamsize>(outBytes->size()));
    if (in.gcount() != static_cast<std::streamsize>(outBytes->size())) {
      if (outError != nullptr) {
        *outError = "failed to read file: " + path.string();
      }
      return false;
    }
  }

  return true;
}

std::string joinLines(const std::vector<std::string> &lines) {
  std::ostringstream out;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i != 0) {
      out << '\n';
    }
    out << lines[i];
  }
  return out.str();
}

std::string joinSemanticErrors(const std::vector<SemanticError> &errors) {
  std::ostringstream out;
  for (size_t i = 0; i < errors.size(); ++i) {
    if (i != 0) {
      out << '\n';
    }
    out << errors[i].toString();
  }
  return out.str();
}

std::unique_ptr<ProgramNode>
mergeResolvedPrograms(std::vector<std::unique_ptr<ProgramNode>> *programs,
                      const std::string &entryModule) {
  auto merged =
      std::make_unique<ProgramNode>(SourceLocation{1, 1, entryModule});
  merged->moduleName = entryModule;

  if (programs == nullptr) {
    return merged;
  }

  std::unordered_set<std::string> seenModuleDecls;
  std::unordered_set<std::string> seenModuleCppDecls;
  for (auto &program : *programs) {
    if (program == nullptr) {
      continue;
    }
    for (auto &decl : program->declarations) {
      if (decl == nullptr) {
        continue;
      }
      if (decl->type == ASTNodeType::ModuleDecl) {
        const auto *moduleDecl = static_cast<const ModuleDeclNode *>(decl.get());
        if (!seenModuleDecls
                 .insert(normalizeModuleNameLocal(moduleDecl->moduleName))
                 .second) {
          continue;
        }
      }
      if (decl->type == ASTNodeType::ModuleCppDecl) {
        const auto *moduleDecl =
            static_cast<const ModuleCppDeclNode *>(decl.get());
        if (!seenModuleCppDecls
                 .insert(normalizeModuleCppName(moduleDecl->moduleName))
                 .second) {
          continue;
        }
      }
      merged->declarations.push_back(std::move(decl));
    }
  }
  return merged;
}

const char *optimizeLabel(BuildOptimizeLevel level) {
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
}

const char *targetCpuLabel(BuildTargetCPU cpu) {
  switch (cpu) {
  case BuildTargetCPU::Native:
    return "native";
  case BuildTargetCPU::Generic:
    return "generic";
  }
  return "generic";
}

const char *tensorProfileLabel(BuildTensorProfile profile) {
  switch (profile) {
  case BuildTensorProfile::Balanced:
    return "balanced";
  case BuildTensorProfile::GemmParity:
    return "gemm_parity";
  case BuildTensorProfile::AIFused:
    return "ai_fused";
  }
  return "balanced";
}

std::string hexDigest(const std::array<uint8_t, 32> &digest) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (uint8_t byte : digest) {
    out << std::setw(2) << static_cast<unsigned>(byte);
  }
  return out.str();
}

void collectModulesFromDir(const fs::path &dir,
                           std::unordered_set<std::string> *outModules) {
  if (outModules == nullptr || !fs::exists(dir) || !fs::is_directory(dir)) {
    return;
  }

  for (const auto &entry : fs::recursive_directory_iterator(dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".nr") {
      continue;
    }

    std::string name = entry.path().stem().string();
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
      return static_cast<char>(std::tolower(ch));
    });
    outModules->insert(name);
  }
}

std::unordered_set<std::string>
collectAvailableModules(const fs::path &projectRoot, const fs::path &sourcePath,
                        const std::optional<ProjectConfig> &config) {
  std::unordered_set<std::string> modules = {
      "system",  "math",     "io",      "time",   "random",
      "logger",  "tensor",   "nn",      "dataset", "image",
      "resource",
  };

  std::string sourceStem = sourcePath.stem().string();
  std::transform(sourceStem.begin(), sourceStem.end(), sourceStem.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  modules.insert(sourceStem);

  collectModulesFromDir(projectRoot / "src", &modules);
  collectModulesFromDir(projectRoot / "modules", &modules);

  if (config.has_value()) {
    for (const auto &dependency : config->dependencies) {
      std::string normalized = dependency.first;
      std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                     [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                     });
      modules.insert(normalized);
    }
  }

  return modules;
}

std::string quotePath(const fs::path &path) {
  return "\"" + path.string() + "\"";
}

std::string currentHostPlatform() {
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
  return "windows_x64";
#elif defined(__linux__) && defined(__x86_64__)
  return "linux_x64";
#elif defined(__APPLE__) && defined(__aarch64__)
  return "macos_arm64";
#else
  return "unsupported";
#endif
}

std::vector<fs::path> candidateLibraryNames(const std::string &targetName) {
  if (targetName.empty()) {
    return {};
  }

  std::vector<fs::path> candidates;
#if defined(_WIN32)
  candidates.push_back(targetName + ".dll");
  candidates.push_back("lib" + targetName + ".dll");
#elif defined(__APPLE__)
  candidates.push_back("lib" + targetName + ".dylib");
  candidates.push_back(targetName + ".dylib");
#else
  candidates.push_back("lib" + targetName + ".so");
  candidates.push_back(targetName + ".so");
#endif
  return candidates;
}

bool findBuiltNativeArtifact(const fs::path &buildDir, const std::string &targetName,
                             fs::path *outArtifact, std::string *outError) {
  if (outArtifact == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null native artifact output";
    }
    return false;
  }

  const auto candidates = candidateLibraryNames(targetName);
  for (const auto &entry : fs::recursive_directory_iterator(buildDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    for (const auto &candidate : candidates) {
      if (entry.path().filename() == candidate) {
        *outArtifact = entry.path();
        return true;
      }
    }
  }

  if (outError != nullptr) {
    *outError = "failed to locate built native artifact for target '" +
                targetName + "' in " + buildDir.string();
  }
  return false;
}

bool buildModuleCppFromSource(const fs::path &projectRoot,
                              const LoadedModuleCppModule &module,
                              const std::string &hostPlatform,
                              fs::path *outArtifact, std::string *outError) {
  if (outArtifact == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null native build artifact output";
    }
    return false;
  }
  if (module.config.sourceDir.empty()) {
    if (outError != nullptr) {
      *outError = "modulecpp '" + module.name + "' has no source_dir";
    }
    return false;
  }

  const std::string buildSystem =
      module.config.buildSystem.empty() ? "cmake" : module.config.buildSystem;
  if (buildSystem != "cmake") {
    if (outError != nullptr) {
      *outError = "modulecpp '" + module.name +
                  "' uses unsupported build_system: " + buildSystem;
    }
    return false;
  }
  if (module.config.cmakeTarget.empty()) {
    if (outError != nullptr) {
      *outError = "modulecpp '" + module.name +
                  "' requires cmake_target when source_dir is set";
    }
    return false;
  }

  const fs::path sourceDir = (projectRoot / module.config.sourceDir).lexically_normal();
  const fs::path buildDir =
      projectRoot / "build" / "modulecpp" / module.name / hostPlatform;
  std::error_code ec;
  fs::create_directories(buildDir, ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to create native build directory '" +
                  buildDir.string() + "': " + ec.message();
    }
    return false;
  }

  const std::string configureCommand =
      "cmake -S " + quotePath(sourceDir) + " -B " + quotePath(buildDir);
  if (std::system(configureCommand.c_str()) != 0) {
    if (outError != nullptr) {
      *outError = "failed to configure modulecpp '" + module.name + "'";
    }
    return false;
  }

  std::string buildCommand =
      "cmake --build " + quotePath(buildDir) + " --target " +
      module.config.cmakeTarget;
#if defined(_WIN32)
  buildCommand += " --config Release";
#endif
  if (std::system(buildCommand.c_str()) != 0) {
    if (outError != nullptr) {
      *outError = "failed to build modulecpp '" + module.name + "'";
    }
    return false;
  }

  return findBuiltNativeArtifact(buildDir, module.config.cmakeTarget, outArtifact,
                                 outError);
}

std::unordered_set<std::string>
collectImportedModuleCppModules(const ProgramNode *program) {
  std::unordered_set<std::string> modules;
  if (program == nullptr) {
    return modules;
  }
  for (const auto &decl : program->declarations) {
    if (decl->type != ASTNodeType::ModuleCppDecl) {
      continue;
    }
    auto *module = static_cast<ModuleCppDeclNode *>(decl.get());
    modules.insert(normalizeModuleCppName(module->moduleName));
  }
  return modules;
}

bool resolveBuild(const BuildRequest &request, ResolvedBuild *outBuild,
                  std::string *outError) {
  if (outBuild == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null build output";
    }
    return false;
  }

  fs::path input = request.inputPath.empty() ? fs::current_path() : request.inputPath;
  if (!input.is_absolute()) {
    input = fs::absolute(input);
  }
  if (input.filename() == "neuron.toml") {
    input = input.parent_path();
  }

  if (fs::exists(input) && fs::is_directory(input)) {
    outBuild->projectRoot = input;
    outBuild->configPath = outBuild->projectRoot / "neuron.toml";
    if (!fs::exists(outBuild->configPath)) {
      if (outError != nullptr) {
        *outError = "ncon build expected neuron.toml in " +
                    outBuild->projectRoot.string();
      }
      return false;
    }

    ProjectConfigParser parser;
    ProjectConfig config;
    if (!parser.parseFile(outBuild->configPath.string(), &config)) {
      if (outError != nullptr) {
        *outError = joinLines(parser.errors());
      }
      return false;
    }
    outBuild->config = config;
    if (!readTextFile(outBuild->configPath, &outBuild->configText, outError)) {
      return false;
    }

    outBuild->sourcePath =
        outBuild->projectRoot /
        fs::path(config.mainFile.empty() ? "src/Main.nr" : config.mainFile);
    outBuild->entryModule = outBuild->sourcePath.stem().string();
    outBuild->appName =
        config.name.empty() ? outBuild->projectRoot.filename().string() : config.name;
    outBuild->appVersion = config.version.empty() ? "0.1.0" : config.version;

    if (!request.outputPath.empty()) {
      outBuild->outputPath = request.outputPath.is_absolute()
                                 ? request.outputPath
                                 : fs::absolute(request.outputPath);
    } else if (!config.ncon.outputPath.empty()) {
      outBuild->outputPath = outBuild->projectRoot / config.ncon.outputPath;
    } else {
      outBuild->outputPath =
          outBuild->projectRoot / "build" / "containers" /
          (outBuild->appName + "-" + outBuild->appVersion + ".ncon");
    }
    return true;
  }

  if (!fs::exists(input) || input.extension() != ".nr") {
    if (outError != nullptr) {
      *outError = "ncon build expected a project directory or .nr file";
    }
    return false;
  }

  outBuild->sourcePath = input;
  outBuild->projectRoot = input.parent_path();
  outBuild->entryModule = input.stem().string();
  outBuild->appName = input.stem().string();

  const fs::path siblingConfig = outBuild->projectRoot / "neuron.toml";
  if (fs::exists(siblingConfig)) {
    ProjectConfigParser parser;
    ProjectConfig config;
    if (!parser.parseFile(siblingConfig.string(), &config)) {
      if (outError != nullptr) {
        *outError = joinLines(parser.errors());
      }
      return false;
    }
    outBuild->config = config;
    outBuild->configPath = siblingConfig;
    if (!readTextFile(outBuild->configPath, &outBuild->configText, outError)) {
      return false;
    }
    if (!config.name.empty()) {
      outBuild->appName = config.name;
    }
    if (!config.version.empty()) {
      outBuild->appVersion = config.version;
    }
  }

  outBuild->outputPath = !request.outputPath.empty()
                             ? (request.outputPath.is_absolute()
                                    ? request.outputPath
                                    : fs::absolute(request.outputPath))
                             : (outBuild->sourcePath.parent_path() /
                                (outBuild->sourcePath.stem().string() + ".ncon"));
  return true;
}

std::string buildDebugMap(const Program &program, const fs::path &sourcePath) {
  std::ostringstream out;
  out << "source: " << sourcePath.lexically_normal().string() << "\n";
  out << "module: " << program.moduleName << "\n";
  out << "entry_function_id: " << program.entryFunctionId << "\n";
  out << "functions:\n";
  for (size_t i = 0; i < program.functions.size(); ++i) {
    const FunctionRecord &record = program.functions[i];
    const std::string name =
        record.nameStringId < program.strings.size()
            ? program.strings[record.nameStringId]
            : std::string("<unnamed>");
    out << "  - #" << i << " " << name << " args=" << record.argCount
        << " slots=" << record.slotCount << " blocks=" << record.blockCount
        << "\n";
  }
  return out.str();
}

} // namespace

bool buildContainerFromInput(const BuildRequest &request,
                             std::filesystem::path *outPath,
                             std::string *outError) {
  ResolvedBuild build;
  if (!resolveBuild(request, &build, outError)) {
    return false;
  }
  if (!fs::exists(build.sourcePath)) {
    if (outError != nullptr) {
      *outError = "entry file not found: " + build.sourcePath.string();
    }
    return false;
  }

  std::string source;
  if (!readTextFile(build.sourcePath, &source, outError)) {
    return false;
  }

  ModuleResolverOptions resolverOptions;
  resolverOptions.autoAddMissingPackages = true;
  resolverOptions.autoIncludeBuiltinLibraries = true;
  resolverOptions.builtinLibrariesRoot = build.projectRoot / "BuiltinLibraries";
  const ModuleResolverResult resolved = ModuleResolver::resolve(
      build.sourcePath, build.config,
      [&](const fs::path &path, std::string *readError) {
        std::string text;
        return readTextFile(path, &text, readError) ? text : std::string();
      },
      resolverOptions);
  if (!resolved.errors.empty()) {
    if (outError != nullptr) {
      *outError = joinLines(resolved.errors);
    }
    return false;
  }

  std::vector<std::unique_ptr<ProgramNode>> parsedPrograms;
  parsedPrograms.reserve(resolved.orderedSources.size());
  for (const auto &moduleSource : resolved.orderedSources) {
    Lexer lexer(moduleSource.sourceText, moduleSource.path.string());
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) {
      if (outError != nullptr) {
        *outError = joinLines(lexer.errors());
      }
      return false;
    }

    Parser parser(std::move(tokens), moduleSource.path.string());
    auto parsed = parser.parse();
    if (!parser.errors().empty()) {
      if (outError != nullptr) {
        *outError = joinLines(parser.errors());
      }
      return false;
    }
    parsedPrograms.push_back(std::move(parsed));
  }

  auto ast = mergeResolvedPrograms(&parsedPrograms, build.entryModule);

  SemanticAnalyzer sema;
  std::unordered_map<std::string, NativeModuleInfo> moduleCppModules;
  std::vector<LoadedModuleCppModule> loadedModules;
  std::vector<std::string> moduleErrors;
  const std::unordered_set<std::string> importedModuleCppModules =
      collectImportedModuleCppModules(ast.get());
  const bool hasBuiltinNativeProvider = std::any_of(
      importedModuleCppModules.begin(), importedModuleCppModules.end(),
      [&](const std::string &moduleName) {
        auto providerIt = resolved.moduleProviders.find(moduleName);
        return providerIt != resolved.moduleProviders.end() &&
               providerIt->second == ModuleProviderKind::BuiltinNative;
      });
  if (build.config.has_value() &&
      !loadProjectModuleCppModules(build.projectRoot, *build.config,
                                   &moduleCppModules, &loadedModules,
                                   &moduleErrors)) {
    if (outError != nullptr) {
      *outError = joinLines(moduleErrors);
    }
    return false;
  }
  sema.setAvailableModules(
      resolved.availableModules.empty()
          ? collectAvailableModules(build.projectRoot, build.sourcePath,
                                    build.config)
          : resolved.availableModules,
      true);
  sema.setModuleCppModules(moduleCppModules);
  sema.analyze(ast.get());
  if (sema.hasErrors()) {
    if (outError != nullptr) {
      *outError = joinSemanticErrors(sema.getErrors());
    }
    return false;
  }

  nir::NIRBuilder nirBuilder;
  nirBuilder.setModuleCppModules(moduleCppModules);
  auto module = nirBuilder.build(ast.get(), build.entryModule);
  auto optimizer = nir::Optimizer::createDefaultOptimizer();
  optimizer->run(module.get());

  Program program;
  llvm::SHA256 hash;
  hash.update(llvm::StringRef(source));
  if (!build.configText.empty()) {
    hash.update(llvm::StringRef(build.configText));
  }

  ContainerData container;
  std::vector<ResourceInfo> manifestResources;
  if (build.config.has_value()) {
    for (const auto &binding : build.config->ncon.resources) {
      const fs::path resourcePath =
          (build.projectRoot / binding.sourcePath).lexically_normal();
      std::vector<uint8_t> bytes;
      if (!readBinaryFile(resourcePath, &bytes, outError)) {
        return false;
      }
      hash.update(llvm::ArrayRef<uint8_t>(bytes));

      const uint64_t blobOffset = container.resourcesBlob.size();
      container.resourcesBlob.insert(container.resourcesBlob.end(), bytes.begin(),
                                     bytes.end());

      const uint32_t resourceCrc = crc32(bytes);
      container.resources.push_back(
          {binding.logicalId, blobOffset, bytes.size(), resourceCrc});
      manifestResources.push_back(
          {binding.logicalId, binding.sourcePath, bytes.size(), resourceCrc, blobOffset});
    }
  }

  LowerToProgramOptions lowerOptions;
  std::vector<NativeModuleManifestInfo> nativeManifestModules;
  const std::string hostPlatform = currentHostPlatform();
  for (const auto &loadedModule : loadedModules) {
    const std::string normalizedName = normalizeModuleCppName(loadedModule.name);
    if (importedModuleCppModules.find(normalizedName) ==
        importedModuleCppModules.end()) {
      continue;
    }

    if (build.config.has_value() && !build.config->ncon.native.enabled &&
        !hasBuiltinNativeProvider) {
      if (outError != nullptr) {
        *outError =
            "modulecpp imports require [ncon.native].enabled = true in neuron.toml unless the module is resolved from BuiltinNativeLibraries";
      }
      return false;
    }

    std::string manifestText;
    if (!readTextFile(loadedModule.manifestPath, &manifestText, outError)) {
      return false;
    }
    hash.update(llvm::StringRef(manifestText));

    NativeModuleManifestInfo nativeModule;
    nativeModule.name = loadedModule.name;
    nativeModule.abi = loadedModule.manifest.abi;

    std::map<std::string, fs::path> artifactPaths;
    if (!loadedModule.config.sourceDir.empty()) {
      if (hostPlatform == "unsupported") {
        if (outError != nullptr) {
          *outError = "modulecpp '" + loadedModule.name +
                      "' source builds are not supported on this host platform";
        }
        return false;
      }
      fs::path builtArtifact;
      if (!buildModuleCppFromSource(build.projectRoot, loadedModule, hostPlatform,
                                    &builtArtifact, outError)) {
        return false;
      }
      artifactPaths[hostPlatform] = std::move(builtArtifact);
    }
    if (!loadedModule.config.artifactWindowsX64.empty() &&
        artifactPaths.count("windows_x64") == 0u) {
      artifactPaths["windows_x64"] =
          (build.projectRoot / loadedModule.config.artifactWindowsX64)
              .lexically_normal();
    }
    if (!loadedModule.config.artifactLinuxX64.empty() &&
        artifactPaths.count("linux_x64") == 0u) {
      artifactPaths["linux_x64"] =
          (build.projectRoot / loadedModule.config.artifactLinuxX64)
              .lexically_normal();
    }
    if (!loadedModule.config.artifactMacosArm64.empty() &&
        artifactPaths.count("macos_arm64") == 0u) {
      artifactPaths["macos_arm64"] =
          (build.projectRoot / loadedModule.config.artifactMacosArm64)
              .lexically_normal();
    }
    if (artifactPaths.empty()) {
      if (outError != nullptr) {
        *outError = "modulecpp '" + loadedModule.name +
                    "' has no source_dir or packaged artifacts";
      }
      return false;
    }

    std::vector<std::string> exportNames;
    exportNames.reserve(loadedModule.manifest.exports.size());
    for (const auto &exportEntry : loadedModule.manifest.exports) {
      exportNames.push_back(exportEntry.first);
    }
    std::sort(exportNames.begin(), exportNames.end());
    for (const auto &exportName : exportNames) {
      const auto &exportEntry = loadedModule.manifest.exports.at(exportName);
      NativeExportInfo exportInfo;
      exportInfo.name = exportName;
      exportInfo.symbol = exportEntry.symbol;
      exportInfo.parameterTypes = exportEntry.parameterTypes;
      exportInfo.returnType = exportEntry.returnType;
      nativeModule.exports.push_back(exportInfo);
      lowerOptions.nativeCallTargets.insert(loadedModule.name + "." + exportName);
    }

    for (const auto &artifactEntry : artifactPaths) {
      std::vector<uint8_t> bytes;
      if (!readBinaryFile(artifactEntry.second, &bytes, outError)) {
        return false;
      }
      hash.update(llvm::ArrayRef<uint8_t>(bytes));

      const uint64_t blobOffset = container.resourcesBlob.size();
      container.resourcesBlob.insert(container.resourcesBlob.end(), bytes.begin(),
                                     bytes.end());

      const uint32_t resourceCrc = crc32(bytes);
      const std::string resourceId =
          "__nativemodules__/" + loadedModule.name + "/" + artifactEntry.first +
          "/" + artifactEntry.second.filename().string();
      container.resources.push_back(
          {resourceId, blobOffset, bytes.size(), resourceCrc});

      llvm::SHA256 artifactHash;
      artifactHash.update(llvm::ArrayRef<uint8_t>(bytes));

      NativeArtifactInfo artifactInfo;
      artifactInfo.platform = artifactEntry.first;
      artifactInfo.resourceId = resourceId;
      artifactInfo.fileName = artifactEntry.second.filename().string();
      artifactInfo.size = bytes.size();
      artifactInfo.crc32 = resourceCrc;
      artifactInfo.sha256 = hexDigest(artifactHash.final());
      nativeModule.artifacts.push_back(std::move(artifactInfo));
    }

    nativeManifestModules.push_back(std::move(nativeModule));
  }

  if (!lowerToProgram(*module, &program, outError, lowerOptions)) {
    return false;
  }
  container.program = std::move(program);

  ManifestData manifest;
  manifest.appName = build.appName;
  manifest.appVersion = build.appVersion;
  manifest.entryModule = build.entryModule;
  manifest.entryFunction = "Init";
  manifest.sourceHash = hexDigest(hash.final());
  manifest.optimize = build.config.has_value()
                          ? optimizeLabel(build.config->optimizeLevel)
                          : "aggressive";
  manifest.targetCPU = build.config.has_value()
                           ? targetCpuLabel(build.config->targetCPU)
                           : "generic";
  manifest.tensorProfile = build.config.has_value()
                               ? tensorProfileLabel(build.config->tensorProfile)
                               : "balanced";
  manifest.tensorAutotune =
      build.config.has_value() ? build.config->tensorAutotune : true;
  manifest.hotReload =
      build.config.has_value() ? build.config->ncon.hotReload : false;
  manifest.nativeEnabled =
      build.config.has_value() ? build.config->ncon.native.enabled : false;
  manifest.permissions = build.config.has_value()
                             ? build.config->ncon.permissions
                             : NconPermissionConfig{};
  manifest.resources = std::move(manifestResources);
  manifest.nativeModules = std::move(nativeManifestModules);

  container.manifestJson = buildManifestJson(manifest);
  container.debugMap = buildDebugMap(container.program, build.sourcePath);

  std::error_code ec;
  fs::create_directories(build.outputPath.parent_path(), ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to create output directory: " + ec.message();
    }
    return false;
  }

  if (!writeContainer(build.outputPath, container, outError)) {
    return false;
  }

  if (outPath != nullptr) {
    *outPath = build.outputPath;
  }
  return true;
}

} // namespace neuron::ncon
