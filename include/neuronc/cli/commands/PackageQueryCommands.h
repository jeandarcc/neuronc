#pragma once

#include "neuronc/cli/PackageManager.h"

#include <filesystem>
#include <functional>
#include <string>

namespace neuron::cli {

struct PackageQueryCommandDependencies {
  std::filesystem::path toolRoot;
  std::filesystem::path currentWorkingDirectory;
  std::function<bool(const std::string &, const std::string &, bool,
                     neuron::InstalledPackageDetails *, std::string *)>
      inspectInstalledPackage;
};

int runSettingsOfCommand(const PackageQueryCommandDependencies &deps,
                         const std::string &target);
int runDependenciesOfCommand(const PackageQueryCommandDependencies &deps,
                             const std::string &target);

} // namespace neuron::cli
