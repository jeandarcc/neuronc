// Project config parser tests - included from tests/test_main.cpp
#include "neuronc/cli/ProjectConfig.h"
#include <string>

using namespace neuron;

TEST(ProjectConfigBasic) {
  const std::string content =
      "[project]\n"
      "name = \"demo\"\n"
      "version = \"1.2.3\"\n"
      "\n"
      "[build]\n"
      "main = \"app/Main.nr\"\n"
      "build_dir = \"out\"\n"
      "optimize = \"O2\"\n"
      "emit_ir = \"both\"\n"
      "target_cpu = \"generic\"\n"
      "tensor_profile = \"gemm_parity\"\n"
      "tensor_autotune = false\n"
      "tensor_kernel_cache = \"cache/tensor\"\n"
      "\n"
      "[dependencies]\n"
      "tensor = \"1.0\"\n"
      "math = \"2.1\"\n";

  ProjectConfig config;
  ProjectConfigParser parser;
  const bool parsed = parser.parseString(content, "<config_test>", &config);
  if (!parsed) {
    for (const auto &error : parser.errors()) {
      std::cerr << error << std::endl;
    }
  }
  ASSERT_TRUE(parsed);
  ASSERT_TRUE(parser.errors().empty());
  ASSERT_EQ(config.name, "demo");
  ASSERT_EQ(config.version, "1.2.3");
  ASSERT_EQ(config.mainFile, "app/Main.nr");
  ASSERT_EQ(config.buildDir, "out");
  ASSERT_EQ(config.optimizeLevel, BuildOptimizeLevel::O2);
  ASSERT_EQ(config.emitIR, BuildEmitIR::Both);
  ASSERT_EQ(config.targetCPU, BuildTargetCPU::Generic);
  ASSERT_EQ(config.tensorProfile, BuildTensorProfile::GemmParity);
  ASSERT_FALSE(config.tensorAutotune);
  ASSERT_EQ(config.tensorKernelCache, "cache/tensor");
  ASSERT_EQ(config.dependencies.size(), 2u);
  ASSERT_EQ(config.dependencies["tensor"].version, "1.0");
  ASSERT_TRUE(config.dependencies["tensor"].legacyShorthand);
  ASSERT_EQ(config.dependencies["math"].version, "2.1");
  ASSERT_TRUE(config.dependencies["math"].legacyShorthand);
  return true;
}

TEST(ProjectConfigCommentsAndUnknownSection) {
  const std::string content =
      "# leading comment\n"
      "[project]\n"
      "name = \"demo\"\n"
      "\n"
      "[unknown]\n"
      "flag = \"x\"\n"
      "\n"
      "[dependencies]\n"
      "tensor = \"1.0\" # inline comment\n";

  ProjectConfig config;
  ProjectConfigParser parser;
  ASSERT_TRUE(parser.parseString(content, "<config_test>", &config));
  ASSERT_TRUE(parser.errors().empty());
  ASSERT_EQ(config.name, "demo");
  ASSERT_EQ(config.mainFile, "src/Main.nr");
  ASSERT_EQ(config.optimizeLevel, BuildOptimizeLevel::Aggressive);
  ASSERT_EQ(config.emitIR, BuildEmitIR::Optimized);
  ASSERT_EQ(config.targetCPU, BuildTargetCPU::Native);
  ASSERT_EQ(config.tensorProfile, BuildTensorProfile::Balanced);
  ASSERT_TRUE(config.tensorAutotune);
  ASSERT_EQ(config.tensorKernelCache, "build/.neuron_cache/tensor/");
  ASSERT_EQ(config.dependencies.size(), 1u);
  ASSERT_EQ(config.dependencies["tensor"].version, "1.0");
  ASSERT_TRUE(config.dependencies["tensor"].legacyShorthand);
  return true;
}

