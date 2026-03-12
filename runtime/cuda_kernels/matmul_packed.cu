extern "C" __global__ void tensor_matmul_packed(
    float *out, const float *a, const float *packedData, const unsigned long long *offsets,
    const float *bias, const float *residual, int m, int n, int k, int kc, int nc,
    int kBlocks, int nBlocks, int panelCount, int biasRows, int biasCols,
    int residualCols, int activation, int accumulate) {
  int row = blockIdx.y * blockDim.y + threadIdx.y;
  int col = blockIdx.x * blockDim.x + threadIdx.x;
  if (row >= m || col >= n) {
    return;
  }

  float sum = accumulate ? out[row * n + col] : 0.0f;
  for (int p = 0; p < k; ++p) {
    int ncBlock = col / nc;
    int kcBlock = p / kc;
    int colLocal = col % nc;
    int pLocal = p % kc;
    int panelIndex = ncBlock * kBlocks + kcBlock;
    if (panelIndex < 0 || panelIndex >= panelCount) {
      continue;
    }
    int nc0 = ncBlock * nc;
    int kc0 = kcBlock * kc;
    int ncCur = (n - nc0) < nc ? (n - nc0) : nc;
    int kcCur = (k - kc0) < kc ? (k - kc0) : kc;
    if (colLocal >= ncCur || pLocal >= kcCur) {
      continue;
    }
    unsigned long long panelOffset = offsets[panelIndex];
    float b = packedData[panelOffset + pLocal * ncCur + colLocal];
    sum += a[row * k + p] * b;
  }

  if (bias != nullptr && biasCols == n) {
    int biasRow = (biasRows == 1) ? 0 : row;
    sum += bias[biasRow * biasCols + col];
  }
  if (residual != nullptr && residualCols == n) {
    sum += residual[row * residualCols + col];
  }

  if (activation == 1) {
    sum = sum < 0.0f ? 0.0f : sum;
  } else if (activation == 2) {
    const float c = 0.044715f;
    const float scale = 0.7978845608f;
    float cubic = sum * sum * sum;
    sum = 0.5f * sum * (1.0f + tanhf(scale * (sum + c * cubic)));
  }

  out[row * n + col] = sum;
}
