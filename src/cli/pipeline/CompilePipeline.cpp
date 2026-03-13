#include "neuronc/cli/pipeline/CompilePipeline.h"

#include "neuronc/frontend/Frontend.h"
#include "neuronc/lexer/Lexer.h"
#include "neuronc/cli/ModuleResolver.h"
#include "neuronc/cli/SettingsMacros.h"
#include "neuronc/nir/NIRBuilder.h"
#include "neuronc/nir/Optimizer.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace neuron::cli {

namespace {

std::string normalizeModuleNameLocal(std::string name) {
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return name;
}

std::unique_ptr<neuron::ProgramNode>
mergeResolvedPrograms(std::vector<std::unique_ptr<neuron::ProgramNode>> *programs,
                      const std::string &entryModule) {
  auto merged = std::make_unique<neuron::ProgramNode>(
      neuron::SourceLocation{1, 1, entryModule});
  merged->moduleName = entryModule;

  std::unordered_set<std::string> seenModuleDecls;
  std::unordered_set<std::string> seenModuleCppDecls;
  if (programs == nullptr) {
    return merged;
  }
  for (auto &program : *programs) {
    if (program == nullptr) {
      continue;
    }
    for (auto &decl : program->declarations) {
      if (decl == nullptr) {
        continue;
      }
      if (decl->type == neuron::ASTNodeType::ModuleDecl) {
        const auto *moduleDecl =
            static_cast<const neuron::ModuleDeclNode *>(decl.get());
        if (!seenModuleDecls
                 .insert(normalizeModuleNameLocal(moduleDecl->moduleName))
                 .second) {
          continue;
        }
      }
      if (decl->type == neuron::ASTNodeType::ModuleCppDecl) {
        const auto *moduleDecl =
            static_cast<const neuron::ModuleCppDeclNode *>(decl.get());
        if (!seenModuleCppDecls
                 .insert(neuron::normalizeModuleCppName(moduleDecl->moduleName))
                 .second) {
          continue;
        }
      }
      merged->declarations.push_back(std::move(decl));
    }
  }
  return merged;
}

} // namespace

int runCompilePipeline(const std::string &filepath,
                       const CompilePipelineDependencies &deps,
                       const std::filesystem::path &toolRoot,
                       std::string *outExecutablePath) {
  CompilePipelineOptions options;
  return runCompilePipelineWithOptions(filepath, deps, toolRoot, options,
                                       outExecutablePath);
}

