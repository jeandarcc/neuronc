#ifndef TENSOR_MICROKERNEL_INTERNAL_H
#define TENSOR_MICROKERNEL_INTERNAL_H

#include "tensor_config_internal.h"
#include <stdint.h>

void tensor_pack_a_panel(const float *src, int32_t lda, float *dst, int32_t mc,
                         int32_t kc);

void tensor_pack_b_panel(const float *src, int32_t ldb, float *dst, int32_t kc,
                         int32_t nc);

void tensor_microkernel_scalar(const float *aPack, const float *bPack, float *c,
                               int32_t ldc, int32_t kc, int32_t nc, int32_t mr,
                               int32_t nr, int accumulate);

int tensor_cpu_supports_avx2_fma(void);

int tensor_compute_mc_block(const float *aPack, const float *bPack,
                            float *cBlock, int32_t n, int32_t kc, int32_t nc,
                            int32_t mc, int useAVX2, int accumulate,
                            TensorKernelVariant kernelVariant,
                            const TensorEpilogueConfig *epilogue,
                            int32_t blockRow0, int32_t blockCol0);

#endif // TENSOR_MICROKERNEL_INTERNAL_H
