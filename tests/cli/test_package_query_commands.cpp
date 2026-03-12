#include "neuronc/cli/commands/PackageQueryCommands.h"
#include "neuronc/cli/PackageLock.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

using namespace neuron;
using namespace neuron::cli;
namespace fs = std::filesystem;

namespace {

void removePackageQueryTempDir(const fs::path &path) {
  std::error_code ec;
  if (!fs::exists(path, ec)) {
    return;
  }
  fs::remove_all(path, ec);
}

struct ScopedTempDir {
  explicit ScopedTempDir(const std::string &name)
      : path(fs::current_path() / name) {
    removePackageQueryTempDir(path);
  }

  ~ScopedTempDir() { removePackageQueryTempDir(path); }

  fs::path path;
};

struct StreamCapture {
  explicit StreamCapture(std::ostream &stream)
      : m_stream(stream), m_old(stream.rdbuf(m_buffer.rdbuf())) {}

  ~StreamCapture() { m_stream.rdbuf(m_old); }

  std::string str() const { return m_buffer.str(); }

private:
  std::ostream &m_stream;
  std::streambuf *m_old;
  std::ostringstream m_buffer;
};

fs::path packageQueryRepoRootPath() {
  return fs::path(__FILE__).parent_path().parent_path().parent_path();
}

void writePackageQueryTextFile(const fs::path &path, const std::string &text) {
  if (!path.parent_path().empty()) {
    fs::create_directories(path.parent_path());
  }
  std::ofstream out(path, std::ios::binary);
  out << text;
}

bool writePackageQueryInstalledMetadata(const fs::path &packageRoot,
                                        const LockedPackage &locked) {
  PackageLock lock;
  lock.packages[locked.name] = locked;
  return writePackageLockFile((packageRoot / ".neuron-package.lock").string(),
                              lock, nullptr);
}

} // namespace

TEST(PackageQueryCommandPrintsBuiltinSettingsSection) {
  PackageQueryCommandDependencies deps;
  deps.toolRoot = packageQueryRepoRootPath();
  deps.currentWorkingDirectory = packageQueryRepoRootPath();

  StreamCapture stdoutCapture(std::cout);
  StreamCapture stderrCapture(std::cerr);
  ASSERT_EQ(runSettingsOfCommand(deps, "IO"), 0);
  ASSERT_TRUE(stderrCapture.str().empty());
  ASSERT_TRUE(stdoutCapture.str().find("[IO]") != std::string::npos);
  ASSERT_TRUE(stdoutCapture.str().find("ENUM_INPUT_NEXT_KEY") != std::string::npos);
  ASSERT_TRUE(stdoutCapture.str().find("[Graphics]") == std::string::npos);
  return true;
}

TEST(PackageQueryCommandPrintsInstalledPackageSettingsByName) {
  ScopedTempDir project("tmp_package_query_settings");
  const fs::path packageRoot = project.path / "modules" / "tensorpkg";
  fs::create_directories(packageRoot / "src");

  LockedPackage locked;
  locked.name = "tensorpkg";
  locked.github = "acme/tensorpkg";
  locked.packageVersion = "1.2.0";
  ASSERT_TRUE(writePackageQueryInstalledMetadata(packageRoot, locked));
  writePackageQueryTextFile(
      packageRoot / "neuron.toml",
      "[project]\n"
      "name = \"tensorpkg\"\n"
      "version = \"1.2.0\"\n\n"
      "[package]\n"
      "kind = \"library\"\n"
      "description = \"Tensor package\"\n"
      "repository = \"https://github.com/acme/tensorpkg\"\n"
      "license = \"MIT\"\n");
  writePackageQueryTextFile(packageRoot / ".modulesettings",
                            "[TensorPkg]\n"
                            "BLOCK_SIZE = 256;\n");

  PackageQueryCommandDependencies deps;
  deps.toolRoot = packageQueryRepoRootPath();
  deps.currentWorkingDirectory = project.path;

  StreamCapture stdoutCapture(std::cout);
  StreamCapture stderrCapture(std::cerr);
  ASSERT_EQ(runSettingsOfCommand(deps, "tensorpkg"), 0);
  ASSERT_TRUE(stderrCapture.str().empty());
  ASSERT_TRUE(stdoutCapture.str().find("Package settings for 'tensorpkg'") !=
              std::string::npos);
  ASSERT_TRUE(stdoutCapture.str().find("[TensorPkg]") != std::string::npos);
  ASSERT_TRUE(stdoutCapture.str().find("BLOCK_SIZE = 256;") != std::string::npos);
  return true;
}

TEST(PackageQueryCommandPrintsInstalledPackageDependenciesByName) {
  ScopedTempDir project("tmp_package_query_dependencies");
  const fs::path packageRoot = project.path / "modules" / "tensorpkg";
  fs::create_directories(packageRoot / "src");

  LockedPackage locked;
  locked.name = "tensorpkg";
  locked.github = "acme/tensorpkg";
  locked.packageVersion = "1.2.0";
  locked.transitiveDependencies = {"coremath", "gpukit"};
  ASSERT_TRUE(writePackageQueryInstalledMetadata(packageRoot, locked));
  writePackageQueryTextFile(
      packageRoot / "neuron.toml",
      "[project]\n"
      "name = \"tensorpkg\"\n"
      "version = \"1.2.0\"\n\n"
      "[package]\n"
      "kind = \"library\"\n"
      "description = \"Tensor package\"\n"
      "repository = \"https://github.com/acme/tensorpkg\"\n"
      "license = \"MIT\"\n\n"
      "[dependencies]\n"
      "coremath = { github = \"acme/coremath\", version = \"^2.0.0\" }\n"
      "gpukit = { github = \"acme/gpukit\", tag = \"v1.1.0\" }\n");

  PackageQueryCommandDependencies deps;
  deps.toolRoot = packageQueryRepoRootPath();
  deps.currentWorkingDirectory = project.path;

  StreamCapture stdoutCapture(std::cout);
  StreamCapture stderrCapture(std::cerr);
  ASSERT_EQ(runDependenciesOfCommand(deps, "tensorpkg"), 0);
  ASSERT_TRUE(stderrCapture.str().empty());
  ASSERT_TRUE(stdoutCapture.str().find("Dependencies for package 'tensorpkg'") !=
              std::string::npos);
  ASSERT_TRUE(stdoutCapture.str().find("coremath: github = \"acme/coremath\", version = \"^2.0.0\"") !=
              std::string::npos);
  ASSERT_TRUE(stdoutCapture.str().find("gpukit: github = \"acme/gpukit\", tag = \"v1.1.0\"") !=
              std::string::npos);
  ASSERT_TRUE(stdoutCapture.str().find("Resolved child packages:") !=
              std::string::npos);
  return true;
}
