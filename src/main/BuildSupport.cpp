// BuildSupport.cpp — Runtime derleme ve derleme seçenekleri implementasyonu.
// Bkz. BuildSupport.h
#include "BuildSupport.h"
#include "AppGlobals.h"
#include "RuntimePaths.h"
#include "ToolchainUtils.h"

#include "neuronc/cli/ProjectConfig.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

// ── Label yardımcıları ───────────────────────────────────────────────────────

neuron::LLVMOptLevel toLLVMOptLevel(neuron::BuildOptimizeLevel level) {
  switch (level) {
  case neuron::BuildOptimizeLevel::O0:
    return neuron::LLVMOptLevel::O0;
  case neuron::BuildOptimizeLevel::O1:
    return neuron::LLVMOptLevel::O1;
  case neuron::BuildOptimizeLevel::O2:
    return neuron::LLVMOptLevel::O2;
  case neuron::BuildOptimizeLevel::O3:
    return neuron::LLVMOptLevel::O3;
  case neuron::BuildOptimizeLevel::Oz:
    return neuron::LLVMOptLevel::Oz;
  case neuron::BuildOptimizeLevel::Aggressive:
    return neuron::LLVMOptLevel::Aggressive;
  }
  return neuron::LLVMOptLevel::Aggressive;
}

neuron::LLVMTargetCPU toLLVMTargetCPU(neuron::BuildTargetCPU cpu) {
  switch (cpu) {
  case neuron::BuildTargetCPU::Native:
    return neuron::LLVMTargetCPU::Native;
  case neuron::BuildTargetCPU::Generic:
    return neuron::LLVMTargetCPU::Generic;
  }
  return neuron::LLVMTargetCPU::Native;
}

const char *optLevelLabel(neuron::BuildOptimizeLevel level) {
  switch (level) {
  case neuron::BuildOptimizeLevel::O0:
    return "O0";
  case neuron::BuildOptimizeLevel::O1:
    return "O1";
  case neuron::BuildOptimizeLevel::O2:
    return "O2";
  case neuron::BuildOptimizeLevel::O3:
    return "O3";
  case neuron::BuildOptimizeLevel::Oz:
    return "Oz";
  case neuron::BuildOptimizeLevel::Aggressive:
    return "aggressive";
  }
  return "aggressive";
}

const char *tensorProfileLabel(neuron::BuildTensorProfile profile) {
  switch (profile) {
  case neuron::BuildTensorProfile::Balanced:
    return "balanced";
  case neuron::BuildTensorProfile::GemmParity:
    return "gemm_parity";
  case neuron::BuildTensorProfile::AIFused:
    return "ai_fused";
  }
  return "balanced";
}

// ── Ortam değişkenleri ───────────────────────────────────────────────────────

void setProcessEnvVar(const std::string &name, const std::string &value) {
#ifdef _WIN32
  _putenv_s(name.c_str(), value.c_str());
#else
  setenv(name.c_str(), value.c_str(), 1);
#endif
}

void applyTensorRuntimeEnv(const std::optional<neuron::ProjectConfig> &cfg) {
  if (!cfg.has_value()) {
    return;
  }
  setProcessEnvVar("NEURON_TENSOR_PROFILE",
                   tensorProfileLabel(cfg->tensorProfile));
  setProcessEnvVar("NEURON_TENSOR_AUTOTUNE", cfg->tensorAutotune ? "1" : "0");
  if (!cfg->tensorKernelCache.empty()) {
    setProcessEnvVar("NEURON_TENSOR_KERNEL_CACHE", cfg->tensorKernelCache);
  }
}

// ── Derleme bayrakları ───────────────────────────────────────────────────────

std::string
runtimeOptimizationFlags(const neuron::LLVMCodeGenOptions &options) {
  std::string flags;
  switch (options.optLevel) {
  case neuron::LLVMOptLevel::O0:
    flags = "-O0";
    break;
  case neuron::LLVMOptLevel::O1:
    flags = "-O1";
    break;
  case neuron::LLVMOptLevel::O2:
    flags = "-O2";
    break;
  case neuron::LLVMOptLevel::O3:
    flags = "-O3 -fopenmp";
    break;
  case neuron::LLVMOptLevel::Oz:
    flags = "-Oz";
    break;
  case neuron::LLVMOptLevel::Aggressive:
    flags = "-O3 -ffast-math -fno-math-errno -funroll-loops "
            "-fstrict-aliasing -fomit-frame-pointer -fopenmp";
    break;
  }
  if (options.targetCPU == neuron::LLVMTargetCPU::Native) {
    flags += " -march=native -mtune=native";
  }
  return flags;
}

