#pragma once

#include "neuronc/cli/PackageManager.h"
#include "neuronc/diagnostics/DiagnosticLocale.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace neuron::cli {

struct AppContext {
  std::filesystem::path toolRoot;
  std::filesystem::path runtimeObjectDir;
  std::string toolchainBinDir;
  bool traceErrors = false;
  bool colorDiagnostics = false;
  bool bypassRules = false;
  neuron::diagnostics::ResolvedLanguage diagnosticLanguage;
  std::vector<std::string> supportedDiagnosticLocales;
};

using FileArgParser = std::function<std::optional<std::string>(
    int argc, char *argv[], int startIndex, const std::string &usageLine)>;
using PrintUsageFn = std::function<void()>;
using ReplFn = std::function<int()>;
using IsInteractiveInputFn = std::function<bool()>;
using PackagesFn = std::function<int()>;
using NewProjectFn = std::function<int(const std::string &name, bool library)>;
using InstallPackageFn = std::function<int()>;
using AddPackageFn = std::function<int(
    const std::string &packageName,
    const neuron::PackageInstallOptions &options)>;
using RemovePackageFn =
    std::function<int(const std::string &packageName, bool removeGlobal)>;
using UpdatePackageFn =
    std::function<int(const std::optional<std::string> &packageName)>;
using PublishFn = std::function<int()>;
using PackageQueryFn = std::function<int(const std::string &target)>;
using LexFn = std::function<int(const std::string &filepath)>;
using ParseFn = std::function<int(const std::string &filepath)>;
using NirFn = std::function<int(const std::string &filepath)>;
using BuildFn = std::function<int()>;
using BuildTargetFn = std::function<int(int argc, char *argv[])>;
using BuildMinimalFn = std::function<int(int argc, char *argv[])>;
using BuildProductFn = std::function<int(int argc, char *argv[])>;
using CompileFn = std::function<int(const std::string &filepath)>;
using RunFn = std::function<int()>;
using RunTargetFn = std::function<int(int argc, char *argv[])>;
using ReleaseFn = std::function<int()>;
using NconCliFn =
    std::function<int(int argc, char *argv[], const char *invokerPath)>;
using SurgeonFn = std::function<int(int argc, char *argv[])>;
using LoadUserLanguageFn = std::function<std::string()>;
using SaveUserLanguageFn =
    std::function<bool(const std::string &languageCode)>;
using ResolveLanguageFn = std::function<neuron::diagnostics::ResolvedLanguage(
    const std::string &requestedLanguage,
    const std::vector<std::string> &supportedLocales)>;

struct AppServices {
  FileArgParser parseFileArgWithTraceFlags;
  PrintUsageFn printUsage;
  ReplFn cmdRepl;
  IsInteractiveInputFn isInteractiveInput;
  PackagesFn cmdPackages;
  NewProjectFn cmdNew;
  InstallPackageFn cmdInstall;
  AddPackageFn cmdAdd;
  RemovePackageFn cmdRemove;
  UpdatePackageFn cmdUpdate;
  PublishFn cmdPublish;
  PackageQueryFn cmdSettingsOf;
  PackageQueryFn cmdDependenciesOf;
  LexFn cmdLex;
  ParseFn cmdParse;
  NirFn cmdNir;
  BuildFn cmdBuild;
  BuildTargetFn cmdBuildTarget;
  BuildMinimalFn cmdBuildMinimal;
  BuildProductFn cmdBuildProduct;
  CompileFn cmdCompile;
  RunFn cmdRun;
  RunTargetFn cmdRunTarget;
  ReleaseFn cmdRelease;
  NconCliFn runNconCli;
  SurgeonFn cmdSurgeon;
  LoadUserLanguageFn loadUserLanguage;
  SaveUserLanguageFn saveUserLanguage;
  ResolveLanguageFn resolveLanguage;
};

}  // namespace neuron::cli