TEST(ProjectConfigParsesPackageMetadataAndInlineDependencies) {
  const std::string content =
      "[project]\n"
      "name = \"tensor-tools\"\n"
      "version = \"1.4.0\"\n"
      "\n"
      "[package]\n"
      "kind = \"library\"\n"
      "description = \"Tensor helper library\"\n"
      "repository = \"https://github.com/acme/tensor-tools\"\n"
      "license = \"MIT\"\n"
      "source_dir = \"lib\"\n"
      "\n"
      "[dependencies]\n"
      "core-math = { github = \"acme/core-math\", version = \"^2.1.0\" }\n"
      "gpu-bridge = { github = \"acme/gpu-bridge\", tag = \"v0.4.3\" }\n"
      "\n"
      "[modulecpp]\n"
      "enabled = true\n"
      "\n"
      "[modulecpp.TensorBridge]\n"
      "manifest = \"native/tensor/modulecpp.toml\"\n"
      "source_dir = \"native/tensor\"\n"
      "cmake_target = \"tensor_bridge\"\n";

  ProjectConfig config;
  ProjectConfigParser parser;
  ASSERT_TRUE(parser.parseString(content, "<config_test>", &config));
  ASSERT_TRUE(config.package.enabled);
  ASSERT_EQ(config.package.kind, PackageKind::Library);
  ASSERT_EQ(config.package.description, "Tensor helper library");
  ASSERT_EQ(config.package.repository, "https://github.com/acme/tensor-tools");
  ASSERT_EQ(config.package.license, "MIT");
  ASSERT_EQ(config.package.sourceDir, "lib");
  ASSERT_EQ(config.dependencies.size(), 2u);
  ASSERT_EQ(config.dependencies["core-math"].github, "acme/core-math");
  ASSERT_EQ(config.dependencies["core-math"].version, "^2.1.0");
  ASSERT_EQ(config.dependencies["gpu-bridge"].github, "acme/gpu-bridge");
  ASSERT_EQ(config.dependencies["gpu-bridge"].tag, "v0.4.3");
  ASSERT_TRUE(config.moduleCppEnabled);
  ASSERT_TRUE(config.moduleCppModules.count("TensorBridge") == 1u);
  ASSERT_EQ(config.moduleCppModules.at("TensorBridge").manifestPath,
            "native/tensor/modulecpp.toml");
  ASSERT_EQ(config.ncon.native.modules.size(), 1u);
  return true;
}

TEST(ProjectConfigInvalidSyntax) {
  const std::string content =
      "[project]\n"
      "name \"broken\"\n";

  ProjectConfig config;
  ProjectConfigParser parser;
  ASSERT_FALSE(parser.parseString(content, "<config_test>", &config));
  ASSERT_FALSE(parser.errors().empty());
  return true;
}

TEST(ProjectConfigInvalidTensorProfile) {
  const std::string content =
      "[project]\n"
      "name = \"broken\"\n"
      "\n"
      "[build]\n"
      "tensor_profile = \"rocket\"\n";

  ProjectConfig config;
  ProjectConfigParser parser;
  ASSERT_FALSE(parser.parseString(content, "<config_test>", &config));
  ASSERT_FALSE(parser.errors().empty());
  return true;
}

TEST(ProjectConfigInvalidBuildOptions) {
  const std::string content =
      "[project]\n"
      "name = \"broken\"\n"
      "\n"
      "[build]\n"
      "optimize = \"crazy\"\n";

  ProjectConfig config;
  ProjectConfigParser parser;
  ASSERT_FALSE(parser.parseString(content, "<config_test>", &config));
  ASSERT_FALSE(parser.errors().empty());
  return true;
}

TEST(ProjectConfigNconSections) {
  const std::string content =
      "[project]\n"
      "name = \"demo\"\n"
      "\n"
      "[build]\n"
      "ncon = true\n"
      "\n"
      "[ncon]\n"
      "output = \"build/containers/demo.ncon\"\n"
      "include_debug_map = false\n"
      "\n"
      "[ncon.permissions]\n"
      "fs_read = [\"res:/\", \"work:/input/\"]\n"
      "fs_write = [\"work:/\"]\n"
      "network = \"deny\"\n"
      "process_spawn = false\n"
      "\n"
      "[ncon.resources]\n"
      "\"model/weights.bin\" = \"models/weights.bin\"\n";

  ProjectConfig config;
  ProjectConfigParser parser;
  ASSERT_TRUE(parser.parseString(content, "<config_test>", &config));
  ASSERT_TRUE(config.ncon.enabled);
  ASSERT_EQ(config.ncon.outputPath, "build/containers/demo.ncon");
  ASSERT_FALSE(config.ncon.includeDebugMap);
  ASSERT_EQ(config.ncon.permissions.fsRead.size(), 2u);
  ASSERT_EQ(config.ncon.permissions.fsRead[0], "res:/");
  ASSERT_EQ(config.ncon.permissions.fsRead[1], "work:/input/");
  ASSERT_EQ(config.ncon.permissions.fsWrite.size(), 1u);
  ASSERT_EQ(config.ncon.permissions.fsWrite[0], "work:/");
  ASSERT_EQ(config.ncon.permissions.network, NconNetworkPolicy::Deny);
  ASSERT_FALSE(config.ncon.permissions.processSpawnAllowed);
  ASSERT_EQ(config.ncon.resources.size(), 1u);
  ASSERT_EQ(config.ncon.resources[0].logicalId, "model/weights.bin");
  ASSERT_EQ(config.ncon.resources[0].sourcePath, "models/weights.bin");
  return true;
}

