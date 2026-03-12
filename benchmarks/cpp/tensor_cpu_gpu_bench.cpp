#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

extern "C" {
#include "neuron_gpu.h"
#include "neuron_tensor.h"
}

namespace {

using Clock = std::chrono::steady_clock;
using Ms = std::chrono::duration<double, std::milli>;

struct BenchConfig {
  int32_t m = 512;
  int32_t n = 512;
  int32_t k = 512;
  int32_t elem = 1 << 20;
  int iters = 8;
  int warmup = 2;
  std::string forceBackend = "auto";
};

struct TimingPair {
  double cpuMs = 0.0;
  double gpuMs = 0.0;
  double maxAbsDiff = 0.0;
  bool ok = false;
};

void setEnvVar(const char *name, const std::string &value) {
#if defined(_WIN32)
  _putenv_s(name, value.c_str());
#else
  setenv(name, value.c_str(), 1);
#endif
}

double medianMs(std::vector<double> values) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const size_t mid = values.size() / 2;
  if ((values.size() % 2U) == 1U) {
    return values[mid];
  }
  return 0.5 * (values[mid - 1] + values[mid]);
}

double maxAbsDiff(const NeuronTensor *lhs, const NeuronTensor *rhs) {
  if (lhs == nullptr || rhs == nullptr || lhs->size != rhs->size) {
    return std::numeric_limits<double>::infinity();
  }
  double maxDiff = 0.0;
  for (int32_t i = 0; i < lhs->size; ++i) {
    const double diff = std::fabs((double)lhs->data[i] - (double)rhs->data[i]);
    if (diff > maxDiff) {
      maxDiff = diff;
    }
  }
  return maxDiff;
}

void fillTensorPattern(NeuronTensor *t, float base, float step) {
  if (t == nullptr) {
    return;
  }
  float v = base;
  for (int32_t i = 0; i < t->size; ++i) {
    t->data[i] = v;
    v += step;
  }
}

template <typename Fn>
double timeMedian(int iters, int warmup, Fn &&fn) {
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
  return medianMs(samples);
}

TimingPair benchAdd(int32_t elem, int iters, int warmup) {
  TimingPair out;
  int32_t shape[2] = {1, elem};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  if (a == nullptr || b == nullptr) {
    std::cerr << "Allocation failed for add benchmark\n";
    neuron_tensor_free(b);
    neuron_tensor_free(a);
    return out;
  }
  fillTensorPattern(a, 0.1f, 0.0001f);
  fillTensorPattern(b, -0.2f, 0.0002f);

  NeuronTensor *cpuRef =
      neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
  NeuronTensor *gpuRef =
      neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
  if (cpuRef != nullptr && gpuRef != nullptr) {
    out.maxAbsDiff = maxAbsDiff(cpuRef, gpuRef);
    out.ok = std::isfinite(out.maxAbsDiff);
  }
  neuron_tensor_free(gpuRef);
  neuron_tensor_free(cpuRef);

  out.cpuMs = timeMedian(iters, warmup, [&]() {
    NeuronTensor *r = neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_CPU_ONLY);
    if (r == nullptr) {
      std::abort();
    }
    volatile float sink = r->data[0];
    (void)sink;
    neuron_tensor_free(r);
  });

  out.gpuMs = timeMedian(iters, warmup, [&]() {
    NeuronTensor *r =
        neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_GPU_PREFER);
    if (r == nullptr) {
      std::abort();
    }
    volatile float sink = r->data[0];
    (void)sink;
    neuron_tensor_free(r);
  });

  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return out;
}

