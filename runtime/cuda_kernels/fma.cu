extern "C" __global__ void tensor_fma(float *out, const float *a,
                                      const float *b, const float *c,
                                      unsigned int n) {
  const unsigned int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) {
    out[i] = a[i] * b[i] + c[i];
  }
}
