// gpu block dispatches tensor ops with GPU-prefer hint and CPU fallback.
Init method() {
  a is Tensor<float>.Random(2, 2);
  b is Tensor<float>.Random(2, 2);
  c is Tensor<float>.Random(2, 2);

  cpuSum is a + b;
  cpuMul is a * b;
  cpuFmaLike is (a * b) + c;

  gpu {
    gpuSum is a + b;
    gpuMul is a * b;
    gpuFmaLike is (a * b) + c;
  }

  Print("gpu block prefers GPU and falls back to CPU for unsupported paths.");
}