int runCompilePipelineWithOptions(const std::string &filepath,
                                  const CompilePipelineDependencies &deps,
                                  const std::filesystem::path &toolRoot,
                                  const CompilePipelineOptions &options,
                                  std::string *outArtifactPath) {
  const NeuronSettings settings = deps.loadNeuronSettings(fs::path(filepath));
  std::string source = deps.readFile(filepath, settings);
  if (source.empty()) {
    return 1;
  }

  const auto projectConfig = deps.tryLoadProjectConfigFromCwd();
  const fs::path sourcePath(filepath);
  const fs::path projectRoot = projectConfig.has_value()
                                   ? fs::current_path()
                                   : sourcePath.parent_path().parent_path();
  neuron::ModuleResolverOptions resolverOptions;
  resolverOptions.autoAddMissingPackages = settings.packageAutoAddMissing;
  resolverOptions.autoIncludebuiltin_libraries = true;
  resolverOptions.builtin_librariesRoot = toolRoot / "builtin_libraries";
  const neuron::ModuleResolverResult resolved =
      neuron::ModuleResolver::resolve(
          sourcePath, projectConfig,
          [&](const fs::path &path, std::string *outError) {
            const NeuronSettings fileSettings = deps.loadNeuronSettings(path);
            std::string text = deps.readFile(path.string(), fileSettings);
            if (text.empty() && outError != nullptr) {
              *outError = "failed to read source file '" + path.string() + "'";
            }
            return text;
          },
          resolverOptions);
  if (!resolved.errors.empty()) {
    deps.reportStringDiagnostics("module", filepath, source, resolved.errors);
    return 1;
  }

  neuron::cli::SettingsMacroProcessor macroProcessor(toolRoot, sourcePath);
  std::vector<std::string> settingsErrors;
  if (!macroProcessor.initialize(&settingsErrors)) {
    deps.reportStringDiagnostics("config", filepath, "", settingsErrors);
    return 1;
  }

  std::vector<std::unique_ptr<neuron::ProgramNode>> parsedPrograms;
  parsedPrograms.reserve(resolved.orderedSources.size());
  for (const auto &moduleSource : resolved.orderedSources) {
    neuron::Lexer lexer(moduleSource.sourceText, moduleSource.path.string());
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) {
      deps.reportStringDiagnostics("lexer", moduleSource.path.string(),
                                   moduleSource.sourceText, lexer.errors());
      return 1;
    }

    std::vector<neuron::Token> expandedTokens;
    settingsErrors.clear();
    if (!macroProcessor.expandSourceTokens(moduleSource.path, tokens,
                                           &expandedTokens, &settingsErrors)) {
      deps.reportStringDiagnostics("config", moduleSource.path.string(),
                                   moduleSource.sourceText, settingsErrors);
      return 1;
    }

    neuron::frontend::ParseResult parseResult =
        neuron::frontend::parseTokens(std::move(expandedTokens),
                                      moduleSource.path.string());
    if (!parseResult.parserErrors.empty()) {
      deps.reportStringDiagnostics("parser", moduleSource.path.string(),
                                   moduleSource.sourceText,
                                   parseResult.parserErrors);
      return 1;
    }
    parsedPrograms.push_back(std::move(parseResult.program));
  }

  auto ast = mergeResolvedPrograms(&parsedPrograms, sourcePath.stem().string());
  const std::unordered_set<std::string> importedModuleCppModules =
      deps.collectImportedModuleCppModules(ast.get());

  std::vector<std::string> configErrors;
  std::unordered_map<std::string, neuron::NativeModuleInfo> moduleCppModules;
  std::vector<neuron::LoadedModuleCppModule> loadedModuleCppModules;
  neuron::frontend::SemanticOptions semaOptions;
  semaOptions.availableModules = resolved.availableModules;
  semaOptions.enforceModuleResolution = true;
  semaOptions.maxClassesPerFile = settings.maxClassesPerFile;
  semaOptions.requireMethodUppercaseStart =
      settings.requireMethodUppercaseStart;
  semaOptions.enforceStrictFileNaming = settings.enforceStrictFileNaming;
  semaOptions.sourceFileStem = sourcePath.stem().string();
  semaOptions.maxLinesPerMethod = settings.maxLinesPerMethod;
  semaOptions.maxLinesPerBlockStatement = settings.maxLinesPerBlockStatement;
  semaOptions.minMethodNameLength = settings.minMethodNameLength;
  semaOptions.requireClassExplicitVisibility =
      settings.requireClassExplicitVisibility;
  semaOptions.requirePropertyExplicitVisibility =
      settings.requirePropertyExplicitVisibility;
  semaOptions.requireConstUppercase = settings.requireConstUppercase;
  semaOptions.maxNestingDepth = settings.maxNestingDepth;
  semaOptions.requirePublicMethodDocs = settings.requirePublicMethodDocs;
  semaOptions.sourceText = source;
  semaOptions.agentHints = settings.agentHints;
  if (!importedModuleCppModules.empty()) {
    const bool hasBuiltinNativeProvider = std::any_of(
        importedModuleCppModules.begin(), importedModuleCppModules.end(),
        [&](const std::string &moduleName) {
          auto providerIt = resolved.moduleProviders.find(moduleName);
          return providerIt != resolved.moduleProviders.end() &&
                 providerIt->second == neuron::ModuleProviderKind::BuiltinNative;
        });

    if (!projectConfig.has_value()) {
      if (!hasBuiltinNativeProvider) {
        configErrors.push_back(
            "modulecpp imports require a neuron.toml project configuration unless the module is resolved from builtin_native_libraries");
      }
    } else {
      if (!neuron::loadProjectModuleCppModules(
              projectRoot, *projectConfig, &moduleCppModules,
              &loadedModuleCppModules, &configErrors)) {
      }
      if (!projectConfig->ncon.native.enabled && !hasBuiltinNativeProvider) {
        configErrors.push_back(
            "modulecpp imports require [ncon.native].enabled = true in neuron.toml unless the module is resolved from builtin_native_libraries");
      }
    }
  }
  semaOptions.moduleCppModules = moduleCppModules;
  if (!configErrors.empty()) {
    deps.reportStringDiagnostics("config", filepath, source, configErrors);
    return 1;
  }
  neuron::frontend::SemanticResult semanticResult =
      neuron::frontend::analyzeProgram(ast.get(), semaOptions);

  if (semanticResult.hasErrors()) {
    deps.reportSemanticDiagnostics(filepath, source,
                                  semanticResult.analyzer->getErrors());
    return 1;
  }

  neuron::BuildOptimizeLevel buildOptLevel =
      neuron::BuildOptimizeLevel::Aggressive;
  neuron::BuildEmitIR emitIRMode = neuron::BuildEmitIR::Optimized;
  neuron::BuildTargetCPU targetCPU = neuron::BuildTargetCPU::Native;
  if (projectConfig.has_value()) {
    buildOptLevel = projectConfig->optimizeLevel;
    emitIRMode = projectConfig->emitIR;
    targetCPU = projectConfig->targetCPU;
  }
  deps.applyTensorRuntimeEnv(projectConfig);

  neuron::LLVMCodeGenOptions llvmOptions;
  llvmOptions.optLevel = deps.toLLVMOptLevel(buildOptLevel);
  llvmOptions.targetCPU = deps.toLLVMTargetCPU(targetCPU);
  llvmOptions.targetTripleOverride = options.targetTripleOverride;
  llvmOptions.enableWasmSimd = options.enableWasmSimd;

  const fs::path inputPath(filepath);
  const std::string baseName =
      options.outputStemOverride.empty() ? inputPath.stem().string()
                                         : options.outputStemOverride;
  fs::path outputDir;
  if (!options.outputDirOverride.empty()) {
    outputDir = options.outputDirOverride;
  } else if (projectConfig.has_value()) {
    outputDir = fs::path(
        projectConfig->buildDir.empty() ? "build" : projectConfig->buildDir);
  } else if (!inputPath.parent_path().empty()) {
    outputDir = inputPath.parent_path();
  } else {
    outputDir = fs::current_path();
  }
  std::error_code dirEc;
  fs::create_directories(outputDir, dirEc);
  if (dirEc) {
    std::cerr << "Failed to create output directory '" << outputDir.string()
              << "': " << dirEc.message() << std::endl;
    return 1;
  }

  neuron::nir::NIRBuilder nirBuilder;
  nirBuilder.setModuleCppModules(moduleCppModules);
  auto nirModule = nirBuilder.build(ast.get(), filepath);
  if (nirBuilder.hasErrors()) {
    deps.reportStringDiagnostics("nir", filepath, source, nirBuilder.errors());
    return 1;
  }
  const std::size_t nirInstBefore = deps.countNIRInstructions(nirModule.get());

  auto optimizer = neuron::nir::Optimizer::createDefaultOptimizer();
  optimizer->run(nirModule.get());
  const std::size_t nirInstAfter = deps.countNIRInstructions(nirModule.get());

  std::unordered_map<std::string, neuron::ModuleCppCompileExport>
      compileModuleCppExports;
  std::vector<std::pair<fs::path, fs::path>> compileModuleCppArtifactCopies;
  if (!importedModuleCppModules.empty()) {
    const std::string hostPlatform = deps.currentHostPlatform();
    if (hostPlatform == "unsupported") {
      std::cerr << "Error: modulecpp is unsupported on this host platform."
                << std::endl;
      return 1;
    }

    for (const auto &loadedModule : loadedModuleCppModules) {
      const std::string normalizedName =
          neuron::normalizeModuleCppName(loadedModule.name);
      if (importedModuleCppModules.find(normalizedName) ==
          importedModuleCppModules.end()) {
        continue;
      }

      fs::path artifactPath;
      if (!loadedModule.config.sourceDir.empty()) {
        std::string buildError;
        if (!deps.buildModuleCppFromSource(projectRoot, loadedModule,
                                           hostPlatform, &artifactPath,
                                           &buildError)) {
          std::cerr << "Error: " << buildError << std::endl;
          return 1;
        }
      } else if (hostPlatform == "windows_x64" &&
                 !loadedModule.config.artifactWindowsX64.empty()) {
        artifactPath = (projectRoot / loadedModule.config.artifactWindowsX64)
                           .lexically_normal();
      } else if (hostPlatform == "linux_x64" &&
                 !loadedModule.config.artifactLinuxX64.empty()) {
        artifactPath = (projectRoot / loadedModule.config.artifactLinuxX64)
                           .lexically_normal();
      } else if (hostPlatform == "macos_arm64" &&
                 !loadedModule.config.artifactMacosArm64.empty()) {
        artifactPath = (projectRoot / loadedModule.config.artifactMacosArm64)
                           .lexically_normal();
      }

      if (artifactPath.empty()) {
        std::cerr << "Error: modulecpp '" << loadedModule.name
                  << "' has no artifact for host platform " << hostPlatform
                  << std::endl;
        return 1;
      }

      const fs::path stagedRelative = fs::path("native_modules") /
                                      loadedModule.name /
                                      artifactPath.filename();
      compileModuleCppArtifactCopies.push_back(
          {artifactPath, outputDir / stagedRelative});

      for (const auto &exportEntry : loadedModule.manifest.exports) {
        neuron::ModuleCppCompileExport compileExport;
        compileExport.callTarget = loadedModule.name + "." + exportEntry.first;
        compileExport.libraryPath = stagedRelative.generic_string();
        compileExport.symbolName = exportEntry.second.symbol;
        compileExport.parameterTypes = exportEntry.second.parameterTypes;
        compileExport.returnType = exportEntry.second.returnType;
        compileModuleCppExports[compileExport.callTarget] =
            std::move(compileExport);
      }
    }
  }

  neuron::LLVMCodeGen codegen;
  codegen.setModuleCppExports(compileModuleCppExports);
  if (!options.graphicsShaderOutputDir.empty()) {
    codegen.setGraphicsShaderOutputDirectory(options.graphicsShaderOutputDir);
  }
  if (!options.graphicsShaderCacheDir.empty()) {
    codegen.setGraphicsShaderCacheDirectory(options.graphicsShaderCacheDir);
  }
  codegen.setGraphicsShaderAllowCache(options.graphicsShaderAllowCache);
  codegen.generate(nirModule.get());
  const std::size_t llvmInstBefore = codegen.instructionCount();
  std::string optError;
  if (!codegen.optimizeModule(llvmOptions, &optError)) {
    std::cerr << "LLVM optimization failed:\n" << optError << std::endl;
    return 1;
  }
  const std::size_t llvmInstAfter = codegen.instructionCount();

  fs::path llFile = outputDir / (baseName + ".ll");
  fs::path rawLlFile = outputDir / (baseName + ".raw.ll");
  const std::string objExt =
      options.objectExtension.empty() ? ".obj" : options.objectExtension;
  const std::string exeExt =
      options.executableExtension.empty() ? ".exe" : options.executableExtension;
  fs::path objFile = outputDir / (baseName + objExt);
  fs::path exeFile = outputDir / (baseName + exeExt);

  if (emitIRMode == neuron::BuildEmitIR::Both) {
    neuron::LLVMCodeGen rawCodegen;
    rawCodegen.generate(nirModule.get());
    codegen.writeIR(llFile.string());
    rawCodegen.writeIR(rawLlFile.string());
    std::cout << "LLVM IR written to " << llFile.string() << std::endl;
    std::cout << "Unoptimized LLVM IR written to " << rawLlFile.string()
              << std::endl;
  } else if (emitIRMode == neuron::BuildEmitIR::Optimized) {
    codegen.writeIR(llFile.string());
    std::cout << "LLVM IR written to " << llFile.string() << std::endl;
  } else {
    std::cout << "LLVM IR output disabled (build.emit_ir = none)."
              << std::endl;
  }

  if (!codegen.compileToObject(objFile.string(), llvmOptions, &optError)) {
    std::cerr << "Failed to compile to object file:\n" << optError
              << std::endl;
    return 1;
  }

  std::cout << "Optimization summary: NIR " << nirInstBefore << " -> "
            << nirInstAfter << ", LLVM " << llvmInstBefore << " -> "
            << llvmInstAfter << " (" << deps.optLevelLabel(buildOptLevel)
            << ")" << std::endl;

  if (!options.linkExecutable) {
    std::cout << "Object artifact: " << objFile.string() << std::endl;
    if (outArtifactPath != nullptr) {
      *outArtifactPath = objFile.string();
    }
    return 0;
  }

  if (!deps.ensureRuntimeObjects(llvmOptions)) {
    return 1;
  }
  const fs::path runtimeLibrary = deps.runtimeSharedLibraryPath();
  const fs::path runtimeLinkTarget = deps.runtimeSharedLinkPath();
  if (!fs::exists(runtimeLibrary) || !fs::exists(runtimeLinkTarget)) {
    std::cerr << "Shared runtime artifact missing after build: "
              << runtimeLibrary.string() << std::endl;
    return 1;
  }

  std::string linkCmd = deps.resolveToolCommand("g++") + " -o " +
                        deps.quotePath(exeFile) + " " +
                        deps.quotePath(objFile) + " " +
                        deps.quotePath(runtimeLinkTarget);
