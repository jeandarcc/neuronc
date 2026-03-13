#ifndef Neuron_RUNTIME_TENSOR_MATH_INTERNAL_H
#define Neuron_RUNTIME_TENSOR_MATH_INTERNAL_H

#include "tensor_core_internal.h"
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float tensor_dot_product(const float *x, const float *y, int32_t n);

int tensor_is_power_of_two_i32(int32_t value);
uint32_t tensor_bit_reverse_u32(uint32_t value, uint32_t bitCount);
const uint32_t *tensor_fft_get_bitrev_table(int32_t n, uint32_t levels);

TensorComplex tensor_complex_mul(TensorComplex a, TensorComplex b);
void tensor_complex_pointwise_mul_inplace(TensorComplex *dst,
                                          const TensorComplex *rhs,
                                          int32_t count);

int32_t tensor_log2_floor_i32(int32_t value);
const TensorComplex *tensor_fft_get_twiddle_table(int32_t n, int inverse);
int tensor_fft_inplace(TensorComplex *data, int32_t n, int inverse);

#endif