// ── Runtime obje derleme ─────────────────────────────────────────────────────

namespace {

std::optional<fs::path> currentExecutablePath() {
#ifdef _WIN32
  std::vector<wchar_t> buffer(MAX_PATH, L'\0');
  for (;;) {
    const DWORD written = GetModuleFileNameW(nullptr, buffer.data(),
                                             static_cast<DWORD>(buffer.size()));
    if (written == 0) {
      return std::nullopt;
    }
    if (written < buffer.size() - 1) {
      return fs::path(std::wstring(buffer.data(), written));
    }
    buffer.resize(buffer.size() * 2, L'\0');
  }
#elif defined(__APPLE__)
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  if (size == 0) {
    return std::nullopt;
  }
  std::vector<char> buffer(size + 1, '\0');
  if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
    return std::nullopt;
  }
  return fs::path(buffer.data());
#else
  std::vector<char> buffer(4096, '\0');
  for (;;) {
    const ssize_t written =
        readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (written < 0) {
      return std::nullopt;
    }
    if (static_cast<std::size_t>(written) < buffer.size() - 1) {
      buffer[static_cast<std::size_t>(written)] = '\0';
      return fs::path(buffer.data());
    }
    buffer.resize(buffer.size() * 2, '\0');
  }
#endif
}

std::optional<fs::path> firstExistingPath(
    const std::vector<fs::path> &candidates) {
  for (const fs::path &candidate : candidates) {
    std::error_code ec;
    if (fs::exists(candidate, ec) && !ec) {
      return candidate.lexically_normal();
    }
  }
  return std::nullopt;
}

std::optional<fs::path> prebuiltRuntimeLibraryPath() {
  const auto exePath = currentExecutablePath();
  if (!exePath.has_value()) {
    return std::nullopt;
  }
  return firstExistingPath(prebuiltRuntimeLibraryCandidates(exePath->parent_path()));
}

std::vector<fs::path> sharedRuntimeSourceList() {
  std::vector<fs::path> sources = {
      "runtime/src/runtime.c",
      "runtime/src/modulecpp_runtime.cpp",
      "runtime/src/tensor.c",
      "runtime/src/tensor/tensor_config.c",
      "runtime/src/tensor/tensor_core.c",
      "runtime/src/tensor/tensor_math.c",
      "runtime/src/tensor/tensor_microkernel.c",
      "runtime/src/nn.c",
      "runtime/src/gpu.c",
      "runtime/src/io.c",
      "runtime/src/platform/platform_manager.c",
      "runtime/src/platform/common/platform_error.c",
      "runtime/src/graphics/graphics_core_state.c",
      "runtime/src/graphics/graphics_core_window_canvas_api.c",
      "runtime/src/graphics/graphics_core_assets_api.c",
      "runtime/src/graphics/graphics_window_canvas.c",
      "runtime/src/graphics/graphics_assets.c",
      "runtime/src/graphics/assets/graphics_asset_png_wic.c",
      "runtime/src/graphics/assets/graphics_asset_obj_parser.c",
      "runtime/src/graphics/backend/vulkan/graphics_vk_dispatch_loader.c",
      "runtime/src/graphics/backend/vulkan/graphics_vk_swapchain.c",
      "runtime/src/graphics/backend/vulkan/graphics_vk_pipeline.c",
      "runtime/src/graphics/backend/vulkan/graphics_vk_backend_lifecycle.c",
      "runtime/src/graphics/backend/vulkan/graphics_vk_backend_frame.c",
      "runtime/src/graphics/backend/vulkan/graphics_vk_backend_interop.c",
      "runtime/src/graphics/backend/stub/graphics_vk_backend_stub.c",
      "runtime/src/gpu_vulkan/gpu_vulkan_core.c",
      "runtime/src/gpu_vulkan/gpu_vulkan_engine.c",
      "runtime/src/gpu_vulkan/gpu_vulkan_public_api.c",
      "runtime/src/gpu_vulkan/gpu_vulkan_init.c",
      "runtime/src/gpu_vulkan/gpu_vulkan_scope.c",
      "runtime/src/gpu_vulkan/gpu_vulkan_dispatch.c",
      "runtime/src/gpu_vulkan/gpu_vulkan_interop.c",
      "runtime/src/vulkan_common_state.c",
      "runtime/src/vulkan_common_loader.c",
      "runtime/src/vulkan_common_device.c",
      "runtime/src/vulkan_common_api.c",
      "runtime/src/gpu_cuda.c",
  };

#ifdef _WIN32
  sources.push_back("runtime/src/platform/win32/platform_win32_library.c");
  sources.push_back("runtime/src/platform/win32/platform_win32_env.c");
  sources.push_back("runtime/src/platform/win32/platform_win32_time.c");
  sources.push_back("runtime/src/platform/win32/platform_win32_path.c");
  sources.push_back("runtime/src/platform/win32/platform_win32_diagnostics.c");
  sources.push_back("runtime/src/platform/win32/platform_win32_process.c");
  sources.push_back("runtime/src/platform/win32/platform_win32_thread.c");
  sources.push_back("runtime/src/platform/win32/platform_win32_window.c");
  sources.push_back("runtime/src/graphics/platform/graphics_platform_win32.c");
#elif defined(__APPLE__)
  sources.push_back("runtime/src/platform/apple/platform_apple_library.c");
  sources.push_back("runtime/src/platform/apple/platform_apple_env.c");
  sources.push_back("runtime/src/platform/apple/platform_apple_time.c");
  sources.push_back("runtime/src/platform/apple/platform_apple_path.c");
  sources.push_back("runtime/src/platform/apple/platform_apple_diagnostics.c");
  sources.push_back("runtime/src/platform/apple/platform_apple_process.c");
  sources.push_back("runtime/src/platform/apple/platform_apple_thread.c");
  sources.push_back("runtime/src/platform/apple/platform_apple_window.c");
#else
  sources.push_back("runtime/src/platform/posix/platform_posix_library.c");
  sources.push_back("runtime/src/platform/posix/platform_posix_env.c");
  sources.push_back("runtime/src/platform/posix/platform_posix_time.c");
  sources.push_back("runtime/src/platform/posix/platform_posix_path.c");
  sources.push_back("runtime/src/platform/posix/platform_posix_diagnostics.c");
  sources.push_back("runtime/src/platform/posix/platform_posix_process.c");
  sources.push_back("runtime/src/platform/posix/platform_posix_thread.c");
  sources.push_back("runtime/src/platform/posix/platform_posix_window_x11.c");
#endif

  return sources;
}