#if defined(__APPLE__)
  linkCmd += " -Wl,-rpath,@loader_path";
#elif !defined(_WIN32)
  linkCmd += " -Wl,-rpath,'$ORIGIN'";
#endif
  std::cout << "Linking: " << linkCmd << std::endl;
  int linkResult = deps.runSystemCommand(linkCmd);
  if (linkResult != 0) {
    std::cerr << "Linking failed" << std::endl;
    return 1;
  }

  std::string copyError;
  const bool useOpenMp = llvmOptions.optLevel == neuron::LLVMOptLevel::O3 ||
                         llvmOptions.optLevel == neuron::LLVMOptLevel::Aggressive;
  if (!deps.copyBundledRuntimeDlls(exeFile.parent_path(), useOpenMp,
                                   &copyError)) {
    std::cerr << copyError << std::endl;
    return 1;
  }
  if (!deps.copyFileIfExists(runtimeLibrary, exeFile.parent_path() / runtimeLibrary.filename(),
                             &copyError)) {
    std::cerr << copyError << std::endl;
    return 1;
  }
  for (const auto &artifactCopy : compileModuleCppArtifactCopies) {
    if (!deps.copyFileIfExists(artifactCopy.first, artifactCopy.second,
                               &copyError)) {
      std::cerr << copyError << std::endl;
      return 1;
    }
  }

  std::cout << "\n=== Compilation successful! ===" << std::endl;
  std::cout << "Executable: " << exeFile.string() << std::endl;
  if (outArtifactPath != nullptr) {
    *outArtifactPath = exeFile.string();
  }
  return 0;
}

} // namespace neuron::cli

