// Cmd_Build.cpp — Derleme, build-nucleus, build-product, run ve release komutları.
//
// Bu dosya şunları içerir:
//   makeProjectDeps   → ProjectCommandDependencies fabrikası (dahili)
//   cmdBuildMinimal   → ncon runtime için tek dosya derleme
//   cmdBuildProduct   → .productsettings ile ürün paketi oluşturma
//   cmdBuild          → proje build komutu
//   cmdRun            → proje run komutu
//   cmdRelease        → proje release komutu
//
// makeCompileDeps() Cmd_Debug.cpp'de tanımlıdır; buradan forward-declare ile
// kullanılır.
#include "CommandHandlers.h"
#include "AppGlobals.h"
#include "BuildSupport.h"
#include "DiagnosticEngine.h"
#include "ProjectHelpers.h"
#include "RuntimePaths.h"
#include "SettingsLoader.h"
#include "ToolchainUtils.h"

#include "neuronc/cli/commands/ProjectCommands.h"
#include "neuronc/cli/MinimalBuilder.h"
#include "neuronc/cli/pipeline/CompilePipeline.h"
#include "neuronc/cli/WebBuildPipeline.h"
#include "neuronc/cli/WebDevServer.h"
#include "neuronc/cli/ProductBuilder.h"
#include "neuronc/cli/ProductSettings.h"

#include <cctype>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

// Forward declaration — tanım Cmd_Debug.cpp'dedir.
neuron::cli::CompilePipelineDependencies makeCompileDeps();

namespace {

std::string toLowerCopy(std::string text) {
  for (char &ch : text) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return text;
}

std::string parseTargetArg(int argc, char *argv[]) {
  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    const std::string prefix = "--target=";
    if (arg.rfind(prefix, 0) == 0) {
      return arg.substr(prefix.size());
    }
    if (arg == "--target" && (i + 1) < argc) {
      return argv[i + 1];
    }
  }
  return std::string();
}

bool hasTargetFlag(int argc, char *argv[]) {
  for (int i = 2; i < argc; ++i) {
    if (argv[i] == nullptr) {
      continue;
    }
    const std::string arg = argv[i];
    if (arg == "--target" || arg.rfind("--target=", 0) == 0) {
      return true;
    }
  }
  return false;
}

bool hasArg(int argc, char *argv[], const std::string &needle) {
  for (int i = 2; i < argc; ++i) {
    if (argv[i] != nullptr && needle == argv[i]) {
      return true;
    }
  }
  return false;
}

int runWebBuildTargetInternal(int argc, char *argv[],
                              neuron::cli::WebBuildResult *outResult) {
  const bool targetProvided = hasTargetFlag(argc, argv);
  const std::string target = toLowerCopy(parseTargetArg(argc, argv));
  if (targetProvided && target.empty()) {
    std::cerr << "Missing value for --target" << std::endl;
    return 1;
  }
  if (target != "web") {
    std::cerr << "Unsupported target for build: "
              << (target.empty() ? "<none>" : target) << std::endl;
    return 1;
  }

  neuron::ProjectConfig config;
  std::vector<std::string> errors;
  if (!loadProjectConfigFromCwd(&config, &errors)) {
    for (const auto &e : errors) {
      std::cerr << e << std::endl;
    }
    return 1;
  }

  neuron::cli::WebBuildRequest request;
  request.toolRoot = g_toolRoot;
  request.projectRoot = fs::current_path();
  request.projectConfig = config;
  request.verbose = hasArg(argc, argv, "--verbose");

  neuron::cli::WebBuildPipelineDependencies deps;
  deps.compileToObject = [](const std::string &filepath,
                            const neuron::cli::CompilePipelineOptions &options,
                            std::string *outObjectPath) {
    return neuron::cli::runCompilePipelineWithOptions(
        filepath, makeCompileDeps(), g_toolRoot, options, outObjectPath);
  };
  deps.resolveToolCommand = resolveToolCommand;
  deps.runSystemCommand = runSystemCommand;
  deps.quotePath = [](const fs::path &path) { return quotePath(path); };

  neuron::cli::WebBuildResult result =
      neuron::cli::runWebBuildPipeline(request, deps);
  for (const std::string &warning : result.warnings) {
    std::cerr << "Web build warning: " << warning << std::endl;
  }

  if (!result.success) {
    std::cerr << "Web build failed: " << result.error << std::endl;
    return 1;
  }

  std::cout << "Web build output: " << result.outputDirectory.string()
            << std::endl;
  std::cout << "Web entry: " << result.htmlEntryPath.string() << std::endl;

  if (outResult != nullptr) {
    *outResult = std::move(result);
  }
  return 0;
}

} // namespace

// ── ProjectCommand bağımlılık fabrikası ──────────────────────────────────────