TEST(ProjectConfigParsesHotReloadAndModuleCpp) {
  const std::string content =
      "[project]\n"
      "name = \"demo\"\n"
      "\n"
      "[ncon]\n"
      "hot_reload = true\n"
      "\n"
      "[ncon.native]\n"
      "enabled = true\n"
      "\n"
      "[ncon.modulecpp.Tensorflow]\n"
      "manifest = \"native/tensorflow/modulecpp.toml\"\n"
      "build_system = \"cmake\"\n"
      "source_dir = \"native/tensorflow\"\n"
      "cmake_target = \"tensorflow_bridge\"\n"
      "artifact_windows_x64 = \"dist/tensorflow.dll\"\n"
      "artifact_linux_x64 = \"dist/libtensorflow.so\"\n"
      "artifact_macos_arm64 = \"dist/libtensorflow.dylib\"\n";

  ProjectConfig config;
  ProjectConfigParser parser;
  ASSERT_TRUE(parser.parseString(content, "<config_test>", &config));
  ASSERT_TRUE(parser.errors().empty());
  ASSERT_TRUE(config.ncon.hotReload);
  ASSERT_TRUE(config.ncon.native.enabled);
  ASSERT_EQ(config.ncon.native.modules.size(), 1u);
  ASSERT_TRUE(config.ncon.native.modules.count("Tensorflow") == 1u);

  const auto &module = config.ncon.native.modules.at("Tensorflow");
  ASSERT_EQ(module.manifestPath, "native/tensorflow/modulecpp.toml");
  ASSERT_EQ(module.buildSystem, "cmake");
  ASSERT_EQ(module.sourceDir, "native/tensorflow");
  ASSERT_EQ(module.cmakeTarget, "tensorflow_bridge");
  ASSERT_EQ(module.artifactWindowsX64, "dist/tensorflow.dll");
  ASSERT_EQ(module.artifactLinuxX64, "dist/libtensorflow.so");
  ASSERT_EQ(module.artifactMacosArm64, "dist/libtensorflow.dylib");
  return true;
}

TEST(ProjectConfigParsesWebSection) {
  const std::string content =
      "[project]\n"
      "name = \"webdemo\"\n"
      "\n"
      "[web]\n"
      "canvas_id = \"game-canvas\"\n"
      "wgsl_cache = false\n"
      "dev_server_port = 9090\n"
      "enable_shared_array = true\n"
      "initial_memory_mb = 96\n"
      "maximum_memory_mb = 768\n"
      "wasm_simd = true\n";

  ProjectConfig config;
  ProjectConfigParser parser;
  ASSERT_TRUE(parser.parseString(content, "<config_test>", &config));
  ASSERT_TRUE(parser.errors().empty());
  ASSERT_EQ(config.web.canvasId, "game-canvas");
  ASSERT_FALSE(config.web.wgslCache);
  ASSERT_EQ(config.web.devServerPort, 9090);
  ASSERT_TRUE(config.web.enableSharedArray);
  ASSERT_EQ(config.web.initialMemoryMb, 96);
  ASSERT_EQ(config.web.maximumMemoryMb, 768);
  ASSERT_TRUE(config.web.wasmSimd);
  return true;
}

TEST(ProjectConfigRejectsInvalidWebPort) {
  const std::string content =
      "[project]\n"
      "name = \"webdemo\"\n"
      "\n"
      "[web]\n"
      "dev_server_port = 70000\n";

  ProjectConfig config;
  ProjectConfigParser parser;
  ASSERT_FALSE(parser.parseString(content, "<config_test>", &config));
  ASSERT_FALSE(parser.errors().empty());
  return true;
}
