#include "neuronc/ncon/Sandbox.h"

#include <filesystem>

using namespace neuron;
namespace fs = std::filesystem;

TEST(NconSandboxResolvesAndDeniesPaths) {
  NconPermissionConfig permissions;
  permissions.fsRead = {"res:/", "work:/input/"};
  permissions.fsWrite = {"work:/output/"};

  ncon::SandboxContext sandbox;
  std::string error;
  ASSERT_TRUE(ncon::initializeSandbox("sandbox-test", permissions, &sandbox, &error));
  ASSERT_TRUE(error.empty());

  fs::path outputPath;
  ASSERT_TRUE(ncon::resolveLogicalPath(sandbox, "work:/output/result.txt",
                                       ncon::SandboxAccessMode::Write,
                                       &outputPath, &error));
  ASSERT_TRUE(outputPath.string().find("result.txt") != std::string::npos);

  ASSERT_FALSE(ncon::resolveLogicalPath(sandbox, "work:/secret.txt",
                                        ncon::SandboxAccessMode::Write,
                                        &outputPath, &error));
  ASSERT_TRUE(error.find("permission denied") != std::string::npos);

  ASSERT_FALSE(ncon::resolveLogicalPath(sandbox, "work:/../escape.txt",
                                        ncon::SandboxAccessMode::Write,
                                        &outputPath, &error));
  ASSERT_TRUE(error.find("escapes its mount root") != std::string::npos);
  return true;
}
