#include <cstring>
#include <cstdlib>
#include <string>

extern "C" {
#include "neuron_gpu.h"
}

static void gpu_test_set_env_value(const char *name, const char *value) {
#if defined(_WIN32)
  _putenv_s(name, value != nullptr ? value : "");
#else
  if (value != nullptr) {
    setenv(name, value, 1);
  } else {
    unsetenv(name);
  }
#endif
}

struct ScopedGpuEnvOverride {
  std::string name;
  bool had_old_value;
  std::string old_value;

  ScopedGpuEnvOverride(const char *key, const char *new_value)
      : name(key), had_old_value(false) {
    const char *existing = std::getenv(key);
    if (existing != nullptr) {
      had_old_value = true;
      old_value = existing;
    }
    gpu_test_set_env_value(key, new_value);
  }

  ~ScopedGpuEnvOverride() {
    if (had_old_value) {
      gpu_test_set_env_value(name.c_str(), old_value.c_str());
    } else {
      gpu_test_set_env_value(name.c_str(), nullptr);
    }
  }
};

TEST(GpuKernelGenerationKnownOp) {
  neuron_gpu_reset();
  const char *kernel = neuron_gpu_generate_kernel("tensor_add", 1);
  ASSERT_TRUE(kernel != nullptr);
  ASSERT_TRUE(std::strstr(kernel, "tensor_add") != nullptr);
  ASSERT_TRUE(std::strstr(kernel, "__global__") != nullptr);
  return true;
}

TEST(GpuMemcpyRoundTrip) {
  neuron_gpu_reset();

  float src[4] = {1.0f, 2.0f, 3.5f, 4.5f};
  float dst[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  void *device = neuron_gpu_malloc(sizeof(src));
  ASSERT_TRUE(device != nullptr);
  ASSERT_EQ(neuron_gpu_memcpy_to_device(device, src, sizeof(src)), 0);
  ASSERT_EQ(neuron_gpu_memcpy_to_host(dst, device, sizeof(dst)), 0);

  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(dst[i] == src[i]);
  }

  neuron_gpu_free(device);
  return true;
}

TEST(GpuMemoryStatsReflectLifecycle) {
  neuron_gpu_reset();

  NeuronGpuMemoryStats before = {0};
  NeuronGpuMemoryStats during = {0};
  NeuronGpuMemoryStats after = {0};

  neuron_gpu_get_memory_stats(&before);
  ASSERT_EQ(before.total_allocations, (size_t)0);

  void *ptr = neuron_gpu_malloc(256);
  ASSERT_TRUE(ptr != nullptr);
  neuron_gpu_get_memory_stats(&during);
  ASSERT_EQ(during.total_allocations, (size_t)1);

  neuron_gpu_free(ptr);
  neuron_gpu_get_memory_stats(&after);
  ASSERT_EQ(after.total_allocations, (size_t)0);

  return true;
}

TEST(GpuSupportsOpMatchesBackend) {
  neuron_gpu_reset();

  const NeuronGpuBackend backend = neuron_gpu_backend();
  const int addSupported = neuron_gpu_supports_op(NEURON_GPU_OP_TENSOR_ADD);
  const int fmaSupported = neuron_gpu_supports_op(NEURON_GPU_OP_TENSOR_FMA);
  const int matmulSupported =
      neuron_gpu_supports_op(NEURON_GPU_OP_TENSOR_MATMUL);

  if (backend == NEURON_GPU_BACKEND_CUDA_DRIVER) {
    ASSERT_EQ(addSupported, 1);
    ASSERT_EQ(fmaSupported, 1);
    ASSERT_EQ(matmulSupported, 1);
  } else if (backend == NEURON_GPU_BACKEND_VULKAN_COMPUTE) {
    ASSERT_EQ(addSupported, 1);
    ASSERT_EQ(fmaSupported, 1);
    ASSERT_EQ(matmulSupported, 1);
  } else {
    ASSERT_EQ(addSupported, 0);
    ASSERT_EQ(fmaSupported, 0);
    ASSERT_EQ(matmulSupported, 0);
  }

  return true;
}

TEST(ForceBackendVulkanSelectionOrFallback) {
  ScopedGpuEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  neuron_gpu_reset();

  const NeuronGpuBackend backend = neuron_gpu_backend();
  ASSERT_TRUE(backend == NEURON_GPU_BACKEND_VULKAN_COMPUTE ||
              backend == NEURON_GPU_BACKEND_CPU_FALLBACK);

  if (backend == NEURON_GPU_BACKEND_VULKAN_COMPUTE) {
    ASSERT_EQ(neuron_gpu_supports_op(NEURON_GPU_OP_TENSOR_ADD), 1);
    ASSERT_EQ(neuron_gpu_supports_op(NEURON_GPU_OP_TENSOR_FMA), 1);
    ASSERT_EQ(neuron_gpu_supports_op(NEURON_GPU_OP_TENSOR_MATMUL), 1);
  }

  return true;
}

TEST(ForceBackendCudaSelectionOrFallback) {
  ScopedGpuEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "cuda");
  neuron_gpu_reset();

  const NeuronGpuBackend backend = neuron_gpu_backend();
  ASSERT_TRUE(backend == NEURON_GPU_BACKEND_CUDA_DRIVER ||
              backend == NEURON_GPU_BACKEND_CPU_FALLBACK);

  if (backend == NEURON_GPU_BACKEND_CUDA_DRIVER) {
    ASSERT_EQ(neuron_gpu_supports_op(NEURON_GPU_OP_TENSOR_ADD), 1);
    ASSERT_EQ(neuron_gpu_supports_op(NEURON_GPU_OP_TENSOR_FMA), 1);
    ASSERT_EQ(neuron_gpu_supports_op(NEURON_GPU_OP_TENSOR_MATMUL), 1);
  }

  return true;
}

TEST(GpuScopeEndWithoutBeginFails) {
  neuron_gpu_reset();

  ASSERT_EQ(neuron_gpu_scope_end(), -1);
  const char *lastError = neuron_gpu_last_error();
  ASSERT_TRUE(lastError != nullptr);
  ASSERT_TRUE(std::strstr(lastError, "scope_end") != nullptr);

  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);
  return true;
}

TEST(GpuScopeNestedDepthBalanced) {
  ScopedGpuEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  neuron_gpu_reset();

  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);
  ASSERT_EQ(neuron_gpu_scope_end(), -1);
  return true;
}

TEST(GpuScopeBeginExAcceptsNamedPreferenceSelectors) {
  ScopedGpuEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "auto");
  neuron_gpu_reset();

  ASSERT_EQ(neuron_gpu_scope_begin_ex(NEURON_GPU_SCOPE_MODE_PREFER,
                                      NEURON_GPU_DEVICE_CLASS_INTEGRATED),
            0);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);

  ASSERT_EQ(neuron_gpu_scope_begin_ex(NEURON_GPU_SCOPE_MODE_PREFER,
                                      NEURON_GPU_DEVICE_CLASS_DISCRETE),
            0);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);
  return true;
}

TEST(GpuScopeBeginExRejectsInvalidSelectors) {
  neuron_gpu_reset();

  ASSERT_EQ(neuron_gpu_scope_begin_ex((NeuronGpuScopeMode)99,
                                      NEURON_GPU_DEVICE_CLASS_ANY),
            -1);
  ASSERT_EQ(neuron_gpu_scope_begin_ex(NEURON_GPU_SCOPE_MODE_DEFAULT,
                                      (NeuronGpuDeviceClass)77),
            -1);
  return true;
}