static neuron::cli::ProjectCommandDependencies makeProjectDeps() {
  neuron::cli::ProjectCommandDependencies deps;
  deps.loadProjectConfigFromCwd         = loadProjectConfigFromCwd;
  deps.loadNeuronSettings               = loadNeuronSettings;
  deps.readFile                         = readFile;
  deps.reportStringDiagnostics          = reportStringDiagnostics;
  deps.reportSemanticDiagnostics        = reportSemanticDiagnostics;
  deps.configureSemanticAnalyzerModules = configureSemanticAnalyzerModules;
  deps.runAutomatedTestSuite            = runAutomatedTestSuite;
  deps.cmdCompile = [](const std::string &fp, std::string *out) {
    return neuron::cli::runCompilePipeline(fp, makeCompileDeps(), g_toolRoot, out);
  };
  deps.cmdCompileWithOptions =
      [](const std::string &fp, const neuron::cli::CompilePipelineOptions &options,
         std::string *out) {
        return neuron::cli::runCompilePipelineWithOptions(
            fp, makeCompileDeps(), g_toolRoot, options, out);
      };
  deps.cmdPublish                    = cmdPublish;
  deps.copyOutputDllsToDirectory     = copyOutputDllsToDirectory;
  deps.runSystemCommand              = runSystemCommand;
  deps.quotePath = [](const fs::path &p) { return quotePath(p); };
  return deps;
}

// ── build-nucleus ────────────────────────────────────────────────────────────

int cmdBuildMinimal(int argc, char *argv[]) {
  const std::string hostPlatform = currentHostPlatform();
  if (hostPlatform == "unsupported") {
    std::cerr << "Error: build-nucleus is unsupported on this host platform."
              << std::endl;
    return 1;
  }

  std::vector<std::string> args;
  args.reserve(argc > 2 ? static_cast<size_t>(argc - 2) : 0u);
  for (int i = 2; i < argc; ++i) { args.emplace_back(argv[i]); }

  neuron::MinimalBuildOptions options;
  std::string parseError;
  if (!neuron::parseMinimalBuildArgs(args, hostPlatform, &options, &parseError)) {
    std::cerr << "Error: " << parseError << std::endl;
    std::cerr << "Usage: neuron build-nucleus --platform <Windows|Linux|Mac>"
                 " [--compiler <path>] [--output <path>] [--extra-obj <path>]"
                 " [--verbose]" << std::endl;
    return 1;
  }

  const std::string compilerPath =
      options.compilerProvided ? options.compilerPath : resolveToolPath("g++");
  if (compilerPath.empty()) {
    std::cerr << "Error: failed to resolve compiler for build-nucleus." << std::endl;
    return 1;
  }

  fs::path outputPath = options.outputPath;
  if (!outputPath.is_absolute()) { outputPath = fs::absolute(outputPath); }

  std::error_code ec;
  fs::create_directories(outputPath.parent_path(), ec);
  if (ec) {
    std::cerr << "Error: failed to create output directory '"
              << outputPath.parent_path().string() << "': " << ec.message() << std::endl;
    return 1;
  }

  const fs::path manifestPath =
      (g_toolRoot / "runtime" / "minimal" / "sources.manifest").lexically_normal();
  std::vector<fs::path> sourceList;
  std::string manifestError;
  if (!readMinimalSourceManifest(manifestPath, &sourceList, &manifestError)) {
    std::cerr << "Error: " << manifestError << std::endl;
    return 1;
  }

  const fs::path objectRoot =
      (runtimeObjectDirectory() / "objects" / "minimal" / options.platformId)
          .lexically_normal();
  fs::create_directories(objectRoot, ec);
  if (ec) {
    std::cerr << "Error: failed to create minimal object cache at '"
              << objectRoot.string() << "': " << ec.message() << std::endl;
    return 1;
  }

  std::vector<fs::path> objectPaths;
  objectPaths.reserve(sourceList.size());
  for (const fs::path &relSrc : sourceList) {
    const fs::path srcPath = (g_toolRoot / relSrc).lexically_normal();
    if (!fs::exists(srcPath)) {
      std::cerr << "Error: minimal source not found: " << srcPath.string() << std::endl;
      return 1;
    }

    fs::path objPath = (objectRoot / relSrc).lexically_normal();
    objPath.replace_extension(".o");
    fs::create_directories(objPath.parent_path(), ec);
    if (ec) {
      std::cerr << "Error: failed to create object directory '"
                << objPath.parent_path().string() << "': " << ec.message() << std::endl;
      return 1;
    }

    const std::string compileCmd =
        neuron::buildMinimalCompileCommand(compilerPath, srcPath, objPath, g_toolRoot);
    if (options.verbose) { std::cout << "Compiling: " << compileCmd << std::endl; }
    if (runSystemCommand(compileCmd) != 0) {
      std::cerr << "Error: build-nucleus compile step failed for "
                << srcPath.string() << std::endl;
      return 1;
    }
    objectPaths.push_back(objPath);
  }

  if (!options.extraObjPath.empty()) {
    if (!fs::exists(options.extraObjPath)) {
      std::cerr << "Error: extra object file not found: "
                << options.extraObjPath.string() << std::endl;
      return 1;
    }
    objectPaths.push_back(options.extraObjPath);
  }

  const fs::path responseFilePath =
      (objectRoot / "minimal-link.rsp").lexically_normal();
  std::string responseError;
  if (!neuron::writeMinimalLinkResponseFile(responseFilePath, objectPaths,
                                            &responseError)) {
    std::cerr << "Error: " << responseError << std::endl;
    return 1;
  }

  const std::string linkCmd = neuron::buildMinimalLinkCommandWithResponseFile(
      compilerPath, responseFilePath, outputPath, options.platformId);
  if (options.verbose) { std::cout << "Linking: " << linkCmd << std::endl; }
  if (runSystemCommand(linkCmd) != 0) {
    std::cerr << "Error: single-file link failed for build-nucleus target '"
              << options.platformId
              << "'. Ensure the selected compiler has static runtime/libffi." << std::endl;
    return 1;
  }

  if (!fs::exists(outputPath)) {
    std::cerr << "Error: build-nucleus completed without output file: "
              << outputPath.string() << std::endl;
    return 1;
  }

  std::cout << "Nucleus runtime built: " << outputPath.string() << std::endl;
  std::cout << "Run containers with: " << quotePath(outputPath)
            << " <program.ncon>" << std::endl;
  return 0;
}