fs::path sharedRuntimeRoot() {
  return (runtimeObjectDirectory() / "objects" / "shared").lexically_normal();
}

fs::path sharedRuntimeStampPath() {
  return sharedRuntimeRoot() / ".build_signature";
}

fs::path sharedRuntimeLinkResponsePath() {
  return sharedRuntimeRoot() / "link.rsp";
}

std::string sharedRuntimeCommonFlags(
    const neuron::LLVMCodeGenOptions &options) {
  std::string flags = runtimeOptimizationFlags(options);
  flags += " -DNEURON_RUNTIME_BUILD_SHARED";
  flags += " -DNPP_ENABLE_CUDA_BACKEND=1";
  flags += " -DNPP_ENABLE_VULKAN_BACKEND=1";
  flags += " -DNPP_ENABLE_WEBGPU_BACKEND=0";
#ifndef _WIN32
  flags += " -fPIC";
#endif
  flags += " -I" + quotePath(g_toolRoot / "runtime/include");
  flags += " -I" + quotePath(g_toolRoot / "runtime/src");
  return flags;
}

std::string sharedRuntimeBuildSignature(
    const neuron::LLVMCodeGenOptions &options,
    const std::vector<fs::path> &sources) {
  std::ostringstream out;
  out << sharedRuntimeCommonFlags(options) << '\n';
  for (const fs::path &source : sources) {
    out << source.generic_string() << '\n';
  }
  return out.str();
}

bool writeResponseFile(const fs::path &responsePath,
                       const std::vector<fs::path> &arguments,
                       std::string *outError) {
  std::error_code ec;
  fs::create_directories(responsePath.parent_path(), ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to create response file directory '" +
                  responsePath.parent_path().string() + "': " + ec.message();
    }
    return false;
  }

  std::ofstream out(responsePath, std::ios::trunc);
  if (!out.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to open response file '" + responsePath.string() + "'";
    }
    return false;
  }

  for (const fs::path &argument : arguments) {
    out << quotePath(argument) << '\n';
  }
  return true;
}

} // namespace

fs::path runtimeSharedLibraryPath() {
#ifdef _WIN32
  return sharedRuntimeRoot() / "neuron_runtime.dll";
#elif defined(__APPLE__)
  return sharedRuntimeRoot() / "libneuron_runtime.dylib";
#else
  return sharedRuntimeRoot() / "libneuron_runtime.so";
#endif
}

