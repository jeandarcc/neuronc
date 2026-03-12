// Cmd_Package.cpp — Paket yönetimi ve proje oluşturma komutları.
//
// Bu dosya şunları içerir:
//   cmdNew, cmdPackages, cmdAdd, cmdRemove, cmdUpdate, cmdPublish
//
// Yeni bir paket yönetimi komutu eklemek için buraya implementasyon yaz
// ve CommandHandlers.h'a prototipi ekle.
#include "CommandHandlers.h"
#include "AppGlobals.h"

#include "neuronc/cli/PackageManager.h"
#include "neuronc/cli/commands/PackageQueryCommands.h"
#include "neuronc/cli/ProjectGenerator.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace fs = std::filesystem;

// ── Proje oluşturma ──────────────────────────────────────────────────────────

int cmdNew(const std::string &name, bool library) {
  return neuron::ProjectGenerator::createProject(name, g_toolRoot, library) ? 0
                                                                             : 1;
}

// ── Paket yönetimi ───────────────────────────────────────────────────────────

int cmdPackages() {
  const auto packages =
      neuron::PackageManager::listInstalledPackages(fs::current_path().string(),
                                                    true);
  std::cout << "Installed Neuron packages:" << std::endl;
  for (const auto &pkg : packages) {
    std::cout << "  - " << pkg.name << "@" << pkg.packageVersion;
    if (!pkg.resolvedTag.empty()) {
      std::cout << " [" << pkg.resolvedTag << "]";
    }
    std::cout << (pkg.global ? " (global)" : " (local)");
    if (!pkg.github.empty()) {
      std::cout << " <" << pkg.github << ">";
    }
    std::cout << std::endl;
  }
  return 0;
}

int cmdInstall() {
  std::string message;
  if (!neuron::PackageManager::installDependencies(fs::current_path().string(),
                                                   &message)) {
    std::cerr << "Error: " << message << std::endl;
    return 1;
  }
  std::cout << message << std::endl;
  return 0;
}

int cmdAdd(const std::string &packageName,
           const neuron::PackageInstallOptions &options) {
  std::string message;
  if (!neuron::PackageManager::addDependency(fs::current_path().string(),
                                             packageName, options, &message)) {
    std::cerr << "Error: " << message << std::endl;
    return 1;
  }
  std::cout << message << std::endl;
  return 0;
}

int cmdRemove(const std::string &packageName, bool removeGlobal) {
  std::string message;
  if (!neuron::PackageManager::removeDependency(
          fs::current_path().string(), packageName, removeGlobal, &message)) {
    std::cerr << "Error: " << message << std::endl;
    return 1;
  }
  std::cout << message << std::endl;
  return 0;
}

int cmdUpdate(const std::optional<std::string> &packageName) {
  std::string message;
  if (!neuron::PackageManager::updateDependency(
          fs::current_path().string(), packageName, &message)) {
    std::cerr << "Error: " << message << std::endl;
    return 1;
  }
  std::cout << message << std::endl;
  return 0;
}

int cmdPublish(std::string *outArtifactPath) {
  std::string message;
  std::string artifactPath;
  if (!neuron::PackageManager::publishProject(
          fs::current_path().string(), &artifactPath, &message)) {
    std::cerr << "Error: " << message << std::endl;
    return 1;
  }
  std::cout << message << std::endl;
  if (outArtifactPath != nullptr) {
    *outArtifactPath = artifactPath;
  }
  return 0;
}

int cmdSettingsOf(const std::string &target) {
  neuron::cli::PackageQueryCommandDependencies deps;
  deps.toolRoot = g_toolRoot;
  deps.currentWorkingDirectory = fs::current_path();
  deps.inspectInstalledPackage = neuron::PackageManager::inspectInstalledPackage;
  return neuron::cli::runSettingsOfCommand(deps, target);
}

int cmdDependenciesOf(const std::string &target) {
  neuron::cli::PackageQueryCommandDependencies deps;
  deps.toolRoot = g_toolRoot;
  deps.currentWorkingDirectory = fs::current_path();
  deps.inspectInstalledPackage = neuron::PackageManager::inspectInstalledPackage;
  return neuron::cli::runDependenciesOfCommand(deps, target);
}

