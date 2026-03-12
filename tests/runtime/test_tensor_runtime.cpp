// Tensor runtime tests - included from tests/test_main.cpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>

extern "C" {
#include "neuron_gpu.h"
#include "neuron_tensor.h"
}

static void fill_tensor_sequential(NeuronTensor *t, float start, float step) {
  if (t == nullptr) {
    return;
  }
  float value = start;
  for (int32_t i = 0; i < t->size; ++i) {
    t->data[i] = value;
    value += step;
  }
}

static void set_env_value(const char *name, const char *value) {
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

struct ScopedEnvOverride {
  std::string name;
  bool hadOldValue;
  std::string oldValue;

  ScopedEnvOverride(const char *key, const char *newValue)
      : name(key), hadOldValue(false) {
    const char *existing = std::getenv(key);
    if (existing != nullptr) {
      hadOldValue = true;
      oldValue = existing;
    }
    set_env_value(key, newValue);
  }

  ~ScopedEnvOverride() {
    if (hadOldValue) {
      set_env_value(name.c_str(), oldValue.c_str());
    } else {
      set_env_value(name.c_str(), nullptr);
    }
  }
};

static void fill_circulant_from_row(NeuronTensor *matrix, const float *firstRow,
                                    int32_t n) {
  if (matrix == nullptr || firstRow == nullptr || matrix->dimensions != 2 ||
      matrix->shape[0] != n || matrix->shape[1] != n) {
    return;
  }
  for (int32_t r = 0; r < n; ++r) {
    for (int32_t c = 0; c < n; ++c) {
      const int32_t idx = (c - r + n) % n;
      matrix->data[r * n + c] = firstRow[idx];
    }
  }
}

static void fill_toeplitz_from_row_col(NeuronTensor *matrix, const float *firstRow,
                                       const float *firstCol, int32_t n) {
  if (matrix == nullptr || firstRow == nullptr || firstCol == nullptr ||
      matrix->dimensions != 2 || matrix->shape[0] != n ||
      matrix->shape[1] != n) {
    return;
  }
  for (int32_t r = 0; r < n; ++r) {
    for (int32_t c = 0; c < n; ++c) {
      matrix->data[r * n + c] = (c >= r) ? firstRow[c - r] : firstCol[r - c];
    }
  }
}

static void add_sparse_residual(NeuronTensor *matrix, int32_t n) {
  if (matrix == nullptr || matrix->dimensions != 2 || matrix->shape[0] != n ||
      matrix->shape[1] != n) {
    return;
  }
  const int32_t coords[][2] = {
      {0, 1}, {1, 3}, {2, 6}, {3, 2}, {4, 7}, {5, 0}, {6, 5}, {7, 4}};
  const float deltas[] = {0.020f, -0.015f, 0.018f, -0.022f,
                          0.016f, -0.019f, 0.021f, -0.017f};
  for (int32_t i = 0; i < 8; ++i) {
    const int32_t r = coords[i][0];
    const int32_t c = coords[i][1];
    matrix->data[r * n + c] += deltas[i];
  }
}

static bool tensor_close(const NeuronTensor *lhs, const NeuronTensor *rhs,
                         float epsilon) {
  if (lhs == nullptr || rhs == nullptr || lhs->size != rhs->size) {
    return false;
  }
  for (int32_t i = 0; i < lhs->size; ++i) {
    if (std::fabs(lhs->data[i] - rhs->data[i]) >= epsilon) {
      return false;
    }
  }
  return true;
}

static NeuronTensor *create_tensor_1d(std::initializer_list<float> values) {
  int32_t shape[1] = {static_cast<int32_t>(values.size())};
  NeuronTensor *tensor = neuron_tensor_create(1, shape);
  if (tensor == nullptr) {
    return nullptr;
  }
  int32_t index = 0;
  for (float value : values) {
    tensor->data[index++] = value;
  }
  return tensor;
}

static NeuronTensor *create_tensor_4d(int32_t n, int32_t c, int32_t h,
                                      int32_t w,
                                      std::initializer_list<float> values) {
  int32_t shape[4] = {n, c, h, w};
  NeuronTensor *tensor = neuron_tensor_create(4, shape);
  if (tensor == nullptr) {
    return nullptr;
  }
  int32_t index = 0;
  for (float value : values) {
    if (index >= tensor->size) {
      break;
    }
    tensor->data[index++] = value;
  }
  return tensor;
}

static double tensor_median_ms(std::vector<double> values) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const size_t mid = values.size() / 2;
  if ((values.size() & 1U) != 0U) {
    return values[mid];
  }
  return 0.5 * (values[mid - 1] + values[mid]);
}

template <typename Fn>
static double tensor_measure_median_ms(int iters, int warmup, Fn &&fn) {
  using Clock = std::chrono::steady_clock;
  using Ms = std::chrono::duration<double, std::milli>;

  for (int i = 0; i < warmup; ++i) {
    fn();
  }

  std::vector<double> samples;
  samples.reserve((size_t)iters);
  for (int i = 0; i < iters; ++i) {
    const auto t0 = Clock::now();
    fn();
    const auto t1 = Clock::now();
    samples.push_back(Ms(t1 - t0).count());
  }
  return tensor_median_ms(samples);
}

