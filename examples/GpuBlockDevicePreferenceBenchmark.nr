module Time;

Init method() {
  loops is 120 as int;
  a is Tensor<float>.Random(1, 65536);
  b is Tensor<float>.Random(1, 65536);
  c is Tensor<float>.Random(1, 65536);
  d is Tensor<float>.Random(1, 65536);

  startDefault is Time.Now();
  gpu {
    for(i is 0 as int; i < loops; i++) {
      x is a + b;
      y is x * c;
      sinkDefault is (y * a) + d;
    }
  }
  endDefault is Time.Now();
  defaultMs is endDefault - startDefault;

  startDiscrete is Time.Now();
  gpu(mode: GPUMode.Discrete, policy: GPUPolicy.Prefer) {
    for(j is 0 as int; j < loops; j++) {
      x2 is a + b;
      y2 is x2 * c;
      sinkDiscrete is (y2 * a) + d;
    }
  }
  endDiscrete is Time.Now();
  discreteMs is endDiscrete - startDiscrete;

  startIntegrated is Time.Now();
  gpu(mode: GPUMode.Integrated, policy: GPUPolicy.Prefer) {
    for(k is 0 as int; k < loops; k++) {
      x3 is a + b;
      y3 is x3 * c;
      sinkIntegrated is (y3 * a) + d;
    }
  }
  endIntegrated is Time.Now();
  integratedMs is endIntegrated - startIntegrated;

  Print("gpu_device_preference_ms");
  Print(defaultMs);
  Print(discreteMs);
  Print(integratedMs);
}
