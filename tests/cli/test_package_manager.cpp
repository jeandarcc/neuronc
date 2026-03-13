// Package manager tests - included from tests/test_main.cpp
#include "neuronc/cli/PackageManager.h"
#include "neuronc/cli/ProjectConfig.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

using namespace neuron;
namespace fs = std::filesystem;

static void removeAllNoThrow(const fs::path &path) {
  std::error_code ec;
  if (!fs::exists(path, ec)) {
    return;
  }
  for (fs::recursive_directory_iterator it(path, ec), end; it != end;
       it.increment(ec)) {
    if (ec) {
      break;
    }
    fs::permissions(it->path(), fs::perms::owner_all, fs::perm_options::add, ec);
  }
  ec.clear();
  fs::permissions(path, fs::perms::owner_all, fs::perm_options::add, ec);
  ec.clear();
  fs::remove_all(path, ec);
}

static fs::path createTempProjectDir(const std::string &name) {
  fs::path dir = fs::current_path() / name;
  removeAllNoThrow(dir);
  fs::create_directories(dir / "src");
  fs::create_directories(dir / "build");

  std::ofstream toml(dir / "neuron.toml");
  toml << "[project]\n";
  toml << "name = \"pkg_test\"\n";
  toml << "version = \"0.1.0\"\n\n";
  toml << "[package]\n";
  toml << "kind = \"library\"\n";
  toml << "description = \"Test package\"\n";
  toml << "repository = \"https://github.com/acme/pkg_test\"\n";
  toml << "license = \"MIT\"\n";
  toml << "source_dir = \"src\"\n\n";
  toml << "[build]\n";
  toml << "main = \"src/Main.nr\"\n";
  toml << "build_dir = \"build\"\n\n";
  toml << "optimize = \"O1\"\n";
  toml << "emit_ir = \"optimized\"\n";
  toml << "target_cpu = \"native\"\n\n";
  toml << "[dependencies]\n";
  toml.close();

  std::ofstream mainFile(dir / "src/Main.nr");
  mainFile << "Init is method() { Print(\"ok\"); }\n";
  mainFile.close();

  std::ofstream readme(dir / "README.md");
  readme << "# pkg_test\n";
  readme.close();
  return dir;
}

static bool runGit(const std::string &command) {
  return std::system(command.c_str()) == 0;
}

static fs::path createPackageRepo(const std::string &name) {
  const fs::path dir = fs::current_path() / name;
  removeAllNoThrow(dir);
  fs::create_directories(dir / "src");

  {
    std::ofstream toml(dir / "neuron.toml");
    toml << "[project]\n";
    toml << "name = \"tensorpkg\"\n";
    toml << "version = \"1.0.0\"\n\n";
    toml << "[package]\n";
    toml << "kind = \"library\"\n";
    toml << "description = \"Tensor package\"\n";
    toml << "repository = \"https://github.com/acme/tensorpkg\"\n";
    toml << "license = \"MIT\"\n";
    toml << "source_dir = \"src\"\n\n";
    toml << "[dependencies]\n";
  }
  {
    std::ofstream source(dir / "src/TensorPkg.nr");
    source << "TensorValue is method() -> string { return \"tensor\"; }\n";
  }
  {
    std::ofstream readme(dir / "README.md");
    readme << "# tensorpkg\n";
  }

  const std::string quoted = "\"" + dir.string() + "\"";
  if (!runGit("git init " + quoted) ||
      !runGit("git -C " + quoted + " config user.email test@example.com") ||
      !runGit("git -C " + quoted + " config user.name TestUser") ||
      !runGit("git -C " + quoted + " add .") ||
      !runGit("git -C " + quoted + " commit -m init") ||
      !runGit("git -C " + quoted + " tag v1.0.0")) {
    removeAllNoThrow(dir);
    return {};
  }

  return dir;
}

TEST(PackageManagerAddRemoveAndPublish) {
  const fs::path dir = createTempProjectDir("tmp_pkg_manager_test");
  const fs::path repo = createPackageRepo("tmp_pkg_repo");
  ASSERT_TRUE(!repo.empty());

  PackageInstallOptions options;
  options.version = "^1.0.0";
  std::string message;
  ASSERT_TRUE(PackageManager::addDependency(dir.string(), repo.string(), options,
                                            &message));
  ASSERT_TRUE(fs::exists(dir / "modules" / "tensorpkg" / "src" /
                         "TensorPkg.nr"));
  ASSERT_TRUE(fs::exists(dir / "neuron.lock"));

  ProjectConfig cfg;
  ProjectConfigParser parser;
  ASSERT_TRUE(parser.parseFile((dir / "neuron.toml").string(), &cfg));
  ASSERT_TRUE(cfg.dependencies.count("tensorpkg") == 1u);
  ASSERT_EQ(cfg.dependencies["tensorpkg"].version, "^1.0.0");
  ASSERT_TRUE(!cfg.dependencies["tensorpkg"].github.empty());
  ASSERT_EQ(cfg.optimizeLevel, BuildOptimizeLevel::O1);
  ASSERT_EQ(cfg.emitIR, BuildEmitIR::Optimized);
  ASSERT_EQ(cfg.targetCPU, BuildTargetCPU::Native);
  ASSERT_EQ(cfg.tensorProfile, BuildTensorProfile::Balanced);
  ASSERT_TRUE(cfg.tensorAutotune);
  ASSERT_EQ(cfg.tensorKernelCache, "build/.neuron_cache/tensor/");

  ASSERT_TRUE(PackageManager::updateDependency(
      dir.string(), std::optional<std::string>("tensorpkg"), &message));
  ASSERT_TRUE(PackageManager::removeDependency(dir.string(), "tensorpkg", false,
                                               &message));

  ASSERT_TRUE(parser.parseFile((dir / "neuron.toml").string(), &cfg));
  ASSERT_TRUE(cfg.dependencies.count("tensorpkg") == 0u);
  ASSERT_EQ(cfg.optimizeLevel, BuildOptimizeLevel::O1);
  ASSERT_EQ(cfg.tensorProfile, BuildTensorProfile::Balanced);
  ASSERT_TRUE(cfg.tensorAutotune);

  std::string artifact;
  ASSERT_TRUE(PackageManager::publishProject(dir.string(), &artifact, &message));
  ASSERT_TRUE(!artifact.empty());
  ASSERT_TRUE(fs::exists(artifact));

  removeAllNoThrow(dir);
  removeAllNoThrow(repo);
  return true;
}

TEST(PackageManagerRejectsUnknownPackage) {
  const fs::path dir = createTempProjectDir("tmp_pkg_manager_test_unknown");
  PackageInstallOptions options;
  std::string message;
  ASSERT_FALSE(PackageManager::addDependency(dir.string(), "missing_pkg", options,
                                             &message));
  ASSERT_FALSE(message.empty());
  removeAllNoThrow(dir);
  return true;
}
