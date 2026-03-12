extern "C" __global__ void tensor_add(float *out, const float *a,
                                      const float *b, unsigned int n) {
  const unsigned int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) {
    out[i] = a[i] + b[i];
  }
}

extern "C" __global__ void tensor_sub(float *out, const float *a,
                                      const float *b, unsigned int n) {
  const unsigned int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) {
    out[i] = a[i] - b[i];
  }
}

extern "C" __global__ void tensor_mul(float *out, const float *a,
                                      const float *b, unsigned int n) {
  const unsigned int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) {
    out[i] = a[i] * b[i];
  }
}

extern "C" __global__ void tensor_div(float *out, const float *a,
                                      const float *b, unsigned int n) {
  const unsigned int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) {
    out[i] = a[i] / b[i];
  }
}
