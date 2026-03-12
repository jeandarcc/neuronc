#include "../../src/main/BuildSupport.h"

TEST(SharedRuntimeBuildUsesCanonicalOptions) {
  const neuron::LLVMCodeGenOptions options = sharedRuntimeBuildOptions();

  ASSERT_EQ(options.optLevel, neuron::LLVMOptLevel::O2);
  ASSERT_EQ(options.targetCPU, neuron::LLVMTargetCPU::Generic);
  ASSERT_FALSE(runtimeUsesOpenMP(options));
  return true;
}

TEST(SharedRuntimePrefersPrebuiltBuildArtifactsNearExecutable) {
  const fs::path exeDir = fs::path("build-mingw") / "bin";
  const std::vector<fs::path> libraryCandidates =
      prebuiltRuntimeLibraryCandidates(exeDir);

#ifdef _WIN32
  ASSERT_EQ(libraryCandidates.front(),
            exeDir / "libneuron_runtime.dll");
#elif defined(__APPLE__)
  ASSERT_EQ(libraryCandidates.front(),
            exeDir / "libneuron_runtime.dylib");
#else
  ASSERT_EQ(libraryCandidates.front(),
            exeDir / "libneuron_runtime.so");
#endif
  return true;
}
