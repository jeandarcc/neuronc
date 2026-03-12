#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

namespace {

void fillRandom(std::vector<float> &values, std::mt19937 &rng) {
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  for (float &v : values) {
    v = dist(rng);
  }
}

int64_t runFmaBench(int size) {
  const std::size_t total = static_cast<std::size_t>(size) *
                            static_cast<std::size_t>(size);
  std::mt19937 rng(1337);
  std::vector<float> a(total);
  std::vector<float> b(total);
  std::vector<float> c(total);
  fillRandom(a, rng);
  fillRandom(b, rng);
  fillRandom(c, rng);

  const auto start = std::chrono::steady_clock::now();
  // Intentionally unfused scalar baseline:
  // mix = (a*b)+c, then out = mix @ b
  std::vector<float> tmp(total);
  std::vector<float> mix(total);
  std::vector<float> out(total, 0.0f);
  for (std::size_t i = 0; i < total; ++i) {
    tmp[i] = a[i] * b[i];
  }
  for (std::size_t i = 0; i < total; ++i) {
    mix[i] = tmp[i] + c[i];
  }
  for (int i = 0; i < size; ++i) {
    for (int j = 0; j < size; ++j) {
      float sum = 0.0f;
      for (int k = 0; k < size; ++k) {
        sum += mix[static_cast<std::size_t>(i) * size + k] *
               b[static_cast<std::size_t>(k) * size + j];
      }
      out[static_cast<std::size_t>(i) * size + j] = sum;
    }
  }
  volatile float sink = out[0];
  (void)sink;
  const auto end = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
      .count();
}

int64_t runMatMulBench(int iterations, int size) {
  const std::size_t total = static_cast<std::size_t>(size) *
                            static_cast<std::size_t>(size);
  std::mt19937 rng(4242);
  std::vector<float> a(total);
  std::vector<float> b(total);
  fillRandom(a, rng);
  fillRandom(b, rng);

  const auto start = std::chrono::steady_clock::now();
  for (int iter = 0; iter < iterations; ++iter) {
    std::vector<float> out(total, 0.0f);
    for (int i = 0; i < size; ++i) {
      for (int j = 0; j < size; ++j) {
        float sum = 0.0f;
        for (int k = 0; k < size; ++k) {
          sum += a[static_cast<std::size_t>(i) * size + k] *
                 b[static_cast<std::size_t>(k) * size + j];
        }
        out[static_cast<std::size_t>(i) * size + j] = sum;
      }
    }
    volatile float sink = out[(static_cast<std::size_t>(iter) * 17) % total];
    (void)sink;
  }
  const auto end = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
      .count();
}

} // namespace

int main() {
  const int64_t fmaMs = runFmaBench(1024);
  const int64_t matmulMs = runMatMulBench(1, 1024);

  std::cout << "NAIVE_FMA_MS\n" << fmaMs << "\n";
  std::cout << "NAIVE_MATMUL_MS\n" << matmulMs << "\n";
  return 0;
}