TEST(TensorExecHintGpuPreferProducesValidResultWithFallback) {
  int32_t shape[2] = {2, 2};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr);

  a->data[0] = 1.0f;
  a->data[1] = 2.0f;
  a->data[2] = 3.0f;
  a->data[3] = 4.0f;
  b->data[0] = 5.0f;
  b->data[1] = 6.0f;
  b->data[2] = 7.0f;
  b->data[3] = 8.0f;

  NeuronTensor *sum =
      neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(sum != nullptr);
  ASSERT_TRUE(std::fabs(sum->data[0] - 6.0f) < 1e-6f);
  ASSERT_TRUE(std::fabs(sum->data[1] - 8.0f) < 1e-6f);
  ASSERT_TRUE(std::fabs(sum->data[2] - 10.0f) < 1e-6f);
  ASSERT_TRUE(std::fabs(sum->data[3] - 12.0f) < 1e-6f);

  if (!neuron_gpu_supports_op(NEURON_GPU_OP_TENSOR_ADD)) {
    ASSERT_TRUE(sum != nullptr);
  }

  neuron_tensor_free(sum);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorExecHintCpuOnlyMatchesDefaultPath) {
  int32_t shape[2] = {2, 2};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  NeuronTensor *c = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr);

  fill_tensor_sequential(a, 0.25f, 0.5f);
  fill_tensor_sequential(b, -0.3f, 0.2f);
  fill_tensor_sequential(c, 0.7f, -0.1f);

  NeuronTensor *cpuOnly =
      neuron_tensor_fma_ex(a, b, c, NEURON_TENSOR_EXEC_CPU_ONLY);
  NeuronTensor *autoPath = neuron_tensor_fma(a, b, c);
  ASSERT_TRUE(cpuOnly != nullptr && autoPath != nullptr);
  ASSERT_EQ(cpuOnly->size, autoPath->size);

  for (int32_t i = 0; i < cpuOnly->size; ++i) {
    ASSERT_TRUE(std::fabs(cpuOnly->data[i] - autoPath->data[i]) < 1e-6f);
  }

  neuron_tensor_free(autoPath);
  neuron_tensor_free(cpuOnly);
  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorGpuPreferVulkanPathProducesCorrectResult) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  neuron_gpu_reset();

  int32_t shape[2] = {2, 2};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  NeuronTensor *c = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr);

  a->data[0] = 1.0f;
  a->data[1] = 2.0f;
  a->data[2] = 3.0f;
  a->data[3] = 4.0f;

  b->data[0] = 5.0f;
  b->data[1] = 6.0f;
  b->data[2] = 7.0f;
  b->data[3] = 8.0f;

  c->data[0] = 1.0f;
  c->data[1] = 1.0f;
  c->data[2] = 1.0f;
  c->data[3] = 1.0f;

  NeuronTensor *mul =
      neuron_tensor_mul_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *fma =
      neuron_tensor_fma_ex(a, b, c, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(mul != nullptr && fma != nullptr);

  ASSERT_TRUE(std::fabs(mul->data[0] - 5.0f) < 1e-5f);
  ASSERT_TRUE(std::fabs(mul->data[1] - 12.0f) < 1e-5f);
  ASSERT_TRUE(std::fabs(mul->data[2] - 21.0f) < 1e-5f);
  ASSERT_TRUE(std::fabs(mul->data[3] - 32.0f) < 1e-5f);

  ASSERT_TRUE(std::fabs(fma->data[0] - 6.0f) < 1e-5f);
  ASSERT_TRUE(std::fabs(fma->data[1] - 13.0f) < 1e-5f);
  ASSERT_TRUE(std::fabs(fma->data[2] - 22.0f) < 1e-5f);
  ASSERT_TRUE(std::fabs(fma->data[3] - 33.0f) < 1e-5f);

  neuron_tensor_free(fma);
  neuron_tensor_free(mul);
  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorGpuPreferFallsBackWhenUnavailable) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "cpu");
  neuron_gpu_reset();

  ASSERT_EQ(neuron_gpu_backend(), NEURON_GPU_BACKEND_CPU_FALLBACK);

  int32_t shape[2] = {2, 2};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr);

  fill_tensor_sequential(a, 0.5f, 0.25f);
  fill_tensor_sequential(b, 1.0f, -0.1f);

  NeuronTensor *gpuPrefer =
      neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *cpuOnly =
      neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(gpuPrefer != nullptr && cpuOnly != nullptr);

  for (int32_t i = 0; i < gpuPrefer->size; ++i) {
    ASSERT_TRUE(std::fabs(gpuPrefer->data[i] - cpuOnly->data[i]) < 1e-6f);
  }

  neuron_tensor_free(cpuOnly);
  neuron_tensor_free(gpuPrefer);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorGpuPreferCudaPathProducesCorrectResult) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "cuda");
  neuron_gpu_reset();

  const NeuronGpuBackend backend = neuron_gpu_backend();
  ASSERT_TRUE(backend == NEURON_GPU_BACKEND_CUDA_DRIVER ||
              backend == NEURON_GPU_BACKEND_CPU_FALLBACK);

  int32_t shape[2] = {2, 2};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  NeuronTensor *c = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr);

  a->data[0] = 1.0f;
  a->data[1] = 2.0f;
  a->data[2] = 3.0f;
  a->data[3] = 4.0f;

  b->data[0] = 5.0f;
  b->data[1] = 6.0f;
  b->data[2] = 7.0f;
  b->data[3] = 8.0f;

  c->data[0] = 1.0f;
  c->data[1] = 1.0f;
  c->data[2] = 1.0f;
  c->data[3] = 1.0f;

  NeuronTensor *mul =
      neuron_tensor_mul_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *fma =
      neuron_tensor_fma_ex(a, b, c, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(mul != nullptr && fma != nullptr);

  ASSERT_TRUE(std::fabs(mul->data[0] - 5.0f) < 1e-5f);
  ASSERT_TRUE(std::fabs(mul->data[1] - 12.0f) < 1e-5f);
  ASSERT_TRUE(std::fabs(mul->data[2] - 21.0f) < 1e-5f);
  ASSERT_TRUE(std::fabs(mul->data[3] - 32.0f) < 1e-5f);

  ASSERT_TRUE(std::fabs(fma->data[0] - 6.0f) < 1e-5f);
  ASSERT_TRUE(std::fabs(fma->data[1] - 13.0f) < 1e-5f);
  ASSERT_TRUE(std::fabs(fma->data[2] - 22.0f) < 1e-5f);
  ASSERT_TRUE(std::fabs(fma->data[3] - 33.0f) < 1e-5f);

  neuron_tensor_free(fma);
  neuron_tensor_free(mul);
  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorGpuPreferCudaFallsBackToVulkanOrCpu) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "cuda");
  neuron_gpu_reset();

  int32_t shape[2] = {2, 2};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr);

  fill_tensor_sequential(a, 0.3f, 0.2f);
  fill_tensor_sequential(b, 1.1f, -0.15f);

  NeuronTensor *gpuPrefer =
      neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *cpuOnly =
      neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(gpuPrefer != nullptr && cpuOnly != nullptr);

  for (int32_t i = 0; i < gpuPrefer->size; ++i) {
    ASSERT_TRUE(std::fabs(gpuPrefer->data[i] - cpuOnly->data[i]) < 1e-6f);
  }

  neuron_tensor_free(cpuOnly);
  neuron_tensor_free(gpuPrefer);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorGpuPreferMatMulCudaPathProducesCorrectResult) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "cuda");
  neuron_gpu_reset();

  int32_t aShape[2] = {3, 4};
  int32_t bShape[2] = {4, 2};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  ASSERT_TRUE(a != nullptr && b != nullptr);
  fill_tensor_sequential(a, -0.2f, 0.15f);
  fill_tensor_sequential(b, 0.35f, -0.07f);

  NeuronTensor *gpuPrefer = neuron_tensor_matmul_ex_hint(
      a, b, nullptr, 0, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *cpuOnly = neuron_tensor_matmul_ex_hint(
      a, b, nullptr, 0, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(gpuPrefer != nullptr && cpuOnly != nullptr);
  ASSERT_EQ(gpuPrefer->size, cpuOnly->size);
  for (int32_t i = 0; i < gpuPrefer->size; ++i) {
    ASSERT_TRUE(std::fabs(gpuPrefer->data[i] - cpuOnly->data[i]) < 1e-3f);
  }

  neuron_tensor_free(cpuOnly);
  neuron_tensor_free(gpuPrefer);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorGpuPreferMatMulVulkanPathProducesCorrectResult) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  neuron_gpu_reset();

  int32_t aShape[2] = {3, 4};
  int32_t bShape[2] = {4, 2};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  ASSERT_TRUE(a != nullptr && b != nullptr);
  fill_tensor_sequential(a, 0.1f, 0.11f);
  fill_tensor_sequential(b, -0.4f, 0.06f);

  NeuronTensor *gpuPrefer = neuron_tensor_matmul_ex_hint(
      a, b, nullptr, 0, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *cpuOnly = neuron_tensor_matmul_ex_hint(
      a, b, nullptr, 0, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(gpuPrefer != nullptr && cpuOnly != nullptr);
  ASSERT_EQ(gpuPrefer->size, cpuOnly->size);
  for (int32_t i = 0; i < gpuPrefer->size; ++i) {
    ASSERT_TRUE(std::fabs(gpuPrefer->data[i] - cpuOnly->data[i]) < 1e-3f);
  }

  neuron_tensor_free(cpuOnly);
  neuron_tensor_free(gpuPrefer);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorGpuPreferMatMulFallsBackToCpuCorrectly) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "cpu");
  neuron_gpu_reset();
  ASSERT_EQ(neuron_gpu_backend(), NEURON_GPU_BACKEND_CPU_FALLBACK);

  int32_t aShape[2] = {2, 3};
  int32_t bShape[2] = {3, 2};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  ASSERT_TRUE(a != nullptr && b != nullptr);
  fill_tensor_sequential(a, 0.2f, 0.08f);
  fill_tensor_sequential(b, -0.1f, 0.05f);

  NeuronTensor *gpuPrefer = neuron_tensor_matmul_ex_hint(
      a, b, nullptr, 0, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *cpuOnly = neuron_tensor_matmul_ex_hint(
      a, b, nullptr, 0, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(gpuPrefer != nullptr && cpuOnly != nullptr);
  for (int32_t i = 0; i < gpuPrefer->size; ++i) {
    ASSERT_TRUE(std::fabs(gpuPrefer->data[i] - cpuOnly->data[i]) < 1e-6f);
  }

  neuron_tensor_free(cpuOnly);
  neuron_tensor_free(gpuPrefer);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorGpuPreferMatMulAddAndLinearFusedCorrectResult) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  neuron_gpu_reset();

  int32_t aShape[2] = {2, 3};
  int32_t bShape[2] = {3, 4};
  int32_t biasShape[2] = {1, 4};
  int32_t residualShape[2] = {2, 4};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  NeuronTensor *bias = neuron_tensor_create(2, biasShape);
  NeuronTensor *residual = neuron_tensor_create(2, residualShape);
  ASSERT_TRUE(a != nullptr && b != nullptr && bias != nullptr &&
              residual != nullptr);
  fill_tensor_sequential(a, -0.2f, 0.17f);
  fill_tensor_sequential(b, 0.25f, -0.03f);
  fill_tensor_sequential(bias, 0.1f, 0.01f);
  fill_tensor_sequential(residual, -0.05f, 0.02f);

  NeuronTensor *addGpu =
      neuron_tensor_matmul_add_ex_hint(a, b, bias, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *addCpu =
      neuron_tensor_matmul_add_ex_hint(a, b, bias, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(addGpu != nullptr && addCpu != nullptr);
  for (int32_t i = 0; i < addGpu->size; ++i) {
    ASSERT_TRUE(std::fabs(addGpu->data[i] - addCpu->data[i]) < 1e-3f);
  }

  NeuronTensor *fusedGpu = neuron_tensor_linear_fused_ex_hint(
      a, b, nullptr, bias, residual, NEURON_TENSOR_ACTIVATION_RELU, nullptr, 0,
      NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *fusedCpu = neuron_tensor_linear_fused_ex_hint(
      a, b, nullptr, bias, residual, NEURON_TENSOR_ACTIVATION_RELU, nullptr, 0,
      NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(fusedGpu != nullptr && fusedCpu != nullptr);
  for (int32_t i = 0; i < fusedGpu->size; ++i) {
    ASSERT_TRUE(std::fabs(fusedGpu->data[i] - fusedCpu->data[i]) < 1e-3f);
  }

  neuron_tensor_free(fusedCpu);
  neuron_tensor_free(fusedGpu);
  neuron_tensor_free(addCpu);
  neuron_tensor_free(addGpu);
  neuron_tensor_free(residual);
  neuron_tensor_free(bias);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorGpuPreferLinearFusedGeluCorrectResult) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  neuron_gpu_reset();

  int32_t aShape[2] = {2, 3};
  int32_t bShape[2] = {3, 4};
  int32_t biasShape[2] = {1, 4};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  NeuronTensor *bias = neuron_tensor_create(2, biasShape);
  ASSERT_TRUE(a != nullptr && b != nullptr && bias != nullptr);
  fill_tensor_sequential(a, -0.4f, 0.14f);
  fill_tensor_sequential(b, 0.3f, -0.04f);
  fill_tensor_sequential(bias, -0.05f, 0.02f);

  NeuronTensor *geluGpu = neuron_tensor_linear_fused_ex_hint(
      a, b, nullptr, bias, nullptr, NEURON_TENSOR_ACTIVATION_GELU, nullptr, 0,
      NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *geluCpu = neuron_tensor_linear_fused_ex_hint(
      a, b, nullptr, bias, nullptr, NEURON_TENSOR_ACTIVATION_GELU, nullptr, 0,
      NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(geluGpu != nullptr && geluCpu != nullptr);
  for (int32_t i = 0; i < geluGpu->size; ++i) {
    ASSERT_TRUE(std::fabs(geluGpu->data[i] - geluCpu->data[i]) < 1e-3f);
  }

  neuron_tensor_free(geluCpu);
  neuron_tensor_free(geluGpu);
  neuron_tensor_free(bias);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorGpuPreferPackedMatMulUsesPackedPathAndMatchesReference) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  neuron_gpu_reset();

  int32_t aShape[2] = {4, 6};
  int32_t bShape[2] = {6, 5};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  ASSERT_TRUE(a != nullptr && b != nullptr);
  fill_tensor_sequential(a, -0.12f, 0.05f);
  fill_tensor_sequential(b, 0.21f, -0.02f);

  NeuronPackedMatrix *packed = nullptr;
  ASSERT_EQ(neuron_tensor_pack_b(b, &packed), 0);
  ASSERT_TRUE(packed != nullptr);

  NeuronTensor *gpuPrefer = neuron_tensor_linear_fused_ex_hint(
      a, b, packed, nullptr, nullptr, NEURON_TENSOR_ACTIVATION_NONE, nullptr, 0,
      NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *cpuOnly = neuron_tensor_linear_fused_ex_hint(
      a, b, packed, nullptr, nullptr, NEURON_TENSOR_ACTIVATION_NONE, nullptr, 0,
      NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(gpuPrefer != nullptr && cpuOnly != nullptr);
  ASSERT_EQ(gpuPrefer->size, cpuOnly->size);
  for (int32_t i = 0; i < gpuPrefer->size; ++i) {
    ASSERT_TRUE(std::fabs(gpuPrefer->data[i] - cpuOnly->data[i]) < 1e-3f);
  }

  neuron_tensor_free(cpuOnly);
  neuron_tensor_free(gpuPrefer);
  neuron_tensor_packed_free(packed);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorMatMulAutoRemainsCpuPolicy) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  neuron_gpu_reset();

  int32_t aShape[2] = {3, 3};
  int32_t bShape[2] = {3, 2};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  ASSERT_TRUE(a != nullptr && b != nullptr);
  fill_tensor_sequential(a, 0.05f, 0.03f);
  fill_tensor_sequential(b, -0.2f, 0.04f);

  NeuronTensor *autoPath =
      neuron_tensor_matmul_ex_hint(a, b, nullptr, 0, NEURON_TENSOR_EXEC_AUTO);
  NeuronTensor *cpuOnly =
      neuron_tensor_matmul_ex_hint(a, b, nullptr, 0, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(autoPath != nullptr && cpuOnly != nullptr);
  for (int32_t i = 0; i < autoPath->size; ++i) {
    ASSERT_TRUE(std::fabs(autoPath->data[i] - cpuOnly->data[i]) < 1e-6f);
  }

  neuron_tensor_free(cpuOnly);
  neuron_tensor_free(autoPath);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(GpuBlockIfWithCpuReadFlushesCorrectly) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  neuron_gpu_reset();

  int32_t shape[2] = {1, 8};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  NeuronTensor *c = neuron_tensor_create(2, shape);
  NeuronTensor *d = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr && d != nullptr);
  fill_tensor_sequential(a, 0.2f, 0.05f);
  fill_tensor_sequential(b, -0.1f, 0.07f);
  fill_tensor_sequential(c, 0.8f, -0.03f);
  fill_tensor_sequential(d, -0.4f, 0.02f);

  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  NeuronTensor *xGpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(xGpu != nullptr);
  const bool gpuBranch = xGpu->data[0] > xGpu->data[1];
  NeuronTensor *outGpu =
      gpuBranch ? neuron_tensor_mul_ex(xGpu, c, NEURON_TENSOR_EXEC_GPU_PREFER)
                : neuron_tensor_sub_ex(xGpu, d, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(outGpu != nullptr);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);

  NeuronTensor *xCpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(xCpu != nullptr);
  const bool cpuBranch = xCpu->data[0] > xCpu->data[1];
  NeuronTensor *outCpu =
      cpuBranch ? neuron_tensor_mul_ex(xCpu, c, NEURON_TENSOR_EXEC_CPU_ONLY)
                : neuron_tensor_sub_ex(xCpu, d, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(outCpu != nullptr);

  ASSERT_EQ(gpuBranch, cpuBranch);
  ASSERT_TRUE(tensor_close(outGpu, outCpu, 1e-4f));

  neuron_tensor_free(outCpu);
  neuron_tensor_free(xCpu);
  neuron_tensor_free(outGpu);
  neuron_tensor_free(xGpu);
  neuron_tensor_free(d);
  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(GpuBlockEarlyReturnScopeEndGuaranteed) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  neuron_gpu_reset();

  int32_t shape[2] = {1, 16};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  NeuronTensor *c = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr);
  fill_tensor_sequential(a, 0.05f, 0.01f);
  fill_tensor_sequential(b, -0.2f, 0.015f);
  fill_tensor_sequential(c, 0.4f, -0.01f);

  float gpuAccum = 0.0f;
  float cpuAccum = 0.0f;
  for (int i = 0; i < 32; ++i) {
    ASSERT_EQ(neuron_gpu_scope_begin(), 0);
    NeuronTensor *xGpu =
        neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
    ASSERT_TRUE(xGpu != nullptr);

    if ((i % 3) == 0) {
      gpuAccum += xGpu->data[i % xGpu->size];
      ASSERT_EQ(neuron_gpu_scope_end(), 0);
      neuron_tensor_free(xGpu);

      NeuronTensor *xCpu =
          neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
      ASSERT_TRUE(xCpu != nullptr);
      cpuAccum += xCpu->data[i % xCpu->size];
      neuron_tensor_free(xCpu);
      continue;
    }

    NeuronTensor *yGpu =
        neuron_tensor_fma_ex(xGpu, c, a, NEURON_TENSOR_EXEC_GPU_PREFER);
    ASSERT_TRUE(yGpu != nullptr);
    gpuAccum += yGpu->data[(i * 5) % yGpu->size];
    ASSERT_EQ(neuron_gpu_scope_end(), 0);

    NeuronTensor *xCpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
    NeuronTensor *yCpu = neuron_tensor_fma_ex(xCpu, c, a, NEURON_TENSOR_EXEC_CPU_ONLY);
    ASSERT_TRUE(xCpu != nullptr && yCpu != nullptr);
    cpuAccum += yCpu->data[(i * 5) % yCpu->size];

    neuron_tensor_free(yCpu);
    neuron_tensor_free(xCpu);
    neuron_tensor_free(yGpu);
    neuron_tensor_free(xGpu);
  }

  ASSERT_TRUE(std::fabs(gpuAccum - cpuAccum) < 1e-3f);
  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);

  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(GpuBlockLongLoopChunkFlushNoOOM) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  neuron_gpu_reset();

  int32_t shape[2] = {1, 256};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  NeuronTensor *c = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr);
  fill_tensor_sequential(a, 0.01f, 0.001f);
  fill_tensor_sequential(b, 0.5f, -0.0007f);
  fill_tensor_sequential(c, -0.3f, 0.0009f);

  NeuronGpuMemoryStats before = {0};
  NeuronGpuMemoryStats after = {0};
  neuron_gpu_get_memory_stats(&before);

  float gpuAccum = 0.0f;
  float cpuAccum = 0.0f;
  for (int i = 0; i < 160; ++i) {
    ASSERT_EQ(neuron_gpu_scope_begin(), 0);
    NeuronTensor *xGpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
    ASSERT_TRUE(xGpu != nullptr);
    NeuronTensor *yGpu = neuron_tensor_mul_ex(xGpu, c, NEURON_TENSOR_EXEC_GPU_PREFER);
    ASSERT_TRUE(yGpu != nullptr);
    NeuronTensor *zGpu = ((i % 4) == 0)
                             ? neuron_tensor_fma_ex(yGpu, a, b,
                                                    NEURON_TENSOR_EXEC_GPU_PREFER)
                             : neuron_tensor_sub_ex(yGpu, a,
                                                    NEURON_TENSOR_EXEC_GPU_PREFER);
    ASSERT_TRUE(zGpu != nullptr);
    ASSERT_EQ(neuron_gpu_scope_end(), 0);
    gpuAccum += zGpu->data[i % zGpu->size];

    NeuronTensor *xCpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
    ASSERT_TRUE(xCpu != nullptr);
    NeuronTensor *yCpu = neuron_tensor_mul_ex(xCpu, c, NEURON_TENSOR_EXEC_CPU_ONLY);
    ASSERT_TRUE(yCpu != nullptr);
    NeuronTensor *zCpu = ((i % 4) == 0)
                             ? neuron_tensor_fma_ex(yCpu, a, b,
                                                    NEURON_TENSOR_EXEC_CPU_ONLY)
                             : neuron_tensor_sub_ex(yCpu, a,
                                                    NEURON_TENSOR_EXEC_CPU_ONLY);
    ASSERT_TRUE(zCpu != nullptr);
    cpuAccum += zCpu->data[i % zCpu->size];

    neuron_tensor_free(zCpu);
    neuron_tensor_free(yCpu);
    neuron_tensor_free(xCpu);
    neuron_tensor_free(zGpu);
    neuron_tensor_free(yGpu);
    neuron_tensor_free(xGpu);
  }

  neuron_gpu_get_memory_stats(&after);
  ASSERT_TRUE(std::fabs(gpuAccum - cpuAccum) < 1e-2f);
  ASSERT_EQ(after.total_allocations, before.total_allocations);

  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(GpuBlockDependentOpsBarrierCorrect) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  neuron_gpu_reset();

  int32_t aShape[2] = {16, 16};
  int32_t bShape[2] = {16, 16};
  int32_t cShape[2] = {16, 16};
  int32_t dShape[2] = {16, 16};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  NeuronTensor *c = neuron_tensor_create(2, cShape);
  NeuronTensor *d = neuron_tensor_create(2, dShape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr && d != nullptr);
  fill_tensor_sequential(a, 0.03f, 0.002f);
  fill_tensor_sequential(b, -0.02f, 0.001f);
  fill_tensor_sequential(c, 0.01f, 0.003f);
  fill_tensor_sequential(d, -0.15f, 0.0008f);

  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  NeuronTensor *xGpu = neuron_tensor_matmul_ex_hint(
      a, b, nullptr, 0, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(xGpu != nullptr);
  NeuronTensor *yGpu = neuron_tensor_matmul_ex_hint(
      xGpu, c, nullptr, 0, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(yGpu != nullptr);
  NeuronTensor *zGpu = neuron_tensor_add_ex(yGpu, d, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(zGpu != nullptr);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);

  NeuronTensor *xCpu = neuron_tensor_matmul_ex_hint(
      a, b, nullptr, 0, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(xCpu != nullptr);
  NeuronTensor *yCpu = neuron_tensor_matmul_ex_hint(
      xCpu, c, nullptr, 0, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(yCpu != nullptr);
  NeuronTensor *zCpu = neuron_tensor_add_ex(yCpu, d, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(zCpu != nullptr);

  ASSERT_TRUE(tensor_close(zGpu, zCpu, 2e-3f));

  neuron_tensor_free(zCpu);
  neuron_tensor_free(yCpu);
  neuron_tensor_free(xCpu);
  neuron_tensor_free(zGpu);
  neuron_tensor_free(yGpu);
  neuron_tensor_free(xGpu);
  neuron_tensor_free(d);
  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(GpuBlockNestedIfForHybridMatchesCpuReference) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  neuron_gpu_reset();

  int32_t vecShape[2] = {1, 64};
  int32_t aShape[2] = {8, 16};
  int32_t bShape[2] = {16, 8};
  NeuronTensor *a = neuron_tensor_create(2, vecShape);
  NeuronTensor *b = neuron_tensor_create(2, vecShape);
  NeuronTensor *c = neuron_tensor_create(2, vecShape);
  NeuronTensor *d = neuron_tensor_create(2, vecShape);
  NeuronTensor *e = neuron_tensor_create(2, vecShape);
  NeuronTensor *matA = neuron_tensor_create(2, aShape);
  NeuronTensor *matB = neuron_tensor_create(2, bShape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr && d != nullptr &&
              e != nullptr && matA != nullptr && matB != nullptr);

  fill_tensor_sequential(a, 0.05f, 0.003f);
  fill_tensor_sequential(b, -0.02f, 0.002f);
  fill_tensor_sequential(c, 0.30f, -0.001f);
  fill_tensor_sequential(d, -0.15f, 0.0015f);
  fill_tensor_sequential(e, 0.80f, -0.0025f);
  fill_tensor_sequential(matA, 0.01f, 0.0008f);
  fill_tensor_sequential(matB, -0.03f, 0.0009f);

  float gpuAccum = 0.0f;
  float cpuAccum = 0.0f;
  for (int outer = 0; outer < 24; ++outer) {
    ASSERT_EQ(neuron_gpu_scope_begin(), 0);
    NeuronTensor *workGpu =
        neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
    ASSERT_TRUE(workGpu != nullptr);

    for (int inner = 0; inner < 6; ++inner) {
      NeuronTensor *nextGpu = ((outer + inner) % 3 == 0)
                                  ? neuron_tensor_fma_ex(
                                        workGpu, c, d, NEURON_TENSOR_EXEC_GPU_PREFER)
                                  : neuron_tensor_mul_ex(
                                        workGpu, e, NEURON_TENSOR_EXEC_GPU_PREFER);
      ASSERT_TRUE(nextGpu != nullptr);
      neuron_tensor_free(workGpu);
      workGpu = nextGpu;

      if ((inner & 1) == 0) {
        NeuronTensor *cpuPeek =
            neuron_tensor_add_ex(workGpu, a, NEURON_TENSOR_EXEC_CPU_ONLY);
        ASSERT_TRUE(cpuPeek != nullptr);
        gpuAccum += cpuPeek->data[(outer + inner) % cpuPeek->size];
        neuron_tensor_free(cpuPeek);
      } else {
        NeuronTensor *mixGpu =
            neuron_tensor_sub_ex(workGpu, d, NEURON_TENSOR_EXEC_GPU_PREFER);
        ASSERT_TRUE(mixGpu != nullptr);
        gpuAccum += mixGpu->data[(outer + inner * 3) % mixGpu->size];
        neuron_tensor_free(mixGpu);
      }
    }

    if ((outer % 4) == 0) {
      NeuronTensor *mmGpu = neuron_tensor_matmul_ex_hint(
          matA, matB, nullptr, 0, NEURON_TENSOR_EXEC_GPU_PREFER);
      ASSERT_TRUE(mmGpu != nullptr);
      gpuAccum += mmGpu->data[outer % mmGpu->size];
      neuron_tensor_free(mmGpu);
    }

    ASSERT_EQ(neuron_gpu_scope_end(), 0);
    gpuAccum += workGpu->data[outer % workGpu->size];
    neuron_tensor_free(workGpu);

    NeuronTensor *workCpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
    ASSERT_TRUE(workCpu != nullptr);
    for (int inner = 0; inner < 6; ++inner) {
      NeuronTensor *nextCpu = ((outer + inner) % 3 == 0)
                                  ? neuron_tensor_fma_ex(
                                        workCpu, c, d, NEURON_TENSOR_EXEC_CPU_ONLY)
                                  : neuron_tensor_mul_ex(
                                        workCpu, e, NEURON_TENSOR_EXEC_CPU_ONLY);
      ASSERT_TRUE(nextCpu != nullptr);
      neuron_tensor_free(workCpu);
      workCpu = nextCpu;

      if ((inner & 1) == 0) {
        NeuronTensor *cpuPeek =
            neuron_tensor_add_ex(workCpu, a, NEURON_TENSOR_EXEC_CPU_ONLY);
        ASSERT_TRUE(cpuPeek != nullptr);
        cpuAccum += cpuPeek->data[(outer + inner) % cpuPeek->size];
        neuron_tensor_free(cpuPeek);
      } else {
        NeuronTensor *mixCpu =
            neuron_tensor_sub_ex(workCpu, d, NEURON_TENSOR_EXEC_CPU_ONLY);
        ASSERT_TRUE(mixCpu != nullptr);
        cpuAccum += mixCpu->data[(outer + inner * 3) % mixCpu->size];
        neuron_tensor_free(mixCpu);
      }
    }
    if ((outer % 4) == 0) {
      NeuronTensor *mmCpu = neuron_tensor_matmul_ex_hint(
          matA, matB, nullptr, 0, NEURON_TENSOR_EXEC_CPU_ONLY);
      ASSERT_TRUE(mmCpu != nullptr);
      cpuAccum += mmCpu->data[outer % mmCpu->size];
      neuron_tensor_free(mmCpu);
    }
    cpuAccum += workCpu->data[outer % workCpu->size];
    neuron_tensor_free(workCpu);
  }

  ASSERT_TRUE(std::fabs(gpuAccum - cpuAccum) < 2e-2f);

  neuron_tensor_free(matB);
  neuron_tensor_free(matA);
  neuron_tensor_free(e);
  neuron_tensor_free(d);
  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(GpuBlockForContinueBreakHybridFlushesCorrectly) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  neuron_gpu_reset();

  int32_t shape[2] = {1, 96};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  NeuronTensor *c = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr);

  fill_tensor_sequential(a, 0.02f, 0.001f);
  fill_tensor_sequential(b, -0.08f, 0.0007f);
  fill_tensor_sequential(c, 0.4f, -0.0009f);

  float gpuAccum = 0.0f;
  float cpuAccum = 0.0f;

  for (int i = 0; i < 60; ++i) {
    ASSERT_EQ(neuron_gpu_scope_begin(), 0);
    NeuronTensor *xGpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
    ASSERT_TRUE(xGpu != nullptr);

    if ((i % 7) == 0) {
      gpuAccum += xGpu->data[i % xGpu->size];
      ASSERT_EQ(neuron_gpu_scope_end(), 0);
      neuron_tensor_free(xGpu);

      NeuronTensor *xCpu =
          neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
      ASSERT_TRUE(xCpu != nullptr);
      cpuAccum += xCpu->data[i % xCpu->size];
      neuron_tensor_free(xCpu);
      continue;
    }

    NeuronTensor *yGpu = ((i % 2) == 0)
                             ? neuron_tensor_mul_ex(
                                   xGpu, c, NEURON_TENSOR_EXEC_GPU_PREFER)
                             : neuron_tensor_sub_ex(
                                   xGpu, c, NEURON_TENSOR_EXEC_GPU_PREFER);
    ASSERT_TRUE(yGpu != nullptr);

    NeuronTensor *hybridCpu =
        neuron_tensor_add_ex(yGpu, b, NEURON_TENSOR_EXEC_CPU_ONLY);
    ASSERT_TRUE(hybridCpu != nullptr);
    gpuAccum += hybridCpu->data[(i * 5) % hybridCpu->size];
    ASSERT_EQ(neuron_gpu_scope_end(), 0);

    NeuronTensor *xCpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
    ASSERT_TRUE(xCpu != nullptr);
    NeuronTensor *yCpu = ((i % 2) == 0)
                             ? neuron_tensor_mul_ex(
                                   xCpu, c, NEURON_TENSOR_EXEC_CPU_ONLY)
                             : neuron_tensor_sub_ex(
                                   xCpu, c, NEURON_TENSOR_EXEC_CPU_ONLY);
    ASSERT_TRUE(yCpu != nullptr);
    NeuronTensor *hybridRef =
        neuron_tensor_add_ex(yCpu, b, NEURON_TENSOR_EXEC_CPU_ONLY);
    ASSERT_TRUE(hybridRef != nullptr);
    cpuAccum += hybridRef->data[(i * 5) % hybridRef->size];

    neuron_tensor_free(hybridRef);
    neuron_tensor_free(yCpu);
    neuron_tensor_free(xCpu);
    neuron_tensor_free(hybridCpu);
    neuron_tensor_free(yGpu);
    neuron_tensor_free(xGpu);

    if (i == 47) {
      break;
    }
  }

  ASSERT_TRUE(std::fabs(gpuAccum - cpuAccum) < 1e-2f);

  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(GpuBlockHybridMatMulAutoPolicyStaysCpuInScope) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  neuron_gpu_reset();

  int32_t aShape[2] = {12, 10};
  int32_t bShape[2] = {10, 6};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  ASSERT_TRUE(a != nullptr && b != nullptr);
  fill_tensor_sequential(a, -0.02f, 0.002f);
  fill_tensor_sequential(b, 0.04f, -0.001f);

  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  NeuronTensor *gpuPrefer = neuron_tensor_matmul_ex_hint(
      a, b, nullptr, 0, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *autoPath =
      neuron_tensor_matmul_ex_hint(a, b, nullptr, 0, NEURON_TENSOR_EXEC_AUTO);
  NeuronTensor *cpuOnly = neuron_tensor_matmul_ex_hint(
      a, b, nullptr, 0, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);

  ASSERT_TRUE(gpuPrefer != nullptr && autoPath != nullptr && cpuOnly != nullptr);
  ASSERT_TRUE(tensor_close(autoPath, cpuOnly, 1e-6f));
  ASSERT_TRUE(tensor_close(gpuPrefer, cpuOnly, 2e-3f));

  neuron_tensor_free(cpuOnly);
  neuron_tensor_free(autoPath);
  neuron_tensor_free(gpuPrefer);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(GpuBlockIfForHybridBenchmarkPrintsMetrics) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  ScopedEnvOverride scopeBatch("NEURON_GPU_SCOPE_BATCH", "0");
  neuron_gpu_reset();

  int32_t shape[2] = {1, 65536};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  NeuronTensor *c = neuron_tensor_create(2, shape);
  NeuronTensor *d = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr && d != nullptr);
  fill_tensor_sequential(a, 0.1f, 0.00001f);
  fill_tensor_sequential(b, -0.2f, 0.00002f);
  fill_tensor_sequential(c, 0.3f, -0.00001f);
  fill_tensor_sequential(d, -0.1f, 0.00003f);

  auto runScenario = [&](NeuronTensorExecHint hint, int useScope) -> float {
    float checksum = 0.0f;
    for (int i = 0; i < 12; ++i) {
      if (useScope && neuron_gpu_scope_begin() != 0) {
        return NAN;
      }
      NeuronTensor *x = neuron_tensor_add_ex(a, b, hint);
      if (x == nullptr) {
        if (useScope) {
          (void)neuron_gpu_scope_end();
        }
        return NAN;
      }
      NeuronTensor *y = neuron_tensor_mul_ex(x, c, hint);
      NeuronTensor *z = neuron_tensor_fma_ex(y, c, d, hint);
      if (y == nullptr || z == nullptr) {
        neuron_tensor_free(z);
        neuron_tensor_free(y);
        neuron_tensor_free(x);
        if (useScope) {
          (void)neuron_gpu_scope_end();
        }
        return NAN;
      }
      if (useScope && neuron_gpu_scope_end() != 0) {
        neuron_tensor_free(z);
        neuron_tensor_free(y);
        neuron_tensor_free(x);
        return NAN;
      }
      checksum += z->data[(i * 19) % z->size];
      neuron_tensor_free(z);
      neuron_tensor_free(y);
      neuron_tensor_free(x);
    }
    return checksum;
  };

  const double cpuMs = tensor_measure_median_ms(4, 1, [&]() {
    volatile float sink = runScenario(NEURON_TENSOR_EXEC_CPU_ONLY, 0);
    (void)sink;
  });

  set_env_value("NEURON_GPU_SCOPE_BATCH", "0");
  neuron_gpu_reset();
  float gpuOffChecksum = 0.0f;
  const double gpuOffMs = tensor_measure_median_ms(4, 1, [&]() {
    volatile float sink = runScenario(NEURON_TENSOR_EXEC_GPU_PREFER, 1);
    gpuOffChecksum = sink;
    (void)sink;
  });

  set_env_value("NEURON_GPU_SCOPE_BATCH", "1");
  set_env_value("NEURON_GPU_SCOPE_FUSION", "0");
  neuron_gpu_reset();
  float gpuOnChecksum = 0.0f;
  const double gpuOnMs = tensor_measure_median_ms(4, 1, [&]() {
    volatile float sink = runScenario(NEURON_TENSOR_EXEC_GPU_PREFER, 1);
    gpuOnChecksum = sink;
    (void)sink;
  });

  set_env_value("NEURON_GPU_SCOPE_BATCH", "1");
  set_env_value("NEURON_GPU_SCOPE_FUSION", "1");
  set_env_value("NEURON_GPU_SCOPE_READBACK_SINK_ONLY", "0");
  neuron_gpu_reset();
  float gpuOnFusionChecksum = 0.0f;
  const double gpuOnFusionMs = tensor_measure_median_ms(4, 1, [&]() {
    volatile float sink = runScenario(NEURON_TENSOR_EXEC_GPU_PREFER, 1);
    gpuOnFusionChecksum = sink;
    (void)sink;
  });

  set_env_value("NEURON_GPU_SCOPE_BATCH", "1");
  set_env_value("NEURON_GPU_SCOPE_FUSION", "1");
  set_env_value("NEURON_GPU_SCOPE_READBACK_SINK_ONLY", "1");
  neuron_gpu_reset();
  float gpuOnFusionSinkChecksum = 0.0f;
  const double gpuOnFusionSinkMs = tensor_measure_median_ms(4, 1, [&]() {
    volatile float sink = runScenario(NEURON_TENSOR_EXEC_GPU_PREFER, 1);
    gpuOnFusionSinkChecksum = sink;
    (void)sink;
  });
  const float cpuChecksum = runScenario(NEURON_TENSOR_EXEC_CPU_ONLY, 0);

  ASSERT_TRUE(std::isfinite(cpuMs) && std::isfinite(gpuOffMs) &&
              std::isfinite(gpuOnMs) && std::isfinite(gpuOnFusionMs) &&
              std::isfinite(gpuOnFusionSinkMs));
  ASSERT_TRUE(std::fabs(gpuOffChecksum - cpuChecksum) < 5e-2f);
  ASSERT_TRUE(std::fabs(gpuOnChecksum - cpuChecksum) < 5e-2f);
  ASSERT_TRUE(std::fabs(gpuOnFusionChecksum - cpuChecksum) < 5e-2f);
  ASSERT_TRUE(std::fabs(gpuOnFusionSinkChecksum - cpuChecksum) < 5e-2f);
  std::printf(
      "[BENCH] gpu_scope_batch_simple cpu_ms=%.3f off_ms=%.3f on_ms=%.3f "
      "on_fusion_ms=%.3f on_fusion_sink_ms=%.3f "
      "off_over_on=%.3f on_over_fusion=%.3f fusion_over_sink=%.3f cpu_over_on=%.3f\n",
      cpuMs, gpuOffMs, gpuOnMs, gpuOnFusionMs, gpuOnFusionSinkMs,
      gpuOnMs > 0.0 ? (gpuOffMs / gpuOnMs) : 0.0,
      gpuOnFusionMs > 0.0 ? (gpuOnMs / gpuOnFusionMs) : 0.0,
      gpuOnFusionSinkMs > 0.0 ? (gpuOnFusionMs / gpuOnFusionSinkMs) : 0.0,
      gpuOnMs > 0.0 ? (cpuMs / gpuOnMs) : 0.0);

  set_env_value("NEURON_GPU_SCOPE_BATCH", "0");
  set_env_value("NEURON_GPU_SCOPE_FUSION", "0");
  set_env_value("NEURON_GPU_SCOPE_READBACK_SINK_ONLY", "0");
  neuron_gpu_reset();

  neuron_tensor_free(d);
  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(GpuBlockScopeBatchElementwiseChainMatchesCpuReference) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  ScopedEnvOverride scopeBatch("NEURON_GPU_SCOPE_BATCH", "1");
  neuron_gpu_reset();

  int32_t shape[2] = {1, 262144};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  NeuronTensor *c = neuron_tensor_create(2, shape);
  NeuronTensor *d = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr && d != nullptr);
  fill_tensor_sequential(a, 0.05f, 0.00001f);
  fill_tensor_sequential(b, -0.03f, 0.00002f);
  fill_tensor_sequential(c, 0.08f, -0.00001f);
  fill_tensor_sequential(d, 0.1f, -0.00002f);

  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  NeuronTensor *xGpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *yGpu = neuron_tensor_mul_ex(xGpu, c, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *zGpu =
      neuron_tensor_fma_ex(yGpu, a, d, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(xGpu != nullptr && yGpu != nullptr && zGpu != nullptr);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);

  NeuronTensor *xCpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
  NeuronTensor *yCpu = neuron_tensor_mul_ex(xCpu, c, NEURON_TENSOR_EXEC_CPU_ONLY);
  NeuronTensor *zCpu =
      neuron_tensor_fma_ex(yCpu, a, d, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(xCpu != nullptr && yCpu != nullptr && zCpu != nullptr);
  ASSERT_TRUE(tensor_close(zGpu, zCpu, 2e-4f));

  neuron_tensor_free(zCpu);
  neuron_tensor_free(yCpu);
  neuron_tensor_free(xCpu);
  neuron_tensor_free(zGpu);
  neuron_tensor_free(yGpu);
  neuron_tensor_free(xGpu);
  neuron_tensor_free(d);
  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(GpuBlockScopeFusionPreservesIntermediateAndFinalOutputs) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  ScopedEnvOverride scopeBatch("NEURON_GPU_SCOPE_BATCH", "1");
  ScopedEnvOverride scopeFusion("NEURON_GPU_SCOPE_FUSION", "1");
  ScopedEnvOverride sinkReadback("NEURON_GPU_SCOPE_READBACK_SINK_ONLY", "0");
  neuron_gpu_reset();

  int32_t shape[2] = {1, 65536};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  NeuronTensor *c = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr);
  fill_tensor_sequential(a, 0.03f, 0.00001f);
  fill_tensor_sequential(b, -0.02f, 0.00002f);
  fill_tensor_sequential(c, 0.08f, -0.00001f);

  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  NeuronTensor *xGpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *yGpu = neuron_tensor_mul_ex(xGpu, c, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(xGpu != nullptr && yGpu != nullptr);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);

  NeuronTensor *xCpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
  NeuronTensor *yCpu = neuron_tensor_mul_ex(xCpu, c, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(xCpu != nullptr && yCpu != nullptr);
  ASSERT_TRUE(tensor_close(xGpu, xCpu, 2e-4f));
  ASSERT_TRUE(tensor_close(yGpu, yCpu, 2e-4f));

  neuron_tensor_free(yCpu);
  neuron_tensor_free(xCpu);
  neuron_tensor_free(yGpu);
  neuron_tensor_free(xGpu);
  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(GpuBlockScopeBinaryThenFmaFusionMatchesCpuReference) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  ScopedEnvOverride scopeBatch("NEURON_GPU_SCOPE_BATCH", "1");
  ScopedEnvOverride scopeFusion("NEURON_GPU_SCOPE_FUSION", "1");
  ScopedEnvOverride sinkReadback("NEURON_GPU_SCOPE_READBACK_SINK_ONLY", "0");
  neuron_gpu_reset();

  int32_t shape[2] = {1, 65536};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  NeuronTensor *c = neuron_tensor_create(2, shape);
  NeuronTensor *d = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr && d != nullptr);
  fill_tensor_sequential(a, 0.01f, 0.00002f);
  fill_tensor_sequential(b, -0.05f, 0.00003f);
  fill_tensor_sequential(c, 0.12f, -0.00001f);
  fill_tensor_sequential(d, -0.02f, 0.00004f);

  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  NeuronTensor *xGpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *yGpu =
      neuron_tensor_fma_ex(xGpu, c, d, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(xGpu != nullptr && yGpu != nullptr);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);

  NeuronTensor *xCpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
  NeuronTensor *yCpu = neuron_tensor_fma_ex(xCpu, c, d, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(xCpu != nullptr && yCpu != nullptr);
  ASSERT_TRUE(tensor_close(xGpu, xCpu, 2e-4f));
  ASSERT_TRUE(tensor_close(yGpu, yCpu, 2e-4f));

  neuron_tensor_free(yCpu);
  neuron_tensor_free(xCpu);
  neuron_tensor_free(yGpu);
  neuron_tensor_free(xGpu);
  neuron_tensor_free(d);
  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(GpuBlockScopeBinaryChainThenFmaFusionMatchesCpuReference) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  ScopedEnvOverride scopeBatch("NEURON_GPU_SCOPE_BATCH", "1");
  ScopedEnvOverride scopeFusion("NEURON_GPU_SCOPE_FUSION", "1");
  ScopedEnvOverride sinkReadback("NEURON_GPU_SCOPE_READBACK_SINK_ONLY", "0");
  neuron_gpu_reset();

  int32_t shape[2] = {1, 65536};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  NeuronTensor *c = neuron_tensor_create(2, shape);
  NeuronTensor *d = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr && d != nullptr);
  fill_tensor_sequential(a, 0.04f, 0.00002f);
  fill_tensor_sequential(b, -0.07f, 0.00001f);
  fill_tensor_sequential(c, 0.12f, -0.00002f);
  fill_tensor_sequential(d, -0.01f, 0.00003f);

  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  NeuronTensor *xGpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *yGpu = neuron_tensor_mul_ex(xGpu, c, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *zGpu =
      neuron_tensor_fma_ex(yGpu, c, d, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(xGpu != nullptr && yGpu != nullptr && zGpu != nullptr);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);

  NeuronTensor *xCpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
  NeuronTensor *yCpu = neuron_tensor_mul_ex(xCpu, c, NEURON_TENSOR_EXEC_CPU_ONLY);
  NeuronTensor *zCpu = neuron_tensor_fma_ex(yCpu, c, d, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(xCpu != nullptr && yCpu != nullptr && zCpu != nullptr);
  ASSERT_TRUE(tensor_close(xGpu, xCpu, 2e-4f));
  ASSERT_TRUE(tensor_close(yGpu, yCpu, 2e-4f));
  ASSERT_TRUE(tensor_close(zGpu, zCpu, 2e-4f));

  neuron_tensor_free(zCpu);
  neuron_tensor_free(yCpu);
  neuron_tensor_free(xCpu);
  neuron_tensor_free(zGpu);
  neuron_tensor_free(yGpu);
  neuron_tensor_free(xGpu);
  neuron_tensor_free(d);
  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(GpuScopeInputCacheVerifyReflectsHostMutation) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  ScopedEnvOverride scopeBatch("NEURON_GPU_SCOPE_BATCH", "1");
  ScopedEnvOverride inputCache("NEURON_GPU_SCOPE_INPUT_CACHE", "1");
  ScopedEnvOverride inputCacheVerify("NEURON_GPU_SCOPE_INPUT_CACHE_VERIFY", "1");
  neuron_gpu_reset();

  int32_t shape[2] = {1, 1024};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr);
  fill_tensor_sequential(a, 0.5f, 0.001f);
  fill_tensor_sequential(b, -0.25f, 0.0005f);

  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  NeuronTensor *first = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(first != nullptr);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);

  for (int32_t i = 0; i < a->size; ++i) {
    a->data[i] += ((i & 1) == 0) ? 0.125f : -0.075f;
  }

  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  NeuronTensor *second = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(second != nullptr);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);

  NeuronTensor *cpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(cpu != nullptr);
  ASSERT_TRUE(tensor_close(second, cpu, 1e-5f));
  ASSERT_TRUE(!tensor_close(first, second, 1e-4f));

  neuron_tensor_free(cpu);
  neuron_tensor_free(second);
  neuron_tensor_free(first);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(GpuBlockScopeSinkReadbackFinalOutputMatchesCpu) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  ScopedEnvOverride scopeBatch("NEURON_GPU_SCOPE_BATCH", "1");
  ScopedEnvOverride sinkReadback("NEURON_GPU_SCOPE_READBACK_SINK_ONLY", "1");
  neuron_gpu_reset();

  int32_t shape[2] = {1, 262144};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  NeuronTensor *c = neuron_tensor_create(2, shape);
  NeuronTensor *d = neuron_tensor_create(2, shape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr && d != nullptr);
  fill_tensor_sequential(a, 0.11f, 0.00001f);
  fill_tensor_sequential(b, -0.07f, 0.00002f);
  fill_tensor_sequential(c, 0.09f, -0.00001f);
  fill_tensor_sequential(d, -0.03f, 0.00003f);

  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  NeuronTensor *xGpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *yGpu = neuron_tensor_mul_ex(xGpu, c, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *zGpu =
      neuron_tensor_fma_ex(yGpu, a, d, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(xGpu != nullptr && yGpu != nullptr && zGpu != nullptr);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);

  NeuronTensor *xCpu = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
  NeuronTensor *yCpu = neuron_tensor_mul_ex(xCpu, c, NEURON_TENSOR_EXEC_CPU_ONLY);
  NeuronTensor *zCpu =
      neuron_tensor_fma_ex(yCpu, a, d, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(xCpu != nullptr && yCpu != nullptr && zCpu != nullptr);
  ASSERT_TRUE(tensor_close(zGpu, zCpu, 2e-4f));

  neuron_tensor_free(zCpu);
  neuron_tensor_free(yCpu);
  neuron_tensor_free(xCpu);
  neuron_tensor_free(zGpu);
  neuron_tensor_free(yGpu);
  neuron_tensor_free(xGpu);
  neuron_tensor_free(d);
  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(GpuBlockScopeBatchMatMulChainMatchesCpuReference) {
  ScopedEnvOverride forceBackend("NEURON_GPU_FORCE_BACKEND", "vulkan");
  ScopedEnvOverride scopeBatch("NEURON_GPU_SCOPE_BATCH", "1");
  neuron_gpu_reset();

  int32_t aShape[2] = {24, 24};
  int32_t bShape[2] = {24, 24};
  int32_t cShape[2] = {24, 24};
  int32_t dShape[2] = {24, 24};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  NeuronTensor *c = neuron_tensor_create(2, cShape);
  NeuronTensor *d = neuron_tensor_create(2, dShape);
  ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr && d != nullptr);
  fill_tensor_sequential(a, 0.01f, 0.001f);
  fill_tensor_sequential(b, -0.02f, 0.0009f);
  fill_tensor_sequential(c, 0.03f, -0.0008f);
  fill_tensor_sequential(d, -0.05f, 0.0004f);

  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  NeuronTensor *xGpu = neuron_tensor_matmul_ex_hint(
      a, b, nullptr, 0, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *yGpu = neuron_tensor_matmul_ex_hint(
      xGpu, c, nullptr, 0, NEURON_TENSOR_EXEC_GPU_PREFER);
  NeuronTensor *zGpu = neuron_tensor_add_ex(yGpu, d, NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(xGpu != nullptr && yGpu != nullptr && zGpu != nullptr);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);

  NeuronTensor *xCpu = neuron_tensor_matmul_ex_hint(
      a, b, nullptr, 0, NEURON_TENSOR_EXEC_CPU_ONLY);
  NeuronTensor *yCpu = neuron_tensor_matmul_ex_hint(
      xCpu, c, nullptr, 0, NEURON_TENSOR_EXEC_CPU_ONLY);
  NeuronTensor *zCpu = neuron_tensor_add_ex(yCpu, d, NEURON_TENSOR_EXEC_CPU_ONLY);
  ASSERT_TRUE(xCpu != nullptr && yCpu != nullptr && zCpu != nullptr);
  ASSERT_TRUE(tensor_close(zGpu, zCpu, 3e-3f));

  neuron_tensor_free(zCpu);
  neuron_tensor_free(yCpu);
  neuron_tensor_free(xCpu);
  neuron_tensor_free(zGpu);
  neuron_tensor_free(yGpu);
  neuron_tensor_free(xGpu);
  neuron_tensor_free(d);
  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorPackedMatMulMatchesRegular) {
  int32_t aShape[2] = {5, 7};
  int32_t bShape[2] = {7, 6};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  ASSERT_TRUE(a != nullptr && b != nullptr);

  fill_tensor_sequential(a, 0.25f, 0.05f);
  fill_tensor_sequential(b, -0.2f, 0.03f);

  NeuronTensor *regular = neuron_tensor_matmul(a, b);
  ASSERT_TRUE(regular != nullptr);

  NeuronPackedMatrix *packed = nullptr;
  ASSERT_EQ(neuron_tensor_pack_b(b, &packed), 0);
  ASSERT_TRUE(packed != nullptr);

  NeuronTensor *packedOut = neuron_tensor_matmul_packed(a, packed, nullptr, 0);
  ASSERT_TRUE(packedOut != nullptr);
  ASSERT_EQ(regular->size, packedOut->size);

  for (int32_t i = 0; i < regular->size; ++i) {
    ASSERT_TRUE(std::fabs(regular->data[i] - packedOut->data[i]) < 1e-3f);
  }

  neuron_tensor_free(packedOut);
  neuron_tensor_packed_free(packed);
  neuron_tensor_free(regular);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorMatMulExAccumulateWorks) {
  int32_t aShape[2] = {3, 4};
  int32_t bShape[2] = {4, 2};
  int32_t outShape[2] = {3, 2};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  NeuronTensor *out = neuron_tensor_create(2, outShape);
  ASSERT_TRUE(a != nullptr && b != nullptr && out != nullptr);

  fill_tensor_sequential(a, 0.1f, 0.1f);
  fill_tensor_sequential(b, -0.3f, 0.07f);
  for (int32_t i = 0; i < out->size; ++i) {
    out->data[i] = 1.0f;
  }

  NeuronTensor *result = neuron_tensor_matmul_ex(
      a, b, out, NEURON_TENSOR_MATMUL_FLAG_ACCUMULATE);
  ASSERT_TRUE(result == out);

  NeuronTensor *plain = neuron_tensor_matmul(a, b);
  ASSERT_TRUE(plain != nullptr);

  for (int32_t i = 0; i < out->size; ++i) {
    ASSERT_TRUE(std::fabs(out->data[i] - (plain->data[i] + 1.0f)) < 1e-3f);
  }

  neuron_tensor_free(plain);
  neuron_tensor_free(out);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorHybridStructuredMatMulMatchesDenseReference) {
  ScopedEnvOverride structuredEnable("NEURON_TENSOR_STRUCTURED_MATMUL", "1");
  ScopedEnvOverride structuredThreshold("NEURON_TENSOR_STRUCTURED_THRESHOLD",
                                        "0.99999");
  ScopedEnvOverride hybridThreshold(
      "NEURON_TENSOR_STRUCTURED_HYBRID_THRESHOLD", "0.80");
  ScopedEnvOverride hybridDensity(
      "NEURON_TENSOR_STRUCTURED_HYBRID_MAX_DENSITY", "0.20");
  ScopedEnvOverride residualEps("NEURON_TENSOR_STRUCTURED_RESIDUAL_EPS",
                                "0.000001");

  int32_t aShape[2] = {3, 8};
  int32_t bShape[2] = {8, 8};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  ASSERT_TRUE(a != nullptr && b != nullptr);

  fill_tensor_sequential(a, -0.31f, 0.09f);
  const float row[8] = {0.15f, 0.05f, -0.2f, 0.4f, -0.1f, 0.25f, 0.07f, -0.3f};
  fill_circulant_from_row(b, row, 8);
  add_sparse_residual(b, 8);

  NeuronTensor *out = neuron_tensor_matmul(a, b);
  ASSERT_TRUE(out != nullptr);

  float expected[24] = {0.0f};
  for (int32_t i = 0; i < 3; ++i) {
    for (int32_t j = 0; j < 8; ++j) {
      float sum = 0.0f;
      for (int32_t k = 0; k < 8; ++k) {
        sum += a->data[i * 8 + k] * b->data[k * 8 + j];
      }
      expected[i * 8 + j] = sum;
    }
  }

  for (int32_t i = 0; i < out->size; ++i) {
    ASSERT_TRUE(std::fabs(out->data[i] - expected[i]) < 5e-3f);
  }

  neuron_tensor_free(out);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorStructuredFusedEpilogueMatchesDenseReference) {
  ScopedEnvOverride structuredEnable("NEURON_TENSOR_STRUCTURED_MATMUL", "1");
  ScopedEnvOverride structuredThreshold("NEURON_TENSOR_STRUCTURED_THRESHOLD",
                                        "0.95");

  int32_t aShape[2] = {2, 8};
  int32_t bShape[2] = {8, 8};
  int32_t biasShape[2] = {1, 8};
  int32_t residualShape[2] = {2, 8};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  NeuronTensor *bias = neuron_tensor_create(2, biasShape);
  NeuronTensor *residual = neuron_tensor_create(2, residualShape);
  ASSERT_TRUE(a != nullptr && b != nullptr && bias != nullptr &&
              residual != nullptr);

  fill_tensor_sequential(a, -0.2f, 0.11f);
  fill_tensor_sequential(bias, -0.1f, 0.04f);
  fill_tensor_sequential(residual, 0.15f, -0.05f);
  const float row[8] = {0.18f, -0.06f, 0.09f, 0.22f, -0.14f, 0.31f, -0.08f,
                        0.04f};
  fill_circulant_from_row(b, row, 8);

  NeuronTensor *out = neuron_tensor_linear_fused(
      a, b, nullptr, bias, residual, NEURON_TENSOR_ACTIVATION_RELU, nullptr, 0);
  ASSERT_TRUE(out != nullptr);

  float expected[16] = {0.0f};
  for (int32_t i = 0; i < 2; ++i) {
    for (int32_t j = 0; j < 8; ++j) {
      float sum = 0.0f;
      for (int32_t k = 0; k < 8; ++k) {
        sum += a->data[i * 8 + k] * b->data[k * 8 + j];
      }
      float v = sum + bias->data[j] + residual->data[i * 8 + j];
      expected[i * 8 + j] = (v < 0.0f) ? 0.0f : v;
    }
  }

  for (int32_t i = 0; i < out->size; ++i) {
    ASSERT_TRUE(std::fabs(out->data[i] - expected[i]) < 3e-3f);
  }

  neuron_tensor_free(out);
  neuron_tensor_free(residual);
  neuron_tensor_free(bias);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorLinearFusedBiasResidualRelu) {
  int32_t aShape[2] = {2, 3};
  int32_t bShape[2] = {3, 4};
  int32_t biasShape[2] = {1, 4};
  int32_t residualShape[2] = {2, 4};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  NeuronTensor *bias = neuron_tensor_create(2, biasShape);
  NeuronTensor *residual = neuron_tensor_create(2, residualShape);
  ASSERT_TRUE(a != nullptr && b != nullptr && bias != nullptr &&
              residual != nullptr);

  fill_tensor_sequential(a, -0.5f, 0.2f);
  fill_tensor_sequential(b, 0.1f, 0.05f);
  fill_tensor_sequential(bias, -0.1f, 0.03f);
  fill_tensor_sequential(residual, 0.2f, -0.04f);

  NeuronTensor *fused = neuron_tensor_linear_fused(
      a, b, nullptr, bias, residual, NEURON_TENSOR_ACTIVATION_RELU, nullptr, 0);
  ASSERT_TRUE(fused != nullptr);

  NeuronTensor *plain = neuron_tensor_matmul(a, b);
  ASSERT_TRUE(plain != nullptr);
  for (int32_t r = 0; r < plain->shape[0]; ++r) {
    for (int32_t c = 0; c < plain->shape[1]; ++c) {
      int32_t idx = r * plain->shape[1] + c;
      float value = plain->data[idx] + bias->data[c] + residual->data[idx];
      plain->data[idx] = value < 0.0f ? 0.0f : value;
    }
  }

  ASSERT_EQ(fused->size, plain->size);
  for (int32_t i = 0; i < plain->size; ++i) {
    ASSERT_TRUE(std::fabs(fused->data[i] - plain->data[i]) < 1e-3f);
  }

  neuron_tensor_free(plain);
  neuron_tensor_free(fused);
  neuron_tensor_free(residual);
  neuron_tensor_free(bias);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorConv2DBatchNormReluFusedMatchesReference) {
  NeuronTensor *input =
      create_tensor_4d(1, 1, 3, 3, {1.0f, 2.0f, 3.0f,
                                     4.0f, 5.0f, 6.0f,
                                     7.0f, 8.0f, 9.0f});
  NeuronTensor *kernel =
      create_tensor_4d(1, 1, 2, 2, {1.0f, 1.0f, 1.0f, 1.0f});
  NeuronTensor *bias = create_tensor_1d({0.0f});
  NeuronTensor *gamma = create_tensor_1d({2.0f});
  NeuronTensor *beta = create_tensor_1d({0.5f});
  NeuronTensor *mean = create_tensor_1d({20.0f});
  NeuronTensor *variance = create_tensor_1d({16.0f});
  ASSERT_TRUE(input != nullptr && kernel != nullptr && bias != nullptr &&
              gamma != nullptr && beta != nullptr && mean != nullptr &&
              variance != nullptr);

  NeuronTensor *autoOut = neuron_tensor_conv2d_batchnorm_relu(
      input, kernel, bias, gamma, beta, mean, variance, 0.0f, 1, 1, 0, 0);
  NeuronTensor *gpuPreferOut = neuron_tensor_conv2d_batchnorm_relu_ex_hint(
      input, kernel, bias, gamma, beta, mean, variance, 0.0f, 1, 1, 0, 0,
      NEURON_TENSOR_EXEC_GPU_PREFER);
  ASSERT_TRUE(autoOut != nullptr && gpuPreferOut != nullptr);
  ASSERT_EQ(autoOut->dimensions, 4);
  ASSERT_EQ(autoOut->shape[0], 1);
  ASSERT_EQ(autoOut->shape[1], 1);
  ASSERT_EQ(autoOut->shape[2], 2);
  ASSERT_EQ(autoOut->shape[3], 2);

  const float expected[] = {0.0f, 0.0f, 2.5f, 4.5f};
  for (int32_t i = 0; i < autoOut->size; ++i) {
    ASSERT_TRUE(std::fabs(autoOut->data[i] - expected[i]) < 1e-5f);
    ASSERT_TRUE(std::fabs(gpuPreferOut->data[i] - expected[i]) < 1e-5f);
  }

  neuron_tensor_free(gpuPreferOut);
  neuron_tensor_free(autoOut);
  neuron_tensor_free(variance);
  neuron_tensor_free(mean);
  neuron_tensor_free(beta);
  neuron_tensor_free(gamma);
  neuron_tensor_free(bias);
  neuron_tensor_free(kernel);
  neuron_tensor_free(input);
  return true;
}

TEST(TensorStructureAnalysisDetectsCirculant) {
  int32_t shape[2] = {8, 8};
  NeuronTensor *b = neuron_tensor_create(2, shape);
  ASSERT_TRUE(b != nullptr);

  const float row[8] = {0.5f, -0.2f, 0.1f, 0.0f, 0.05f, -0.1f, 0.3f, 0.2f};
  fill_circulant_from_row(b, row, 8);

  NeuronTensorStructureInfo info{};
  ASSERT_EQ(neuron_tensor_analyze_structure(b, &info), 0);
  ASSERT_EQ(info.rows, 8);
  ASSERT_EQ(info.cols, 8);
  ASSERT_TRUE(info.circulant_score > 0.999f);
  ASSERT_TRUE(info.selected_score > 0.999f);
  ASSERT_EQ(info.selected_kind, NEURON_TENSOR_STRUCTURE_CIRCULANT);

  neuron_tensor_free(b);
  return true;
}

TEST(TensorStructureAnalysisDetectsToeplitz) {
  int32_t shape[2] = {8, 8};
  NeuronTensor *b = neuron_tensor_create(2, shape);
  ASSERT_TRUE(b != nullptr);

  const float row[8] = {0.35f, 0.10f, -0.25f, 0.40f, 0.05f, -0.15f, 0.22f,
                        -0.12f};
  const float col[8] = {0.35f, -0.45f, 0.30f, 0.18f, -0.08f, 0.27f, -0.31f,
                        0.14f};
  fill_toeplitz_from_row_col(b, row, col, 8);

  NeuronTensorStructureInfo info{};
  ASSERT_EQ(neuron_tensor_analyze_structure(b, &info), 0);
  ASSERT_EQ(info.rows, 8);
  ASSERT_EQ(info.cols, 8);
  ASSERT_TRUE(info.toeplitz_score > 0.999f);
  ASSERT_EQ(info.selected_kind, NEURON_TENSOR_STRUCTURE_TOEPLITZ);

  neuron_tensor_free(b);
  return true;
}

TEST(TensorStructuredMatMulMatchesDenseReference) {
  ScopedEnvOverride structuredEnable("NEURON_TENSOR_STRUCTURED_MATMUL", "1");
  ScopedEnvOverride structuredThreshold("NEURON_TENSOR_STRUCTURED_THRESHOLD",
                                        "0.95");

  int32_t aShape[2] = {3, 8};
  int32_t bShape[2] = {8, 8};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  ASSERT_TRUE(a != nullptr && b != nullptr);

  fill_tensor_sequential(a, -0.25f, 0.03f);
  const float row[8] = {0.15f, 0.05f, -0.2f, 0.4f, -0.1f, 0.25f, 0.07f, -0.3f};
  fill_circulant_from_row(b, row, 8);

  NeuronTensor *out = neuron_tensor_matmul(a, b);
  ASSERT_TRUE(out != nullptr);

  float expected[24] = {0.0f};
  for (int32_t i = 0; i < 3; ++i) {
    for (int32_t j = 0; j < 8; ++j) {
      float sum = 0.0f;
      for (int32_t k = 0; k < 8; ++k) {
        sum += a->data[i * 8 + k] * b->data[k * 8 + j];
      }
      expected[i * 8 + j] = sum;
    }
  }

  for (int32_t i = 0; i < out->size; ++i) {
    ASSERT_TRUE(std::fabs(out->data[i] - expected[i]) < 2e-3f);
  }

  neuron_tensor_free(out);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorMatMulCacheInvalidatesOnWeightUpdate) {
  int32_t aShape[2] = {2, 1};
  int32_t bShape[2] = {1, 1};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  ASSERT_TRUE(a != nullptr && b != nullptr);
  a->data[0] = 2.0f;
  a->data[1] = -3.0f;
  b->data[0] = 1.5f;

  NeuronTensor *first = neuron_tensor_matmul(a, b);
  ASSERT_TRUE(first != nullptr);
  ASSERT_TRUE(std::fabs(first->data[0] - 3.0f) < 1e-4f);
  ASSERT_TRUE(std::fabs(first->data[1] + 4.5f) < 1e-4f);

  b->data[0] = -2.0f;
  NeuronTensor *second = neuron_tensor_matmul(a, b);
  ASSERT_TRUE(second != nullptr);
  ASSERT_TRUE(std::fabs(second->data[0] + 4.0f) < 1e-4f);
  ASSERT_TRUE(std::fabs(second->data[1] - 6.0f) < 1e-4f);

  neuron_tensor_free(second);
  neuron_tensor_free(first);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}

TEST(TensorToeplitzStructuredMatMulMatchesDenseReference) {
  ScopedEnvOverride structuredEnable("NEURON_TENSOR_STRUCTURED_MATMUL", "1");
  ScopedEnvOverride structuredThreshold("NEURON_TENSOR_STRUCTURED_THRESHOLD",
                                        "0.95");

  int32_t aShape[2] = {3, 8};
  int32_t bShape[2] = {8, 8};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  ASSERT_TRUE(a != nullptr && b != nullptr);

  fill_tensor_sequential(a, -0.11f, 0.07f);
  const float row[8] = {0.35f, 0.10f, -0.25f, 0.40f, 0.05f, -0.15f, 0.22f,
                        -0.12f};
  const float col[8] = {0.35f, -0.45f, 0.30f, 0.18f, -0.08f, 0.27f, -0.31f,
                        0.14f};
  fill_toeplitz_from_row_col(b, row, col, 8);

  NeuronTensor *out = neuron_tensor_matmul(a, b);
  ASSERT_TRUE(out != nullptr);

  float expected[24] = {0.0f};
  for (int32_t i = 0; i < 3; ++i) {
    for (int32_t j = 0; j < 8; ++j) {
      float sum = 0.0f;
      for (int32_t k = 0; k < 8; ++k) {
        sum += a->data[i * 8 + k] * b->data[k * 8 + j];
      }
      expected[i * 8 + j] = sum;
    }
  }

  for (int32_t i = 0; i < out->size; ++i) {
    ASSERT_TRUE(std::fabs(out->data[i] - expected[i]) < 3e-3f);
  }

  neuron_tensor_free(out);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return true;
}
