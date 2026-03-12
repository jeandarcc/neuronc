#ifndef NPP_RUNTIME_TENSOR_CORE_INTERNAL_H
#define NPP_RUNTIME_TENSOR_CORE_INTERNAL_H

#include "neuron_tensor.h"
#include <stddef.h>
#include <stdint.h>


#if defined(_MSC_VER)
#define TENSOR_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define TENSOR_THREAD_LOCAL __thread
#else
#define TENSOR_THREAD_LOCAL _Thread_local
#endif

typedef enum {
  kTensorOpAdd,
  kTensorOpSub,
  kTensorOpMul,
  kTensorOpDiv,
} TensorBinaryOp;

typedef struct {
  float re;
  float im;
} TensorComplex;

typedef enum {
  kTensorStructuredNone = 0,
  kTensorStructuredCirculant = 1,
  kTensorStructuredToeplitz = 2,
} TensorStructuredKind;

typedef struct {
  float circulantScore;
  float toeplitzScore;
  float sparsity;
  float selectedScore;
  TensorStructuredKind selectedKind;
} TensorStructureAnalysis;

#define TENSOR_NC 256
#define TENSOR_KC 128
#define TENSOR_MC 256
#define TENSOR_MR 8
#define TENSOR_NR 8
#define TENSOR_PARALLEL_MIN_WORKLOAD 67108864LL

typedef struct {
  float *aPack;
  size_t aPackCapacity;
  float *bPack;
  size_t bPackCapacity;
  TensorComplex *fftWork;
  size_t fftWorkCapacity;
  TensorComplex *fftAux;
  size_t fftAuxCapacity;
  TensorComplex *fftTwiddleForward;
  size_t fftTwiddleForwardCapacity;
  int32_t fftTwiddleForwardN;
  TensorComplex *fftTwiddleInverse;
  size_t fftTwiddleInverseCapacity;
  int32_t fftTwiddleInverseN;
  uint32_t *fftBitRev;
  size_t fftBitRevCapacity;
  int32_t fftBitRevN;
} TensorThreadWorkspace;

typedef struct {
  const float *sourceData;
  int32_t rows;
  int32_t cols;
  uint64_t dataFingerprint;
  NeuronPackedMatrix *packed;
  TensorStructureAnalysis analysis;
  int32_t circulantFftSize;
  TensorComplex *circulantSpectrum;
  int32_t toeplitzFftSize;
  TensorComplex *toeplitzSpectrum;
  int hybridEnabled;
  TensorStructuredKind hybridBaseKind;
  int32_t hybridN;
  int32_t hybridNnz;
  float hybridDensity;
  int hybridUseDenseCorrection;
  NeuronPackedMatrix *hybridResidualPacked;
  int32_t *hybridRowPtr;
  int32_t *hybridColIdx;
  float *hybridValues;
} TensorPackedBCache;

typedef struct {
  const NeuronPackedMatrix *packedB;
  int accumulate;
  const NeuronTensor *bias;
  const NeuronTensor *residual;
  int32_t activation;
} TensorMatMulKernelOptions;

typedef struct {
  int enabled;
  const float *biasData;
  int32_t biasRows;
  int32_t biasCols;
  const float *residualData;
  int32_t residualCols;
  int32_t activation;
} TensorEpilogueConfig;

extern TENSOR_THREAD_LOCAL TensorThreadWorkspace g_tensorWorkspace;
extern TENSOR_THREAD_LOCAL TensorPackedBCache g_tensorPackedBCache;

int32_t tensor_min_i32(int32_t a, int32_t b);
void *tensor_aligned_alloc(size_t size, size_t alignment);
void tensor_aligned_free(void *ptr);
float *tensor_workspace_reserve(float **slot, size_t *capacity,
                                size_t elementCount);
TensorComplex *tensor_workspace_reserve_complex(TensorComplex **slot,
                                                size_t *capacity,
                                                size_t elementCount);
uint32_t *tensor_workspace_reserve_u32(uint32_t **slot, size_t *capacity,
                                       size_t elementCount);

void tensor_pin_current_thread(int threadIndex);
int tensor_recommended_threads(int64_t workload, int32_t m, int32_t n,
                               int32_t k);

#endif
