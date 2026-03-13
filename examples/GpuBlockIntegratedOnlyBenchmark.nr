module Time;

Init method() {
  loops is 120 as int;
  a is Tensor<float>.Random(1, 65536);
  b is Tensor<float>.Random(1, 65536);
  c is Tensor<float>.Random(1, 65536);
  d is Tensor<float>.Random(1, 65536);

  startMs is Time.Now();
  gpu(mode: GPUMode.Integrated, policy: GPUPolicy.Prefer) {
    for(i is 0 as int; i < loops; i++) {
      x is a + b;
      y is x * c;
      sink is (y * a) + d;
    }
  }
  endMs is Time.Now();
  totalMs is endMs - startMs;

  Print("gpu_integrated_only_ms");
  Print(totalMs);
}
