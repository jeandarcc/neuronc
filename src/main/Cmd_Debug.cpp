// Cmd_Debug.cpp — Yardım metni, debug ve compile komutları.
//
// Bu dosya şunları içerir:
//   printUsage   → metin UsageText.h'dan gelir, bu dosyaya dokunma
//   cmdLex, cmdParse, cmdNir  → pipeline'a bağlanan debug komutları
//   makeCompileDeps           → CompilePipeline bağımlılık fabrikası (dahili)
//   cmdCompile                → tek dosya derleme
//
// Yeni bir debug komutu (örn. cmdAst) eklemek için buraya yaz.
#include "CommandHandlers.h"
#include "AppGlobals.h"
#include "BuildSupport.h"
#include "DiagnosticEngine.h"
#include "ProjectHelpers.h"
#include "RuntimePaths.h"
#include "SettingsLoader.h"
#include "ToolchainUtils.h"
#include "UsageText.h"
#include "UsageTextBuilder.h"

#include "neuronc/cli/commands/DebugCommands.h"
#include "neuronc/cli/pipeline/CompilePipeline.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

// ── Yardım metni ────────────────────────────────────────────────────────────
// Metni değiştirmek için UsageText.h dosyasını düzenle — buraya dokunma.

void printUsage() {
  std::cout << neuron::cli::buildUsageText(g_toolRoot);
}

// ── CompilePipeline bağımlılık fabrikası ─────────────────────────────────────
// Hem cmdCompile hem de Cmd_Build.cpp tarafından kullanılır.
// Tanım burada, dışarıya açmak için CompilePipelineDeps.h yapılabilir.

neuron::cli::CompilePipelineDependencies makeCompileDeps() {
  neuron::cli::CompilePipelineDependencies deps;
  deps.reportStringDiagnostics        = reportStringDiagnostics;
  deps.reportSemanticDiagnostics      = reportSemanticDiagnostics;
  deps.loadNeuronSettings             = loadNeuronSettings;
  deps.readFile                       = readFile;
  deps.tryLoadProjectConfigFromCwd    = tryLoadProjectConfigFromCwd;
  deps.collectAvailableModules        = collectAvailableModules;
  deps.collectImportedModuleCppModules = collectImportedModuleCppModules;
  deps.applyTensorRuntimeEnv          = applyTensorRuntimeEnv;
  deps.toLLVMOptLevel                 = toLLVMOptLevel;
  deps.toLLVMTargetCPU                = toLLVMTargetCPU;
  deps.optLevelLabel                  = optLevelLabel;
  deps.countNIRInstructions           = countNIRInstructions;
  deps.currentHostPlatform            = currentHostPlatform;
  deps.ensureRuntimeObjects           = ensureRuntimeObjects;
  deps.runtimeObjectDirectory         = runtimeObjectDirectory;
  deps.runtimeSharedLibraryPath       = runtimeSharedLibraryPath;
  deps.runtimeSharedLinkPath          = runtimeSharedLinkPath;
  deps.quotePath  = [](const fs::path &p) { return quotePath(p); };
  deps.resolveToolCommand             = resolveToolCommand;
  deps.runSystemCommand               = runSystemCommand;
  deps.copyBundledRuntimeDlls         = copyBundledRuntimeDlls;
  deps.copyFileIfExists               = copyFileIfExists;
  deps.buildModuleCppFromSource       = buildModuleCppFromSource;
  return deps;
}

// ── Debug komutları ──────────────────────────────────────────────────────────

int cmdLex(const std::string &filepath) {
  neuron::cli::DebugCommandDependencies deps;
  deps.reportStringDiagnostics          = reportStringDiagnostics;
  deps.reportSemanticDiagnostics        = reportSemanticDiagnostics;
  deps.tryLoadProjectConfigFromCwd      = tryLoadProjectConfigFromCwd;
  deps.configureSemanticAnalyzerModules = configureSemanticAnalyzerModules;
  deps.toolRoot                         = g_toolRoot;
  deps.loadNeuronSettings               = loadNeuronSettings;
  deps.readFile                         = readFile;
  return neuron::cli::runLexCommand(filepath, deps);
}

int cmdParse(const std::string &filepath) {
  neuron::cli::DebugCommandDependencies deps;
  deps.reportStringDiagnostics          = reportStringDiagnostics;
  deps.reportSemanticDiagnostics        = reportSemanticDiagnostics;
  deps.tryLoadProjectConfigFromCwd      = tryLoadProjectConfigFromCwd;
  deps.configureSemanticAnalyzerModules = configureSemanticAnalyzerModules;
  deps.toolRoot                         = g_toolRoot;
  deps.loadNeuronSettings               = loadNeuronSettings;
  deps.readFile                         = readFile;
  return neuron::cli::runParseCommand(filepath, deps);
}

int cmdNir(const std::string &filepath) {
  neuron::cli::DebugCommandDependencies deps;
  deps.reportStringDiagnostics          = reportStringDiagnostics;
  deps.reportSemanticDiagnostics        = reportSemanticDiagnostics;
  deps.tryLoadProjectConfigFromCwd      = tryLoadProjectConfigFromCwd;
  deps.configureSemanticAnalyzerModules = configureSemanticAnalyzerModules;
  deps.toolRoot                         = g_toolRoot;
  deps.loadNeuronSettings               = loadNeuronSettings;
  deps.readFile                         = readFile;
  return neuron::cli::runNirCommand(filepath, deps);
}

// ── Compile ──────────────────────────────────────────────────────────────────

int cmdCompile(const std::string &filepath,
               neuron::LLVMCodeGenOptions *outRuntimeOptions) {
  // outRuntimeOptions: CompilePipeline API'sinde henüz yok, gelecek için ayrıldı.
  (void)outRuntimeOptions;
  return neuron::cli::runCompilePipeline(filepath, makeCompileDeps(), g_toolRoot);
}