fs::path jitRuntimeSharedLibraryPath() {
  if (const auto prebuiltPath = prebuiltRuntimeLibraryPath();
      prebuiltPath.has_value()) {
    return *prebuiltPath;
  }
  return runtimeSharedLibraryPath();
}

fs::path runtimeSharedLinkPath() {
#ifdef _WIN32
  return sharedRuntimeRoot() / "libneuron_runtime.dll.a";
#else
  return runtimeSharedLibraryPath();
#endif
}

bool ensureRuntimeObjects(const neuron::LLVMCodeGenOptions &options) {
  (void)options;

  const fs::path runtimeLibrary = runtimeSharedLibraryPath();
  const fs::path runtimeLinkTarget = runtimeSharedLinkPath();
  if (fs::exists(runtimeLibrary) && fs::exists(runtimeLinkTarget)) {
    return true;
  }

  const neuron::LLVMCodeGenOptions buildOptions = sharedRuntimeBuildOptions();
  const std::vector<fs::path> sources = sharedRuntimeSourceList();
  const fs::path objectRoot = sharedRuntimeRoot();
  const fs::path signaturePath = sharedRuntimeStampPath();
  const std::string buildSignature =
      sharedRuntimeBuildSignature(buildOptions, sources);

  std::error_code ec;
  fs::create_directories(objectRoot, ec);
  if (ec) {
    std::cerr << "Failed to create runtime cache directory '"
              << objectRoot.string() << "': " << ec.message() << std::endl;
    return false;
  }

  bool forceRebuild = false;
  {
    std::ifstream in(signaturePath);
    if (!in.is_open()) {
      forceRebuild = true;
    } else {
      std::stringstream buffer;
      buffer << in.rdbuf();
      forceRebuild = buffer.str() != buildSignature;
    }
  }

  const std::string commonFlags = sharedRuntimeCommonFlags(buildOptions);
  std::vector<fs::path> objectPaths;
  objectPaths.reserve(sources.size());
  bool anyObjectRebuilt = false;

  for (const fs::path &sourceRel : sources) {
    const fs::path sourcePath = (g_toolRoot / sourceRel).lexically_normal();
    if (!fs::exists(sourcePath)) {
      std::cerr << "Runtime source missing: " << sourcePath.string()
                << std::endl;
      return false;
    }

    fs::path outputPath = (objectRoot / sourceRel).lexically_normal();
    outputPath.replace_extension(".o");
    fs::create_directories(outputPath.parent_path(), ec);
    if (ec) {
      std::cerr << "Failed to create runtime object directory '"
                << outputPath.parent_path().string() << "': " << ec.message()
                << std::endl;
      return false;
    }

    bool needsBuild = forceRebuild || !fs::exists(outputPath);
    if (!needsBuild) {
      const auto sourceTime = fs::last_write_time(sourcePath, ec);
      const auto objectTime = fs::last_write_time(outputPath, ec);
      if (ec) {
        needsBuild = true;
        ec.clear();
      } else {
        needsBuild = sourceTime > objectTime;
      }
    }

    if (needsBuild) {
      const bool isCpp = sourcePath.extension() == ".cpp";
      const std::string compiler = resolveToolCommand(isCpp ? "g++" : "gcc");
      std::string cmd = compiler + " -c " + commonFlags + " ";
      if (isCpp) {
        cmd += "-std=c++20 ";
      }
      cmd += quotePath(sourcePath) + " -o " + quotePath(outputPath);

      std::cout << "Building runtime object: " << outputPath.string()
                << std::endl;
      if (runSystemCommand(cmd) != 0) {
        std::cerr << "Failed to build runtime object: " << outputPath.string()
                  << std::endl;
        return false;
      }
      anyObjectRebuilt = true;
    }

    objectPaths.push_back(outputPath);
  }

  bool needsLink = forceRebuild || anyObjectRebuilt || !fs::exists(runtimeLibrary);
#ifdef _WIN32
  needsLink = needsLink || !fs::exists(runtimeLinkTarget);
#endif
  if (!needsLink && fs::exists(runtimeLibrary)) {
    const auto libraryWriteTime = fs::last_write_time(runtimeLibrary, ec);
    if (ec) {
      needsLink = true;
      ec.clear();
    } else {
      for (const fs::path &objectPath : objectPaths) {
        if (fs::last_write_time(objectPath, ec) > libraryWriteTime) {
          needsLink = true;
        }
        if (ec || needsLink) {
          ec.clear();
          break;
        }
      }
    }
  }

  if (needsLink) {
    std::string responseError;
    if (!writeResponseFile(sharedRuntimeLinkResponsePath(), objectPaths,
                           &responseError)) {
      std::cerr << responseError << std::endl;
      return false;
    }

    std::string linkCmd = resolveToolCommand("g++") + " -shared -o " +
                          quotePath(runtimeLibrary) + " @" +
                          quotePath(sharedRuntimeLinkResponsePath());
#ifdef _WIN32
    linkCmd += " \"-Wl,--out-implib," + runtimeLinkTarget.string() + "\"";
    linkCmd += " -Wl,--export-all-symbols";
#endif
    if (runtimeUsesOpenMP(buildOptions)) {
      linkCmd += " -fopenmp";
    }
#ifdef _WIN32
    linkCmd += " -lffi -luser32 -lgdi32 -lole32 -lwindowscodecs";
    linkCmd += " -luuid -lshell32";
#elif defined(__APPLE__)
    linkCmd += " -lffi";
#else
    linkCmd += " -lffi -ldl -lm";
#endif

    std::cout << "Linking runtime shared library: " << runtimeLibrary.string()
              << std::endl;
    if (runSystemCommand(linkCmd) != 0) {
      std::cerr << "Failed to link runtime shared library: "
                << runtimeLibrary.string() << std::endl;
      return false;
    }
  }

  std::ofstream out(signaturePath, std::ios::trunc);
  if (out.is_open()) {
    out << buildSignature;
  }
  return fs::exists(runtimeLibrary);
}

