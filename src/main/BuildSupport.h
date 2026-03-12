// BuildSupport.h — Runtime obje derleme ve derleme seçenekleri yardımcıları.
//
// Bu modül şu işlemleri sağlar:
//   - Runtime C/C++ kaynaklarını önbelleğe alarak derleme (ensureRuntimeObjects)
//   - Minimal kaynak manifestini okuma
//   - LLVMOptLevel / LLVMTargetCPU dönüşüm yardımcıları
//   - Tensor runtime ortam değişkenlerini ayarlama
//   - Otomatik test paketi çalıştırma
//
// Yeni bir runtime kaynak dosyası eklemek için ensureRuntimeObjects() içindeki
// 'units' dizisine yeni bir giriş ekle.
#pragma once

#include "neuronc/cli/DebugSupport.h"
#include "neuronc/cli/ProjectConfig.h"
#include "neuronc/codegen/LLVMCodeGen.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using neuron::cli::NeuronSettings;

// ── Label yardımcıları ───────────────────────────────────────────────────────

neuron::LLVMOptLevel    toLLVMOptLevel(neuron::BuildOptimizeLevel level);
neuron::LLVMTargetCPU   toLLVMTargetCPU(neuron::BuildTargetCPU cpu);
const char             *optLevelLabel(neuron::BuildOptimizeLevel level);
const char             *tensorProfileLabel(neuron::BuildTensorProfile profile);

// ── Ortam değişkenleri ───────────────────────────────────────────────────────

/// Bir süreç ortam değişkenini ayarlar (cross-platform).
void setProcessEnvVar(const std::string &name, const std::string &value);

/// Proje config'inden tensor runtime env değişkenlerini ayarlar.
void applyTensorRuntimeEnv(const std::optional<neuron::ProjectConfig> &cfg);

// ── Derleme bayrakları ───────────────────────────────────────────────────────

/// Opt level'a göre gcc/g++ optimizasyon bayraklarını döner.
std::string runtimeOptimizationFlags(const neuron::LLVMCodeGenOptions &options);

/// Verilen seçeneklerin OpenMP gerektirip gerektirmediğini döner.
inline bool runtimeUsesOpenMP(const neuron::LLVMCodeGenOptions &options) {
  return options.optLevel == neuron::LLVMOptLevel::O3 ||
         options.optLevel == neuron::LLVMOptLevel::Aggressive;
}

inline neuron::LLVMCodeGenOptions sharedRuntimeBuildOptions() {
  neuron::LLVMCodeGenOptions options;
  options.optLevel = neuron::LLVMOptLevel::O2;
  options.targetCPU = neuron::LLVMTargetCPU::Generic;
  return options;
}

inline std::vector<fs::path>
prebuiltRuntimeLibraryCandidates(const fs::path &exeDir) {
#ifdef _WIN32
  return {exeDir / "libneuron_runtime.dll", exeDir / "neuron_runtime.dll"};
#elif defined(__APPLE__)
  return {exeDir / "libneuron_runtime.dylib", exeDir / "neuron_runtime.dylib"};
#else
  return {exeDir / "libneuron_runtime.so", exeDir / "neuron_runtime.so"};
#endif
}

// ── Runtime obje derleme ─────────────────────────────────────────────────────

/// runtime/src/ altındaki C/C++ kaynaklarını önbelleğe alarak derler.
/// Bayraklar değişmemişse ve kaynak dosya değişmemişse atlar.
bool ensureRuntimeObjects(const neuron::LLVMCodeGenOptions &options);
fs::path runtimeSharedLibraryPath();
fs::path jitRuntimeSharedLibraryPath();
fs::path runtimeSharedLinkPath();

// ── Minimal manifest ─────────────────────────────────────────────────────────

/// runtime/minimal/sources.manifest dosyasını okur.
bool readMinimalSourceManifest(const fs::path &manifestPath,
                                std::vector<fs::path> *outSources,
                                std::string *outError);

// ── Otomatik test ────────────────────────────────────────────────────────────

/// build/bin/neuron_tests çalıştırır; süre ve sonucu kontrol eder.
bool runAutomatedTestSuite(const NeuronSettings &settings,
                            bool requireAutoFolder,
                            const std::string &phaseLabel);