TimingPair benchFma(int32_t elem, int iters, int warmup) {
  TimingPair out;
  int32_t shape[2] = {1, elem};
  NeuronTensor *a = neuron_tensor_create(2, shape);
  NeuronTensor *b = neuron_tensor_create(2, shape);
  NeuronTensor *c = neuron_tensor_create(2, shape);
  if (a == nullptr || b == nullptr || c == nullptr) {
    std::cerr << "Allocation failed for fma benchmark\n";
    neuron_tensor_free(c);
    neuron_tensor_free(b);
    neuron_tensor_free(a);
    return out;
  }
  fillTensorPattern(a, 0.1f, 0.0001f);
  fillTensorPattern(b, -0.3f, 0.00015f);
  fillTensorPattern(c, 0.4f, -0.00005f);

  NeuronTensor *cpuRef =
      neuron_tensor_fma_ex(a, b, c, NEURON_TENSOR_EXEC_CPU_ONLY);
  NeuronTensor *gpuRef =
      neuron_tensor_fma_ex(a, b, c, NEURON_TENSOR_EXEC_GPU_PREFER);
  if (cpuRef != nullptr && gpuRef != nullptr) {
    out.maxAbsDiff = maxAbsDiff(cpuRef, gpuRef);
    out.ok = std::isfinite(out.maxAbsDiff);
  }
  neuron_tensor_free(gpuRef);
  neuron_tensor_free(cpuRef);

  out.cpuMs = timeMedian(iters, warmup, [&]() {
    NeuronTensor *r =
        neuron_tensor_fma_ex(a, b, c, NEURON_TENSOR_EXEC_CPU_ONLY);
    if (r == nullptr) {
      std::abort();
    }
    volatile float sink = r->data[0];
    (void)sink;
    neuron_tensor_free(r);
  });

  out.gpuMs = timeMedian(iters, warmup, [&]() {
    NeuronTensor *r =
        neuron_tensor_fma_ex(a, b, c, NEURON_TENSOR_EXEC_GPU_PREFER);
    if (r == nullptr) {
      std::abort();
    }
    volatile float sink = r->data[0];
    (void)sink;
    neuron_tensor_free(r);
  });

  neuron_tensor_free(c);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return out;
}

TimingPair benchMatMul(int32_t m, int32_t n, int32_t k, int iters, int warmup) {
  TimingPair out;
  int32_t aShape[2] = {m, k};
  int32_t bShape[2] = {k, n};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  if (a == nullptr || b == nullptr) {
    std::cerr << "Allocation failed for matmul benchmark\n";
    neuron_tensor_free(b);
    neuron_tensor_free(a);
    return out;
  }
  fillTensorPattern(a, 0.05f, 0.00002f);
  fillTensorPattern(b, -0.03f, 0.00003f);

  NeuronTensor *cpuRef = neuron_tensor_matmul_ex_hint(
      a, b, nullptr, 0, NEURON_TENSOR_EXEC_CPU_ONLY);
  NeuronTensor *gpuRef = neuron_tensor_matmul_ex_hint(
      a, b, nullptr, 0, NEURON_TENSOR_EXEC_GPU_PREFER);
  if (cpuRef != nullptr && gpuRef != nullptr) {
    out.maxAbsDiff = maxAbsDiff(cpuRef, gpuRef);
    out.ok = std::isfinite(out.maxAbsDiff);
  }
  neuron_tensor_free(gpuRef);
  neuron_tensor_free(cpuRef);

  out.cpuMs = timeMedian(iters, warmup, [&]() {
    NeuronTensor *r = neuron_tensor_matmul_ex_hint(
        a, b, nullptr, 0, NEURON_TENSOR_EXEC_CPU_ONLY);
    if (r == nullptr) {
      std::abort();
    }
    volatile float sink = r->data[0];
    (void)sink;
    neuron_tensor_free(r);
  });

  out.gpuMs = timeMedian(iters, warmup, [&]() {
    NeuronTensor *r = neuron_tensor_matmul_ex_hint(
        a, b, nullptr, 0, NEURON_TENSOR_EXEC_GPU_PREFER);
    if (r == nullptr) {
      std::abort();
    }
    volatile float sink = r->data[0];
    (void)sink;
    neuron_tensor_free(r);
  });

  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return out;
}

