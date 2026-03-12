#pragma once

#include "neuronc/cli/PackageLock.h"
#include "neuronc/cli/ProjectConfig.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace neuron {

struct PackageInstallOptions {
  bool global = false;
  std::string version;
  std::string tag;
  std::string commit;
};

struct InstalledPackageRecord {
  std::string name;
  std::string github;
  std::string resolvedTag;
  std::string packageVersion;
  bool global = false;
  std::vector<std::string> exportedModules;
};

struct PackagePublishArtifact {
  std::string manifestPath;
  std::string snapshotDirectory;
};

struct PackageRecord {
  std::string name;
  std::string github;
  std::string resolvedTag;
  std::string description;
};

struct InstalledPackageDetails {
  std::filesystem::path packageRoot;
  LockedPackage locked;
  ProjectConfig config;
  bool global = false;
};

class PackageManager {
public:
  static bool loadProjectConfig(const std::string &projectRoot,
                                ProjectConfig *config,
                                std::vector<std::string> *outErrors);
  static bool writeProjectConfig(const std::string &projectRoot,
                                 const ProjectConfig &config,
                                 std::string *outMessage);
  static bool loadPackageLock(const std::string &projectRoot, PackageLock *lock,
                              std::vector<std::string> *outErrors);
  static bool writePackageLock(const std::string &projectRoot,
                               const PackageLock &lock,
                               std::string *outMessage);

  static std::vector<InstalledPackageRecord> listInstalledPackages(
      const std::string &projectRoot, bool includeGlobal);
  static bool inspectInstalledPackage(const std::string &projectRoot,
                                      const std::string &packageSource,
                                      bool includeGlobal,
                                      InstalledPackageDetails *outDetails,
                                      std::string *outMessage);

  static bool addDependency(const std::string &projectRoot,
                            const std::string &packageSpec,
                            const PackageInstallOptions &options,
                            std::string *outMessage);
  static bool installDependencies(const std::string &projectRoot,
                                  std::string *outMessage);
  static bool removeDependency(const std::string &projectRoot,
                               const std::string &packageName,
                               bool removeGlobal,
                               std::string *outMessage);
  static bool updateDependency(const std::string &projectRoot,
                               const std::optional<std::string> &packageName,
                               std::string *outMessage);
  static bool autoAddMissingModule(const std::string &projectRoot,
                                   const std::string &moduleName,
                                   std::string *outMessage);
  static bool publishProject(const std::string &projectRoot,
                             std::string *outArtifactPath,
                             std::string *outMessage);
};

} // namespace neuron
