#include "neuronc/ncon/Manifest.h"

using namespace neuron;

TEST(NconManifestRoundTripsNativeModules) {
  ncon::ManifestData manifest;
  manifest.appName = "demo";
  manifest.appVersion = "1.0.0";
  manifest.entryModule = "Main";
  manifest.entryFunction = "Init";
  manifest.sourceHash = "abc123";
  manifest.optimize = "aggressive";
  manifest.targetCPU = "generic";
  manifest.tensorProfile = "balanced";
  manifest.tensorAutotune = true;
  manifest.hotReload = true;
  manifest.nativeEnabled = true;
  manifest.permissions.fsRead = {"res:/"};
  manifest.permissions.fsWrite = {"work:/"};
  manifest.resources.push_back({"data/model.bin", "models/model.bin", 12, 77, 0});

  ncon::NativeModuleManifestInfo module;
  module.name = "Tensorflow";
  module.abi = "c";
  module.exports.push_back({"Version", "npp_tf_version", {}, "string"});
  module.artifacts.push_back(
      {"windows_x64", "__nativemodules__/Tensorflow/windows_x64/tf.dll",
       "tf.dll", 42, 99, "deadbeef"});
  manifest.nativeModules.push_back(module);

  const std::string json = ncon::buildManifestJson(manifest);
  ncon::ManifestData parsed;
  std::string error;
  ASSERT_TRUE(ncon::parseManifestData(json, &parsed, &error));
  ASSERT_TRUE(error.empty());
  ASSERT_EQ(parsed.appName, "demo");
  ASSERT_TRUE(parsed.hotReload);
  ASSERT_TRUE(parsed.nativeEnabled);
  ASSERT_EQ(parsed.nativeModules.size(), 1u);
  ASSERT_EQ(parsed.nativeModules[0].name, "Tensorflow");
  ASSERT_EQ(parsed.nativeModules[0].exports.size(), 1u);
  ASSERT_EQ(parsed.nativeModules[0].exports[0].symbol, "npp_tf_version");
  ASSERT_EQ(parsed.nativeModules[0].artifacts.size(), 1u);
  ASSERT_EQ(parsed.nativeModules[0].artifacts[0].platform, "windows_x64");
  return true;
}