TimingPair benchLinearFused(int32_t m, int32_t n, int32_t k, int iters,
                            int warmup) {
  TimingPair out;
  int32_t aShape[2] = {m, k};
  int32_t bShape[2] = {k, n};
  int32_t biasShape[2] = {1, n};
  int32_t residualShape[2] = {m, n};
  NeuronTensor *a = neuron_tensor_create(2, aShape);
  NeuronTensor *b = neuron_tensor_create(2, bShape);
  NeuronTensor *bias = neuron_tensor_create(2, biasShape);
  NeuronTensor *residual = neuron_tensor_create(2, residualShape);
  if (a == nullptr || b == nullptr || bias == nullptr || residual == nullptr) {
    std::cerr << "Allocation failed for linear fused benchmark\n";
    neuron_tensor_free(residual);
    neuron_tensor_free(bias);
    neuron_tensor_free(b);
    neuron_tensor_free(a);
    return out;
  }
  fillTensorPattern(a, 0.01f, 0.00004f);
  fillTensorPattern(b, -0.07f, 0.00002f);
  fillTensorPattern(bias, 0.02f, 0.00003f);
  fillTensorPattern(residual, -0.03f, 0.00001f);

  NeuronTensor *cpuRef = neuron_tensor_linear_fused_ex_hint(
      a, b, nullptr, bias, residual, NEURON_TENSOR_ACTIVATION_RELU, nullptr, 0,
      NEURON_TENSOR_EXEC_CPU_ONLY);
  NeuronTensor *gpuRef = neuron_tensor_linear_fused_ex_hint(
      a, b, nullptr, bias, residual, NEURON_TENSOR_ACTIVATION_RELU, nullptr, 0,
      NEURON_TENSOR_EXEC_GPU_PREFER);
  if (cpuRef != nullptr && gpuRef != nullptr) {
    out.maxAbsDiff = maxAbsDiff(cpuRef, gpuRef);
    out.ok = std::isfinite(out.maxAbsDiff);
  }
  neuron_tensor_free(gpuRef);
  neuron_tensor_free(cpuRef);

  out.cpuMs = timeMedian(iters, warmup, [&]() {
    NeuronTensor *r = neuron_tensor_linear_fused_ex_hint(
        a, b, nullptr, bias, residual, NEURON_TENSOR_ACTIVATION_RELU, nullptr, 0,
        NEURON_TENSOR_EXEC_CPU_ONLY);
    if (r == nullptr) {
      std::abort();
    }
    volatile float sink = r->data[0];
    (void)sink;
    neuron_tensor_free(r);
  });

  out.gpuMs = timeMedian(iters, warmup, [&]() {
    NeuronTensor *r = neuron_tensor_linear_fused_ex_hint(
        a, b, nullptr, bias, residual, NEURON_TENSOR_ACTIVATION_RELU, nullptr, 0,
        NEURON_TENSOR_EXEC_GPU_PREFER);
    if (r == nullptr) {
      std::abort();
    }
    volatile float sink = r->data[0];
    (void)sink;
    neuron_tensor_free(r);
  });

  neuron_tensor_free(residual);
  neuron_tensor_free(bias);
  neuron_tensor_free(b);
  neuron_tensor_free(a);
  return out;
}

double speedup(double cpuMs, double gpuMs) {
  if (gpuMs <= 0.0) {
    return 0.0;
  }
  return cpuMs / gpuMs;
}

template <typename T>
bool parsePositiveIntArg(const char *arg, T &out) {
  char *end = nullptr;
  long v = std::strtol(arg, &end, 10);
  if (end == nullptr || *end != '\0' || v <= 0 ||
      v > std::numeric_limits<T>::max()) {
    return false;
  }
  out = (T)v;
  return true;
}

void printUsage(const char *exe) {
  std::cout << "Usage: " << exe
            << " [--m N] [--n N] [--k N] [--elem N] [--iters N] [--warmup N]"
               " [--backend auto|cpu|cuda|vulkan]\n";
}

} // namespace