// ── Minimal manifest ─────────────────────────────────────────────────────────

bool readMinimalSourceManifest(const fs::path &manifestPath,
                               std::vector<fs::path> *outSources,
                               std::string *outError) {
  if (outSources == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null minimal source list output";
    }
    return false;
  }

  std::ifstream in(manifestPath);
  if (!in.is_open()) {
    if (outError != nullptr) {
      *outError =
          "failed to open minimal source manifest: " + manifestPath.string();
    }
    return false;
  }

  outSources->clear();
  std::string line;
  while (std::getline(in, line)) {
    const std::string cleaned = [&]() {
      auto t = line;
      auto notSpace = [](unsigned char c) { return !std::isspace(c); };
      t.erase(t.begin(), std::find_if(t.begin(), t.end(), notSpace));
      t.erase(std::find_if(t.rbegin(), t.rend(), notSpace).base(), t.end());
      return t;
    }();
    if (cleaned.empty() || cleaned.rfind("#", 0) == 0) {
      continue;
    }
    outSources->emplace_back(cleaned);
  }

  if (outSources->empty()) {
    if (outError != nullptr) {
      *outError = "minimal source manifest is empty: " + manifestPath.string();
    }
    return false;
  }
  return true;
}

// ── Otomatik test ────────────────────────────────────────────────────────────

bool runAutomatedTestSuite(const NeuronSettings &settings,
                           bool requireAutoFolder,
                           const std::string &phaseLabel) {
  if (g_bypassRules) {
    std::cout << "Bypassing project rules: skipping automated tests for "
              << phaseLabel << "." << std::endl;
    return true;
  }

#ifdef _WIN32
  const fs::path testBinary = fs::path("build") / "bin" / "neuron_tests.exe";
#else
  const fs::path testBinary = fs::path("build") / "bin" / "neuron_tests";
#endif
  const fs::path autoTestsDir = fs::path("tests") / "auto";

  if (!fs::exists(testBinary)) {
    return true;
  }
  if (requireAutoFolder && !fs::exists(autoTestsDir)) {
    return true;
  }

  std::cout << "Running automated tests for " << phaseLabel << "..."
            << std::endl;
  const auto start = std::chrono::steady_clock::now();
  const int testResult = runSystemCommand(quotePath(testBinary));
  const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  if (testResult != 0) {
    std::cerr << phaseLabel << " blocked: automated tests failed." << std::endl;
    return false;
  }

  if (settings.maxAutoTestDurationMs > 0 &&
      elapsedMs.count() > settings.maxAutoTestDurationMs) {
    std::cerr << phaseLabel << " blocked: automated tests exceeded "
              << settings.maxAutoTestDurationMs
              << " ms (actual: " << elapsedMs.count() << " ms)." << std::endl;
    return false;
  }

  return true;
}
