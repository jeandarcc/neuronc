#include "neuronc/cli/MinimalBuilder.h"

#include <algorithm>
#include <fstream>

using namespace neuron;

TEST(MinimalBuildRequiresPlatformFlag) {
  MinimalBuildOptions options;
  std::string error;
  const bool ok =
      parseMinimalBuildArgs({}, "windows_x64", &options, &error);
  ASSERT_FALSE(ok);
  ASSERT_TRUE(error.find("--platform") != std::string::npos);
  return true;
}

TEST(MinimalBuildParsesWindowsCaseInsensitive) {
  MinimalBuildOptions options;
  std::string error;
  const bool ok = parseMinimalBuildArgs({"--platform", "wInDoWs"},
                                        "windows_x64", &options, &error);
  ASSERT_TRUE(ok);
  ASSERT_EQ(options.platformId, "windows_x64");
  ASSERT_EQ(options.outputPath.string(), "nucleus.exe");
  ASSERT_FALSE(options.compilerProvided);
  return true;
}

TEST(MinimalBuildRequiresCompilerForNonHostTarget) {
  MinimalBuildOptions options;
  std::string error;
  const bool ok =
      parseMinimalBuildArgs({"--platform", "Linux"}, "windows_x64", &options,
                            &error);
  ASSERT_FALSE(ok);
  ASSERT_TRUE(error.find("--compiler") != std::string::npos);
  return true;
}

TEST(MinimalBuildAcceptsCompilerForNonHostTarget) {
  MinimalBuildOptions options;
  std::string error;
  const bool ok = parseMinimalBuildArgs({"--platform", "Linux", "--compiler",
                                         "x86_64-linux-gnu-g++"},
                                        "windows_x64", &options, &error);
  ASSERT_TRUE(ok);
  ASSERT_EQ(options.platformId, "linux_x64");
  ASSERT_TRUE(options.compilerProvided);
  ASSERT_EQ(options.compilerPath, "x86_64-linux-gnu-g++");
  return true;
}

TEST(MinimalBuildLinkCommandUsesStaticFlags) {
  std::vector<std::filesystem::path> objects = {"a.o", "b.o"};
  const std::string linkCmd = buildMinimalLinkCommand(
      "g++", objects, std::filesystem::path("nucleus.exe"), "windows_x64");
  ASSERT_TRUE(linkCmd.find("-static") != std::string::npos);
  ASSERT_TRUE(linkCmd.find("-static-libstdc++") != std::string::npos);
  ASSERT_TRUE(linkCmd.find("-static-libgcc") != std::string::npos);
  return true;
}

TEST(MinimalBuildCompileCommandAddsRuntimeSourceIncludeRoot) {
  const std::string compileCmd = buildMinimalCompileCommand(
      "gcc", std::filesystem::path("runtime/src/tensor/tensor_config.c"),
      std::filesystem::path("build/tensor_config.o"),
      std::filesystem::path("C:/toolroot"));
  std::string normalizedCompileCmd = compileCmd;
  std::replace(normalizedCompileCmd.begin(), normalizedCompileCmd.end(), '\\',
               '/');
  ASSERT_TRUE(normalizedCompileCmd.find("-I\"C:/toolroot/runtime/src\"") !=
              std::string::npos);
  return true;
}

TEST(MinimalBuildLinkCommandSupportsResponseFile) {
  const std::string linkCmd = buildMinimalLinkCommandWithResponseFile(
      "g++", std::filesystem::path("build/minimal-link.rsp"),
      std::filesystem::path("nucleus.exe"), "windows_x64");
  ASSERT_TRUE(linkCmd.find("@build/minimal-link.rsp") != std::string::npos);
  ASSERT_TRUE(linkCmd.find("-static-libstdc++") != std::string::npos);
  return true;
}

TEST(MinimalBuildManifestEntriesExist) {
  const std::filesystem::path repoRoot =
      std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
  const std::filesystem::path manifestPath =
      repoRoot / "runtime" / "minimal" / "sources.manifest";

  std::ifstream manifest(manifestPath);
  ASSERT_TRUE(manifest.is_open());

  std::string line;
  bool sawSource = false;
  while (std::getline(manifest, line)) {
    const auto first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || line[first] == '#') {
      continue;
    }
    const auto last = line.find_last_not_of(" \t\r\n");
    const std::string cleaned = line.substr(first, last - first + 1);
    if (!std::filesystem::exists(repoRoot / cleaned)) {
      std::cerr << "Manifest entry NOT FOUND at: " << (repoRoot / cleaned).string() << " (repoRoot=" << repoRoot.string() << ", cleaned=" << cleaned << ")" << std::endl;
      ASSERT_TRUE(false);
    }
    sawSource = true;
  }

  ASSERT_TRUE(sawSource);
  return true;
}