// ── build-product ────────────────────────────────────────────────────────────

int cmdBuildProduct(int argc, char *argv[]) {
  const std::string hostPlatform = currentHostPlatform();
  if (hostPlatform == "unsupported") {
    std::cerr << "Error: build-product unsupported on this host platform." << std::endl;
    return 1;
  }

  std::vector<std::string> args;
  args.reserve(argc > 2 ? static_cast<size_t>(argc - 2) : 0u);
  for (int i = 2; i < argc; ++i) { args.emplace_back(argv[i]); }

  neuron::ProductBuildOptions options;
  std::string parseError;
  if (!neuron::parseProductBuildArgs(args, hostPlatform, &options, &parseError)) {
    std::cerr << "Error: " << parseError << std::endl;
    std::cerr << "Usage: neuron build-product --platform <Windows|Linux|Mac> "
                 "[--compiler <path>] [--no-installer] [--no-updater] [--verbose]"
              << std::endl;
    return 1;
  }

  const fs::path projectRoot  = fs::current_path();
  const fs::path settingsPath = projectRoot / ".productsettings";
  if (!fs::exists(settingsPath)) {
    std::cerr << "Error: .productsettings not found in current directory.\n"
              << "Are you in a Neuron++ project directory?" << std::endl;
    return 1;
  }

  neuron::ProductSettings settings;
  std::vector<neuron::ProductSettingsError> settingErrors;
  if (!neuron::parseProductSettings(settingsPath, &settings, &settingErrors)) {
    std::cerr << "Failed to load .productsettings:\n";
    for (const auto &err : settingErrors) {
      std::cerr << "  Line " << err.line << ": " << err.message << "\n";
    }
    return 1;
  }

  const neuron::ProductBuildResult result =
      neuron::buildProduct(settings, options, projectRoot, g_toolRoot);
  if (!result.success) {
    std::cerr << "\nProduct build failed:\n";
    for (const auto &err : result.errors) {
      std::cerr << "  - " << err << "\n";
    }
    return 1;
  }

  return 0;
}

// ── build / run / release ────────────────────────────────────────────────────

int cmdBuild() {
  return neuron::cli::runBuildCommand(makeProjectDeps());
}

int cmdBuildTarget(int argc, char *argv[]) {
  return runWebBuildTargetInternal(argc, argv, nullptr);
}

int cmdRun() {
  return neuron::cli::runRunCommand(makeProjectDeps());
}

int cmdRunTarget(int argc, char *argv[]) {
  const bool targetProvided = hasTargetFlag(argc, argv);
  const std::string target = toLowerCopy(parseTargetArg(argc, argv));
  if (targetProvided && target.empty()) {
    std::cerr << "Missing value for --target" << std::endl;
    return 1;
  }
  if (target != "web") {
    std::cerr << "Unsupported target for run: "
              << (target.empty() ? "<none>" : target) << std::endl;
    return 1;
  }

  neuron::cli::WebBuildResult buildResult;
  const int buildRc = runWebBuildTargetInternal(argc, argv, &buildResult);
  if (buildRc != 0) {
    return buildRc;
  }

  neuron::cli::WebDevServerOptions serverOptions;
  serverOptions.rootDirectory = buildResult.outputDirectory;
  serverOptions.port = buildResult.devServerPort;
  serverOptions.openBrowser = !hasArg(argc, argv, "--no-open");

  std::string serverError;
  const int serverRc = neuron::cli::runWebDevServer(serverOptions, &serverError);
  if (serverRc != 0) {
    std::cerr << "Web dev server failed: " << serverError << std::endl;
  }
  return serverRc;
}

int cmdRelease() {
  return neuron::cli::runReleaseCommand(makeProjectDeps());
}