int main(int argc, char **argv) {
  BenchConfig cfg;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto needValue = [&](const char *flag) -> const char * {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << flag << "\n";
        printUsage(argv[0]);
        std::exit(2);
      }
      return argv[++i];
    };

    if (arg == "--m") {
      if (!parsePositiveIntArg(needValue("--m"), cfg.m)) {
        std::cerr << "Invalid --m value\n";
        return 2;
      }
    } else if (arg == "--n") {
      if (!parsePositiveIntArg(needValue("--n"), cfg.n)) {
        std::cerr << "Invalid --n value\n";
        return 2;
      }
    } else if (arg == "--k") {
      if (!parsePositiveIntArg(needValue("--k"), cfg.k)) {
        std::cerr << "Invalid --k value\n";
        return 2;
      }
    } else if (arg == "--elem") {
      if (!parsePositiveIntArg(needValue("--elem"), cfg.elem)) {
        std::cerr << "Invalid --elem value\n";
        return 2;
      }
    } else if (arg == "--iters") {
      if (!parsePositiveIntArg(needValue("--iters"), cfg.iters)) {
        std::cerr << "Invalid --iters value\n";
        return 2;
      }
    } else if (arg == "--warmup") {
      if (!parsePositiveIntArg(needValue("--warmup"), cfg.warmup)) {
        std::cerr << "Invalid --warmup value\n";
        return 2;
      }
    } else if (arg == "--backend") {
      cfg.forceBackend = needValue("--backend");
      if (cfg.forceBackend != "auto" && cfg.forceBackend != "cpu" &&
          cfg.forceBackend != "cuda" && cfg.forceBackend != "vulkan") {
        std::cerr << "Invalid --backend value\n";
        return 2;
      }
    } else if (arg == "--help" || arg == "-h") {
      printUsage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      printUsage(argv[0]);
      return 2;
    }
  }

  setEnvVar("NEURON_GPU_FORCE_BACKEND", cfg.forceBackend);
  neuron_gpu_reset();
  const NeuronGpuBackend backend = neuron_gpu_backend();
  const char *backendName = neuron_gpu_backend_name();

  std::cout << "=== Tensor CPU vs GPU Benchmark ===\n";
  std::cout << "forced_backend=" << cfg.forceBackend
            << " active_backend=" << (backendName ? backendName : "unknown")
            << " backend_id=" << (int)backend << "\n";
  std::cout << "supports(add/fma/matmul)="
            << neuron_gpu_supports_op(NEURON_GPU_OP_TENSOR_ADD) << "/"
            << neuron_gpu_supports_op(NEURON_GPU_OP_TENSOR_FMA) << "/"
            << neuron_gpu_supports_op(NEURON_GPU_OP_TENSOR_MATMUL) << "\n";
  std::cout << "shape(matmul)=(" << cfg.m << "," << cfg.k << ")x(" << cfg.k
            << "," << cfg.n << ")"
            << " elem=" << cfg.elem << " iters=" << cfg.iters
            << " warmup=" << cfg.warmup << "\n\n";

  const TimingPair add = benchAdd(cfg.elem, cfg.iters, cfg.warmup);
  const TimingPair fma = benchFma(cfg.elem, cfg.iters, cfg.warmup);
  const TimingPair matmul =
      benchMatMul(cfg.m, cfg.n, cfg.k, cfg.iters, cfg.warmup);
  const TimingPair fused =
      benchLinearFused(cfg.m, cfg.n, cfg.k, cfg.iters, cfg.warmup);

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "op,cpu_ms,gpu_ms,speedup_cpu_over_gpu,max_abs_diff\n";
  std::cout << "add," << add.cpuMs << "," << add.gpuMs << ","
            << speedup(add.cpuMs, add.gpuMs) << "," << add.maxAbsDiff << "\n";
  std::cout << "fma," << fma.cpuMs << "," << fma.gpuMs << ","
            << speedup(fma.cpuMs, fma.gpuMs) << "," << fma.maxAbsDiff << "\n";
  std::cout << "matmul," << matmul.cpuMs << "," << matmul.gpuMs << ","
            << speedup(matmul.cpuMs, matmul.gpuMs) << "," << matmul.maxAbsDiff
            << "\n";
  std::cout << "linear_fused_relu," << fused.cpuMs << "," << fused.gpuMs << ","
            << speedup(fused.cpuMs, fused.gpuMs) << "," << fused.maxAbsDiff
            << "\n";

  const char *lastError = neuron_gpu_last_error();
  if (lastError != nullptr && lastError[0] != '\0') {
    std::cout << "\nlast_gpu_error=" << lastError << "\n";
  }

  return 0;
}
