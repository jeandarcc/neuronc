#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>

extern "C" {
#include "../../runtime/include/neuron_tensor.h"
}

namespace {

constexpr int kSize = 1024;
constexpr int kRepeats = 6;

void setEnvVar(const char *name, const char *value) {
#if defined(_WIN32)
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

void fillRandom(std::vector<float> &values, std::mt19937 &rng, float lo,
                float hi) {
  std::uniform_real_distribution<float> dist(lo, hi);
  for (float &v : values) {
    v = dist(rng);
  }
}

std::vector<float> makeCirculant(const std::vector<float> &firstRow, int n) {
  std::vector<float> out(static_cast<std::size_t>(n) *
                         static_cast<std::size_t>(n));
  for (int r = 0; r < n; ++r) {
    for (int c = 0; c < n; ++c) {
      const int idx = (c - r + n) % n;
      out[static_cast<std::size_t>(r) * n + c] =
          firstRow[static_cast<std::size_t>(idx)];
    }
  }
  return out;
}

std::vector<float> makeToeplitz(const std::vector<float> &firstRow,
                                const std::vector<float> &firstCol, int n) {
  std::vector<float> out(static_cast<std::size_t>(n) *
                         static_cast<std::size_t>(n));
  for (int r = 0; r < n; ++r) {
    for (int c = 0; c < n; ++c) {
      out[static_cast<std::size_t>(r) * n + c] =
          (c >= r) ? firstRow[static_cast<std::size_t>(c - r)]
                   : firstCol[static_cast<std::size_t>(r - c)];
    }
  }
  return out;
}

std::vector<float> makeHybrid(const std::vector<float> &base, int n,
                              float density, float amplitude,
                              std::uint32_t seed) {
  std::vector<float> out = base;
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> noise(-amplitude, amplitude);
  std::uniform_int_distribution<int> indexDist(0, n - 1);
  const int total = n * n;
  const int perturbCount = static_cast<int>(density * static_cast<float>(total));
  for (int i = 0; i < perturbCount; ++i) {
    const int r = indexDist(rng);
    const int c = indexDist(rng);
    out[static_cast<std::size_t>(r) * n + c] += noise(rng);
  }
  return out;
}

NeuronTensor *createTensorFromData(int rows, int cols,
                                   const std::vector<float> &data) {
  int32_t shape[2] = {rows, cols};
  NeuronTensor *t = neuron_tensor_create(2, shape);
  if (t == nullptr) {
    return nullptr;
  }
  std::memcpy(t->data, data.data(),
              static_cast<std::size_t>(rows) * cols * sizeof(float));
  return t;
}

template <typename Fn> double benchmarkMs(Fn &&fn, int repeats) {
  fn();
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < repeats; ++i) {
    fn();
  }
  const auto end = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(end - start).count() /
         static_cast<double>(repeats);
}

double benchMatMul(NeuronTensor *a, NeuronTensor *b) {
  return benchmarkMs(
      [&]() {
        NeuronTensor *out = neuron_tensor_matmul(a, b);
        if (out == nullptr) {
          std::abort();
        }
        neuron_tensor_free(out);
      },
      kRepeats);
}

double benchFused(NeuronTensor *a, NeuronTensor *b, NeuronTensor *bias,
                  NeuronTensor *residual) {
  return benchmarkMs(
      [&]() {
        NeuronTensor *out = neuron_tensor_linear_fused(
            a, b, nullptr, bias, residual, NEURON_TENSOR_ACTIVATION_RELU, nullptr,
            0);
        if (out == nullptr) {
          std::abort();
        }
        neuron_tensor_free(out);
      },
      kRepeats);
}

long long roundMs(double value) {
  return static_cast<long long>(std::llround(value));
}

} // namespace

int main() {
  setEnvVar("NEURON_TENSOR_STRUCTURED_MATMUL", "1");
  setEnvVar("NEURON_TENSOR_STRUCTURED_THRESHOLD", "0.99");
  setEnvVar("NEURON_TENSOR_STRUCTURED_HYBRID_THRESHOLD", "0.85");
  setEnvVar("NEURON_TENSOR_STRUCTURED_HYBRID_MAX_DENSITY", "0.05");
  setEnvVar("NEURON_TENSOR_STRUCTURED_RESIDUAL_EPS", "0.02");

  std::mt19937 rng(1337);
  std::vector<float> aData(static_cast<std::size_t>(kSize) * kSize);
  std::vector<float> firstRow(static_cast<std::size_t>(kSize));
  std::vector<float> firstCol(static_cast<std::size_t>(kSize));
  std::vector<float> biasData(static_cast<std::size_t>(kSize));
  std::vector<float> residualData(static_cast<std::size_t>(kSize) * kSize);
  fillRandom(aData, rng, -1.0f, 1.0f);
  fillRandom(firstRow, rng, -0.5f, 0.5f);
  fillRandom(firstCol, rng, -0.5f, 0.5f);
  firstCol[0] = firstRow[0];
  fillRandom(biasData, rng, -0.1f, 0.1f);
  fillRandom(residualData, rng, -0.2f, 0.2f);

  const std::vector<float> bCirc = makeCirculant(firstRow, kSize);
  const std::vector<float> bToep = makeToeplitz(firstRow, firstCol, kSize);
  const std::vector<float> bHybrid =
      makeHybrid(bCirc, kSize, 0.06f, 0.03f, 2026u);

  NeuronTensor *a = createTensorFromData(kSize, kSize, aData);
  NeuronTensor *circ = createTensorFromData(kSize, kSize, bCirc);
  NeuronTensor *toep = createTensorFromData(kSize, kSize, bToep);
  NeuronTensor *hybrid = createTensorFromData(kSize, kSize, bHybrid);
  NeuronTensor *bias = createTensorFromData(1, kSize, biasData);
  NeuronTensor *residual = createTensorFromData(kSize, kSize, residualData);
  if (a == nullptr || circ == nullptr || toep == nullptr || hybrid == nullptr ||
      bias == nullptr || residual == nullptr) {
    std::cerr << "Failed to allocate tensors.\n";
    return 1;
  }

  const auto circMatMulMs = benchMatMul(a, circ);
  const auto toepMatMulMs = benchMatMul(a, toep);
  const auto hybridMatMulMs = benchMatMul(a, hybrid);

  const auto circFusedMs = benchFused(a, circ, bias, residual);
  const auto toepFusedMs = benchFused(a, toep, bias, residual);
  const auto hybridFusedMs = benchFused(a, hybrid, bias, residual);

  std::cout << "Neuron_STRUCT_CIRC_MATMUL_MS\n" << roundMs(circMatMulMs) << "\n";
  std::cout << "Neuron_STRUCT_TOEP_MATMUL_MS\n" << roundMs(toepMatMulMs) << "\n";
  std::cout << "Neuron_STRUCT_HYBRID_MATMUL_MS\n" << roundMs(hybridMatMulMs)
            << "\n";
  std::cout << "Neuron_STRUCT_CIRC_FUSED_MS\n" << roundMs(circFusedMs) << "\n";
  std::cout << "Neuron_STRUCT_TOEP_FUSED_MS\n" << roundMs(toepFusedMs) << "\n";
  std::cout << "Neuron_STRUCT_HYBRID_FUSED_MS\n" << roundMs(hybridFusedMs)
            << "\n";

  neuron_tensor_free(residual);
  neuron_tensor_free(bias);
  neuron_tensor_free(hybrid);
  neuron_tensor_free(toep);
  neuron_tensor_free(circ);
  neuron_tensor_free(a);
  neuron_tensor_release_workspace_cache();
  return 0;
}
