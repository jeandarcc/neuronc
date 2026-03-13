// main.cpp â€” Neuron compiler entry point.
//
// This file ONLY performs the following:
//   1. Initializes global state (trace, color, toolRoot, toolchain)
//   2. Fills AppServices and AppContext objects by directing them to modules
//   3. Passes to CommandDispatcher
//   4. Returns the exit code
//
// To add a new command, go to CommandHandlers.h + appropriate Cmd_*.cpp,
// you do not need to touch this file.
#include "main/AppGlobals.h"
#include "main/BuildSupport.h"
#include "main/CommandHandlers.h"
#include "main/DiagnosticEngine.h"
#include "main/ProjectHelpers.h"
#include "main/RuntimePaths.h"
#include "main/SettingsLoader.h"
#include "main/ToolchainUtils.h"
#include "main/UserProfileSettings.h"

#include "neuronc/cli/App.h"
#include "neuronc/cli/CommandDispatcher.h"
#include "neuronc/cli/repl/ReplConsoleReader.h"
#include "neuronc/diagnostics/DiagnosticLocale.h"
#include "neuronc/ncon/NconCLI.h"

#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
  // â”€â”€ Environment initialization â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  initializeTraceErrorsFromEnv();
  initializeDiagnosticColorFromEnv();

  std::error_code ec;
  const fs::path exePath = fs::weakly_canonical(fs::path(argv[0]), ec);
  if (!ec) {
    const auto detectedToolRoot = findToolRootFromExecutable(exePath);
    if (detectedToolRoot.has_value()) {
      g_toolRoot = *detectedToolRoot;
    }
  }
  if (!hasRuntimeSourcesAt(g_toolRoot) &&
      hasRuntimeSourcesAt(fs::current_path())) {
    g_toolRoot = fs::current_path();
  }
  initializeToolchainBinDir();

  // â”€â”€ Application context â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  neuron::cli::AppContext appContext;
  appContext.toolRoot = g_toolRoot;
  appContext.runtimeObjectDir = g_runtimeObjectDir;
  appContext.toolchainBinDir = g_toolchainBinDir;
  appContext.traceErrors = g_traceErrors;
  appContext.colorDiagnostics = g_colorDiagnostics;
  appContext.bypassRules = g_bypassRules;
  appContext.supportedDiagnosticLocales =
      neuron::diagnostics::loadSupportedDiagnosticLocales(g_toolRoot);
  const auto userSettings = neuron::loadUserProfileSettings();
  appContext.diagnosticLanguage = neuron::diagnostics::resolveLanguagePreference(
      userSettings.has_value() ? userSettings->language : "auto",
      appContext.supportedDiagnosticLocales);

  // â”€â”€ Service bindings â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  neuron::cli::AppServices services;
  services.parseFileArgWithTraceFlags = parseFileArgWithTraceFlags;
  services.printUsage = printUsage;
  services.cmdRepl = cmdRepl;
  services.isInteractiveInput = neuron::cli::ReplConsoleReader::isInteractiveInput;
  services.cmdPackages = cmdPackages;
  services.cmdNew = cmdNew;
  services.cmdInstall = cmdInstall;
  services.cmdAdd = cmdAdd;
  services.cmdRemove = cmdRemove;
  services.cmdUpdate = cmdUpdate;
  services.cmdPublish = []() { return cmdPublish(); };
  services.cmdSettingsOf = cmdSettingsOf;
  services.cmdDependenciesOf = cmdDependenciesOf;
  services.cmdLex = cmdLex;
  services.cmdParse = cmdParse;
  services.cmdNir = cmdNir;
  services.cmdBuild = cmdBuild;
  services.cmdBuildTarget = cmdBuildTarget;
  services.cmdBuildMinimal = cmdBuildMinimal;
  services.cmdBuildProduct = cmdBuildProduct;
  services.cmdCompile = [](const std::string &filepath) {
    return cmdCompile(filepath, nullptr);
  };
  services.cmdRun = cmdRun;
  services.cmdRunTarget = cmdRunTarget;
  services.cmdRelease = cmdRelease;
  services.cmdSurgeon = cmdSurgeon;
  services.runNconCli = [](int nestedArgc, char *nestedArgv[],
                           const char *invokerPath) {
    return neuron::ncon::runCli(nestedArgc, nestedArgv, invokerPath);
  };
  services.loadUserLanguage = []() {
    const auto settings = neuron::loadUserProfileSettings();
    return settings.has_value() ? settings->language : std::string("auto");
  };
  services.saveUserLanguage = [](const std::string &languageCode) {
    neuron::UserProfileSettings settings;
    settings.language = languageCode;
    return neuron::saveUserProfileSettings(settings);
  };
  services.resolveLanguage = [](const std::string &requestedLanguage,
                                const std::vector<std::string> &supportedLocales) {
    return neuron::diagnostics::resolveLanguagePreference(requestedLanguage,
                                                          supportedLocales);
  };

  // â”€â”€ Command dispatching â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  const int result =
      neuron::cli::dispatchCommand(appContext, services, argc, argv);

  // Dispatcher might have updated the context â€” synchronize global state.
  g_toolRoot = appContext.toolRoot;
  g_runtimeObjectDir = appContext.runtimeObjectDir;
  g_toolchainBinDir = appContext.toolchainBinDir;
  g_traceErrors = appContext.traceErrors;
  g_colorDiagnostics = appContext.colorDiagnostics;
  g_bypassRules = appContext.bypassRules;

  return result;
}
