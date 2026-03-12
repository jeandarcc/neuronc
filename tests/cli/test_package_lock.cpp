// Package lock tests - included from tests/test_main.cpp
#include "neuronc/cli/PackageLock.h"

#include <filesystem>

using namespace neuron;
namespace fs = std::filesystem;

TEST(PackageLockRoundTrip) {
  const fs::path path = fs::current_path() / "tmp_neuron_lock_test.toml";
  fs::remove(path);

  PackageLock lock;
  LockedPackage pkg;
  pkg.name = "tensorpkg";
  pkg.github = "acme/tensorpkg";
  pkg.versionConstraint = "^1.0.0";
  pkg.resolvedTag = "v1.0.0";
  pkg.resolvedCommit = "abc123";
  pkg.packageVersion = "1.0.0";
  pkg.contentHash = "deadbeef";
  pkg.sourceDir = "src";
  pkg.exportedModules = {"TensorPkg", "TensorMath"};
  pkg.transitiveDependencies = {"coremath"};
  pkg.moduleCppEnabled = true;
  pkg.moduleCppModules["TensorBridge"].manifestPath =
      "native/tensor/modulecpp.toml";
  pkg.moduleCppModules["TensorBridge"].sourceDir = "native/tensor";
  pkg.moduleCppModules["TensorBridge"].cmakeTarget = "tensor_bridge";
  lock.packages[pkg.name] = pkg;

  std::string error;
  ASSERT_TRUE(writePackageLockFile(path.string(), lock, &error));

  PackageLock parsed;
  PackageLockParser parser;
  ASSERT_TRUE(parser.parseFile(path.string(), &parsed));
  ASSERT_TRUE(parsed.packages.count("tensorpkg") == 1u);
  const LockedPackage &roundTrip = parsed.packages.at("tensorpkg");
  ASSERT_EQ(roundTrip.github, "acme/tensorpkg");
  ASSERT_EQ(roundTrip.versionConstraint, "^1.0.0");
  ASSERT_EQ(roundTrip.resolvedTag, "v1.0.0");
  ASSERT_EQ(roundTrip.resolvedCommit, "abc123");
  ASSERT_EQ(roundTrip.packageVersion, "1.0.0");
  ASSERT_EQ(roundTrip.contentHash, "deadbeef");
  ASSERT_EQ(roundTrip.exportedModules.size(), 2u);
  ASSERT_TRUE(roundTrip.moduleCppEnabled);
  ASSERT_TRUE(roundTrip.moduleCppModules.count("TensorBridge") == 1u);

  fs::remove(path);
  return true;
}
