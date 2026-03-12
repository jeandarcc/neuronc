// Package CLI smoke tests - included from tests/test_main.cpp
#include "neuronc/cli/CommandDispatcher.h"

#include <optional>
#include <string>
#include <vector>

using namespace neuron;

namespace {

std::vector<char *> toArgv(std::vector<std::string> *args) {
  std::vector<char *> argv;
  argv.reserve(args->size());
  for (std::string &arg : *args) {
    argv.push_back(arg.data());
  }
  return argv;
}

neuron::cli::AppServices makeDefaultServices() {
  neuron::cli::AppServices services;
  services.parseFileArgWithTraceFlags =
      [](int, char *[], int, const std::string &) -> std::optional<std::string> {
    return std::nullopt;
  };
  services.printUsage = []() {};
  services.cmdPackages = []() { return 0; };
  services.cmdNew = [](const std::string &, bool) { return 0; };
  services.cmdInstall = []() { return 0; };
  services.cmdAdd = [](const std::string &,
                       const PackageInstallOptions &) { return 0; };
  services.cmdRemove = [](const std::string &, bool) { return 0; };
  services.cmdUpdate = [](const std::optional<std::string> &) { return 0; };
  services.cmdPublish = []() { return 0; };
  services.cmdSettingsOf = [](const std::string &) { return 0; };
  services.cmdDependenciesOf = [](const std::string &) { return 0; };
  services.cmdLex = [](const std::string &) { return 0; };
  services.cmdParse = [](const std::string &) { return 0; };
  services.cmdNir = [](const std::string &) { return 0; };
  services.cmdBuild = []() { return 0; };
  services.cmdBuildTarget = [](int, char *[]) { return 0; };
  services.cmdBuildMinimal = [](int, char *[]) { return 0; };
  services.cmdBuildProduct = [](int, char *[]) { return 0; };
  services.cmdCompile = [](const std::string &) { return 0; };
  services.cmdRun = []() { return 0; };
  services.cmdRunTarget = [](int, char *[]) { return 0; };
  services.cmdRelease = []() { return 0; };
  services.runNconCli = [](int, char *[], const char *) { return 0; };
  return services;
}

} // namespace

TEST(CommandDispatcherParsesNewLibAndInstall) {
  neuron::cli::AppContext context;
  bool sawLibrary = false;
  bool sawInstall = false;
  bool newArgsValid = true;

  neuron::cli::AppServices services = makeDefaultServices();
  services.cmdNew = [&](const std::string &name, bool library) {
    if (name != "demo-lib" || !library) {
      newArgsValid = false;
      return 1;
    }
    sawLibrary = true;
    return 0;
  };
  services.cmdInstall = [&]() {
    sawInstall = true;
    return 0;
  };

  std::vector<std::string> newArgs = {"neuron", "new", "demo-lib", "--lib"};
  std::vector<char *> newArgv = toArgv(&newArgs);
  ASSERT_EQ(neuron::cli::dispatchCommand(context, services,
                                         static_cast<int>(newArgv.size()),
                                         newArgv.data()),
            0);
  ASSERT_TRUE(sawLibrary);
  ASSERT_TRUE(newArgsValid);

  std::vector<std::string> installArgs = {"neuron", "install"};
  std::vector<char *> installArgv = toArgv(&installArgs);
  ASSERT_EQ(neuron::cli::dispatchCommand(context, services,
                                         static_cast<int>(installArgv.size()),
                                         installArgv.data()),
            0);
  ASSERT_TRUE(sawInstall);
  return true;
}

TEST(CommandDispatcherParsesAddAndRemoveFlags) {
  neuron::cli::AppContext context;
  std::optional<PackageInstallOptions> capturedOptions;
  std::string capturedSpec;
  std::string removedPackage;
  bool removedGlobal = false;

  neuron::cli::AppServices services = makeDefaultServices();
  services.cmdAdd = [&](const std::string &packageSpec,
                        const PackageInstallOptions &options) {
    capturedSpec = packageSpec;
    capturedOptions = options;
    return 0;
  };
  services.cmdRemove = [&](const std::string &packageName, bool removeGlobal) {
    removedPackage = packageName;
    removedGlobal = removeGlobal;
    return 0;
  };

  std::vector<std::string> addArgs = {"neuron",   "add",         "acme/tensorpkg",
                                      "--version", "^1.2.0",     "--tag=v1.2.3",
                                      "--commit",  "abc123",     "--global"};
  std::vector<char *> addArgv = toArgv(&addArgs);
  ASSERT_EQ(neuron::cli::dispatchCommand(context, services,
                                         static_cast<int>(addArgv.size()),
                                         addArgv.data()),
            0);
  ASSERT_EQ(capturedSpec, "acme/tensorpkg");
  ASSERT_TRUE(capturedOptions.has_value());
  ASSERT_EQ(capturedOptions->version, "^1.2.0");
  ASSERT_EQ(capturedOptions->tag, "v1.2.3");
  ASSERT_EQ(capturedOptions->commit, "abc123");
  ASSERT_TRUE(capturedOptions->global);

  std::vector<std::string> removeArgs = {"neuron", "remove", "tensorpkg", "--global"};
  std::vector<char *> removeArgv = toArgv(&removeArgs);
  ASSERT_EQ(neuron::cli::dispatchCommand(context, services,
                                         static_cast<int>(removeArgv.size()),
                                         removeArgv.data()),
            0);
  ASSERT_EQ(removedPackage, "tensorpkg");
  ASSERT_TRUE(removedGlobal);
  return true;
}

TEST(CommandDispatcherParsesSettingsAndDependenciesQueries) {
  neuron::cli::AppContext context;
  std::string settingsTarget;
  std::string dependenciesTarget;

  neuron::cli::AppServices services = makeDefaultServices();
  services.cmdSettingsOf = [&](const std::string &target) {
    settingsTarget = target;
    return 0;
  };
  services.cmdDependenciesOf = [&](const std::string &target) {
    dependenciesTarget = target;
    return 0;
  };

  std::vector<std::string> settingsArgs = {"neuron", "settings-of", "IO"};
  std::vector<char *> settingsArgv = toArgv(&settingsArgs);
  ASSERT_EQ(neuron::cli::dispatchCommand(context, services,
                                         static_cast<int>(settingsArgv.size()),
                                         settingsArgv.data()),
            0);
  ASSERT_EQ(settingsTarget, "IO");

  std::vector<std::string> dependenciesArgs = {
      "neuron", "dependencies-of", "acme/tensorpkg"};
  std::vector<char *> dependenciesArgv = toArgv(&dependenciesArgs);
  ASSERT_EQ(neuron::cli::dispatchCommand(
                context, services, static_cast<int>(dependenciesArgv.size()),
                dependenciesArgv.data()),
            0);
  ASSERT_EQ(dependenciesTarget, "acme/tensorpkg");
  return true;
}
