#include "gpu_internal.h"
#include "cuda_kernels.h"
#include "neuron_platform.h"

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef Neuron_ENABLE_CUDA_BACKEND
#define Neuron_ENABLE_CUDA_BACKEND 1
#endif

#if Neuron_ENABLE_CUDA_BACKEND


typedef int CUresult;
typedef int CUdevice;
typedef struct CUctx_st *CUcontext;
typedef struct CUmod_st *CUmodule;
typedef struct CUfunc_st *CUfunction;
typedef unsigned long long CUdeviceptr;

#define CUDA_SUCCESS 0

typedef CUresult (*PFN_cuInit)(unsigned int flags);
typedef CUresult (*PFN_cuDeviceGetCount)(int *count);
typedef CUresult (*PFN_cuDeviceGet)(CUdevice *device, int ordinal);
typedef CUresult (*PFN_cuCtxCreate)(CUcontext *pctx, unsigned int flags,
                                    CUdevice dev);
typedef CUresult (*PFN_cuCtxDestroy)(CUcontext ctx);
typedef CUresult (*PFN_cuModuleLoadDataEx)(CUmodule *module, const void *image,
                                           unsigned int numOptions,
                                           int *options, void **optionValues);
typedef CUresult (*PFN_cuModuleUnload)(CUmodule hmod);
typedef CUresult (*PFN_cuModuleGetFunction)(CUfunction *hfunc, CUmodule hmod,
                                            const char *name);
typedef CUresult (*PFN_cuLaunchKernel)(CUfunction f, unsigned int gridDimX,
                                       unsigned int gridDimY,
                                       unsigned int gridDimZ,
                                       unsigned int blockDimX,
                                       unsigned int blockDimY,
                                       unsigned int blockDimZ,
                                       unsigned int sharedMemBytes,
                                       void *hStream, void **kernelParams,
                                       void **extra);
typedef CUresult (*PFN_cuCtxSynchronize)(void);
typedef CUresult (*PFN_cuMemAlloc)(CUdeviceptr *dptr, size_t bytesize);
typedef CUresult (*PFN_cuMemFree)(CUdeviceptr dptr);
typedef CUresult (*PFN_cuMemcpyHtoD)(CUdeviceptr dstDevice,
                                     const void *srcHost, size_t ByteCount);
typedef CUresult (*PFN_cuMemcpyDtoH)(void *dstHost, CUdeviceptr srcDevice,
                                     size_t ByteCount);

typedef struct {
  NeuronPlatformLibraryHandle handle;
  int initialized;
  int available;
  char last_error[512];

  CUcontext context;
  CUmodule module;
  CUfunction kernel_add;
  CUfunction kernel_sub;
  CUfunction kernel_mul;
  CUfunction kernel_div;
  CUfunction kernel_fma;
  CUfunction kernel_matmul_dense;
  CUfunction kernel_matmul_packed;

  PFN_cuInit cuInit;
  PFN_cuDeviceGetCount cuDeviceGetCount;
  PFN_cuDeviceGet cuDeviceGet;
  PFN_cuCtxCreate cuCtxCreate;
  PFN_cuCtxDestroy cuCtxDestroy;
  PFN_cuModuleLoadDataEx cuModuleLoadDataEx;
  PFN_cuModuleUnload cuModuleUnload;
  PFN_cuModuleGetFunction cuModuleGetFunction;
  PFN_cuLaunchKernel cuLaunchKernel;
  PFN_cuCtxSynchronize cuCtxSynchronize;
  PFN_cuMemAlloc cuMemAlloc;
  PFN_cuMemFree cuMemFree;
  PFN_cuMemcpyHtoD cuMemcpyHtoD;
  PFN_cuMemcpyDtoH cuMemcpyDtoH;
} CudaDispatchState;

static CudaDispatchState g_cuda_dispatch = {0};

static void clear_last_error(void) { g_cuda_dispatch.last_error[0] = '\0'; }

static void set_last_error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(g_cuda_dispatch.last_error, sizeof(g_cuda_dispatch.last_error), fmt,
            args);
  va_end(args);
}

static void copy_error_out(char *error_buffer, size_t error_size) {
  if (error_buffer == NULL || error_size == 0) {
    return;
  }
  if (g_cuda_dispatch.last_error[0] == '\0') {
    error_buffer[0] = '\0';
    return;
  }
  snprintf(error_buffer, error_size, "%s", g_cuda_dispatch.last_error);
}

static void *load_symbol(const char *name) {
  if (g_cuda_dispatch.handle == NULL || name == NULL) {
    return NULL;
  }
  return neuron_platform_load_symbol(g_cuda_dispatch.handle, name);
}

static void *load_symbol_any(const char *first, const char *second) {
  void *symbol = load_symbol(first);
  if (symbol == NULL && second != NULL) {
    symbol = load_symbol(second);
  }
  return symbol;
}

static int open_cuda_driver_library(void) {
#if defined(_WIN32)
  g_cuda_dispatch.handle = neuron_platform_open_library("nvcuda.dll");
#else
  g_cuda_dispatch.handle = neuron_platform_open_library("libcuda.so.1");
  if (g_cuda_dispatch.handle == NULL) {
    g_cuda_dispatch.handle = neuron_platform_open_library("libcuda.so");
  }
#endif
  return g_cuda_dispatch.handle != NULL;
}

static void close_cuda_driver_library(void) {
  if (g_cuda_dispatch.handle == NULL) {
    return;
  }
  neuron_platform_close_library(g_cuda_dispatch.handle);
  g_cuda_dispatch.handle = NULL;
}

static int load_cuda_symbols(void) {
  g_cuda_dispatch.cuInit = (PFN_cuInit)load_symbol("cuInit");
  g_cuda_dispatch.cuDeviceGetCount =
      (PFN_cuDeviceGetCount)load_symbol("cuDeviceGetCount");
  g_cuda_dispatch.cuDeviceGet = (PFN_cuDeviceGet)load_symbol("cuDeviceGet");
  g_cuda_dispatch.cuCtxCreate =
      (PFN_cuCtxCreate)load_symbol_any("cuCtxCreate_v2", "cuCtxCreate");
  g_cuda_dispatch.cuCtxDestroy =
      (PFN_cuCtxDestroy)load_symbol_any("cuCtxDestroy_v2", "cuCtxDestroy");
  g_cuda_dispatch.cuModuleLoadDataEx = (PFN_cuModuleLoadDataEx)load_symbol(
      "cuModuleLoadDataEx");
  g_cuda_dispatch.cuModuleUnload =
      (PFN_cuModuleUnload)load_symbol("cuModuleUnload");
  g_cuda_dispatch.cuModuleGetFunction =
      (PFN_cuModuleGetFunction)load_symbol("cuModuleGetFunction");
  g_cuda_dispatch.cuLaunchKernel =
      (PFN_cuLaunchKernel)load_symbol("cuLaunchKernel");
  g_cuda_dispatch.cuCtxSynchronize =
      (PFN_cuCtxSynchronize)load_symbol("cuCtxSynchronize");
  g_cuda_dispatch.cuMemAlloc =
      (PFN_cuMemAlloc)load_symbol_any("cuMemAlloc_v2", "cuMemAlloc");
  g_cuda_dispatch.cuMemFree =
      (PFN_cuMemFree)load_symbol_any("cuMemFree_v2", "cuMemFree");
  g_cuda_dispatch.cuMemcpyHtoD =
      (PFN_cuMemcpyHtoD)load_symbol_any("cuMemcpyHtoD_v2", "cuMemcpyHtoD");
  g_cuda_dispatch.cuMemcpyDtoH =
      (PFN_cuMemcpyDtoH)load_symbol_any("cuMemcpyDtoH_v2", "cuMemcpyDtoH");

  return g_cuda_dispatch.cuInit != NULL &&
         g_cuda_dispatch.cuDeviceGetCount != NULL &&
         g_cuda_dispatch.cuDeviceGet != NULL &&
         g_cuda_dispatch.cuCtxCreate != NULL &&
         g_cuda_dispatch.cuCtxDestroy != NULL &&
         g_cuda_dispatch.cuModuleLoadDataEx != NULL &&
         g_cuda_dispatch.cuModuleUnload != NULL &&
         g_cuda_dispatch.cuModuleGetFunction != NULL &&
         g_cuda_dispatch.cuLaunchKernel != NULL &&
         g_cuda_dispatch.cuCtxSynchronize != NULL &&
         g_cuda_dispatch.cuMemAlloc != NULL && g_cuda_dispatch.cuMemFree != NULL &&
         g_cuda_dispatch.cuMemcpyHtoD != NULL &&
         g_cuda_dispatch.cuMemcpyDtoH != NULL;
}

static int initialize_cuda_module(void) {
  if (g_cuda_dispatch.cuModuleLoadDataEx(&g_cuda_dispatch.module,
                                         (const void *)kCudaTensorOpsPtx, 0,
                                         NULL, NULL) != CUDA_SUCCESS) {
    set_last_error("cuModuleLoadDataEx failed for embedded tensor PTX");
    return 0;
  }

  if (g_cuda_dispatch.cuModuleGetFunction(&g_cuda_dispatch.kernel_add,
                                          g_cuda_dispatch.module,
                                          "tensor_add") != CUDA_SUCCESS ||
      g_cuda_dispatch.cuModuleGetFunction(&g_cuda_dispatch.kernel_sub,
                                          g_cuda_dispatch.module,
                                          "tensor_sub") != CUDA_SUCCESS ||
      g_cuda_dispatch.cuModuleGetFunction(&g_cuda_dispatch.kernel_mul,
                                          g_cuda_dispatch.module,
                                          "tensor_mul") != CUDA_SUCCESS ||
      g_cuda_dispatch.cuModuleGetFunction(&g_cuda_dispatch.kernel_div,
                                          g_cuda_dispatch.module,
                                          "tensor_div") != CUDA_SUCCESS ||
      g_cuda_dispatch.cuModuleGetFunction(&g_cuda_dispatch.kernel_fma,
                                          g_cuda_dispatch.module,
                                          "tensor_fma") != CUDA_SUCCESS ||
      g_cuda_dispatch.cuModuleGetFunction(&g_cuda_dispatch.kernel_matmul_dense,
                                          g_cuda_dispatch.module,
                                          "tensor_matmul_dense") !=
          CUDA_SUCCESS ||
      g_cuda_dispatch.cuModuleGetFunction(&g_cuda_dispatch.kernel_matmul_packed,
                                          g_cuda_dispatch.module,
                                          "tensor_matmul_packed") !=
          CUDA_SUCCESS) {
    set_last_error("cuModuleGetFunction failed for one or more tensor kernels");
    return 0;
  }

  return 1;
}

static int ensure_initialized(void) {
  if (g_cuda_dispatch.initialized) {
    return g_cuda_dispatch.available;
  }

  g_cuda_dispatch.initialized = 1;
  g_cuda_dispatch.available = 0;
  clear_last_error();

  if (!open_cuda_driver_library()) {
    set_last_error("CUDA driver library was not found at runtime");
    return 0;
  }
  if (!load_cuda_symbols()) {
    set_last_error("CUDA driver loaded but required dispatch symbols are missing");
    return 0;
  }
  if (g_cuda_dispatch.cuInit(0) != CUDA_SUCCESS) {
    set_last_error("cuInit failed for CUDA dispatch module");
    return 0;
  }

  int device_count = 0;
  if (g_cuda_dispatch.cuDeviceGetCount(&device_count) != CUDA_SUCCESS ||
      device_count <= 0) {
    set_last_error("No CUDA devices were detected for CUDA dispatch module");
    return 0;
  }

  CUdevice device = 0;
  if (g_cuda_dispatch.cuDeviceGet(&device, 0) != CUDA_SUCCESS) {
    set_last_error("cuDeviceGet failed for CUDA dispatch module");
    return 0;
  }

  if (g_cuda_dispatch.cuCtxCreate(&g_cuda_dispatch.context, 0, device) !=
      CUDA_SUCCESS) {
    set_last_error("cuCtxCreate failed for CUDA dispatch module");
    return 0;
  }

  if (!initialize_cuda_module()) {
    return 0;
  }

  g_cuda_dispatch.available = 1;
  clear_last_error();
  return 1;
}

static void cleanup_dispatch_state(void) {
  if (g_cuda_dispatch.module != NULL && g_cuda_dispatch.cuModuleUnload != NULL) {
    g_cuda_dispatch.cuModuleUnload(g_cuda_dispatch.module);
    g_cuda_dispatch.module = NULL;
  }
  if (g_cuda_dispatch.context != NULL && g_cuda_dispatch.cuCtxDestroy != NULL) {
    g_cuda_dispatch.cuCtxDestroy(g_cuda_dispatch.context);
    g_cuda_dispatch.context = NULL;
  }
  close_cuda_driver_library();
  memset(&g_cuda_dispatch, 0, sizeof(g_cuda_dispatch));
}

static int launch_binary_kernel(CUfunction kernel, const float *a,
                                const float *b, float *out,
                                int32_t element_count) {
  const size_t bytes = (size_t)element_count * sizeof(float);
  CUdeviceptr d_a = 0;
  CUdeviceptr d_b = 0;
  CUdeviceptr d_out = 0;
  int ok = 0;

  if (g_cuda_dispatch.cuMemAlloc(&d_a, bytes) != CUDA_SUCCESS ||
      g_cuda_dispatch.cuMemAlloc(&d_b, bytes) != CUDA_SUCCESS ||
      g_cuda_dispatch.cuMemAlloc(&d_out, bytes) != CUDA_SUCCESS) {
    set_last_error("cuMemAlloc failed for CUDA tensor binary dispatch");
    goto cleanup;
  }
  if (g_cuda_dispatch.cuMemcpyHtoD(d_a, a, bytes) != CUDA_SUCCESS ||
      g_cuda_dispatch.cuMemcpyHtoD(d_b, b, bytes) != CUDA_SUCCESS) {
    set_last_error("cuMemcpyHtoD failed for CUDA tensor binary dispatch");
    goto cleanup;
  }

  const uint32_t n = (uint32_t)element_count;
  void *params[] = {&d_a, &d_b, &d_out, (void *)&n};
  const unsigned int block_x = 256u;
  const unsigned int grid_x = (n + block_x - 1u) / block_x;

  if (g_cuda_dispatch.cuLaunchKernel(kernel, grid_x, 1u, 1u, block_x, 1u, 1u,
                                     0u, NULL, params, NULL) != CUDA_SUCCESS) {
    set_last_error("cuLaunchKernel failed for CUDA tensor binary dispatch");
    goto cleanup;
  }
  if (g_cuda_dispatch.cuCtxSynchronize() != CUDA_SUCCESS) {
    set_last_error("cuCtxSynchronize failed for CUDA tensor binary dispatch");
    goto cleanup;
  }
  if (g_cuda_dispatch.cuMemcpyDtoH(out, d_out, bytes) != CUDA_SUCCESS) {
    set_last_error("cuMemcpyDtoH failed for CUDA tensor binary dispatch");
    goto cleanup;
  }

  ok = 1;

cleanup:
  if (d_out != 0) {
    g_cuda_dispatch.cuMemFree(d_out);
  }
  if (d_b != 0) {
    g_cuda_dispatch.cuMemFree(d_b);
  }
  if (d_a != 0) {
    g_cuda_dispatch.cuMemFree(d_a);
  }
  return ok;
}

static int launch_fma_kernel(const float *a, const float *b, const float *c,
                             float *out, int32_t element_count) {
  const size_t bytes = (size_t)element_count * sizeof(float);
  CUdeviceptr d_a = 0;
  CUdeviceptr d_b = 0;
  CUdeviceptr d_c = 0;
  CUdeviceptr d_out = 0;
  int ok = 0;

  if (g_cuda_dispatch.cuMemAlloc(&d_a, bytes) != CUDA_SUCCESS ||
      g_cuda_dispatch.cuMemAlloc(&d_b, bytes) != CUDA_SUCCESS ||
      g_cuda_dispatch.cuMemAlloc(&d_c, bytes) != CUDA_SUCCESS ||
      g_cuda_dispatch.cuMemAlloc(&d_out, bytes) != CUDA_SUCCESS) {
    set_last_error("cuMemAlloc failed for CUDA tensor fma dispatch");
    goto cleanup;
  }
  if (g_cuda_dispatch.cuMemcpyHtoD(d_a, a, bytes) != CUDA_SUCCESS ||
      g_cuda_dispatch.cuMemcpyHtoD(d_b, b, bytes) != CUDA_SUCCESS ||
      g_cuda_dispatch.cuMemcpyHtoD(d_c, c, bytes) != CUDA_SUCCESS) {
    set_last_error("cuMemcpyHtoD failed for CUDA tensor fma dispatch");
    goto cleanup;
  }

  const uint32_t n = (uint32_t)element_count;
  void *params[] = {&d_a, &d_b, &d_c, &d_out, (void *)&n};
  const unsigned int block_x = 256u;
  const unsigned int grid_x = (n + block_x - 1u) / block_x;

  if (g_cuda_dispatch.cuLaunchKernel(g_cuda_dispatch.kernel_fma, grid_x, 1u, 1u,
                                     block_x, 1u, 1u, 0u, NULL, params,
                                     NULL) != CUDA_SUCCESS) {
    set_last_error("cuLaunchKernel failed for CUDA tensor fma dispatch");
    goto cleanup;
  }
  if (g_cuda_dispatch.cuCtxSynchronize() != CUDA_SUCCESS) {
    set_last_error("cuCtxSynchronize failed for CUDA tensor fma dispatch");
    goto cleanup;
  }
  if (g_cuda_dispatch.cuMemcpyDtoH(out, d_out, bytes) != CUDA_SUCCESS) {
    set_last_error("cuMemcpyDtoH failed for CUDA tensor fma dispatch");
    goto cleanup;
  }

  ok = 1;

cleanup:
  if (d_out != 0) {
    g_cuda_dispatch.cuMemFree(d_out);
  }
  if (d_c != 0) {
    g_cuda_dispatch.cuMemFree(d_c);
  }
  if (d_b != 0) {
    g_cuda_dispatch.cuMemFree(d_b);
  }
  if (d_a != 0) {
    g_cuda_dispatch.cuMemFree(d_a);
  }
  return ok;
}

static int validate_matmul_descriptor(const NeuronGpuMatMulDispatchDesc *desc) {
  if (desc == NULL || desc->a == NULL || desc->out == NULL || desc->m <= 0 ||
      desc->n <= 0 || desc->k <= 0) {
    set_last_error("Invalid CUDA tensor matmul descriptor");
    return 0;
  }
  if (desc->b == NULL && desc->packed_b == NULL) {
    set_last_error("CUDA tensor matmul requires dense or packed B");
    return 0;
  }
  if (desc->packed_b != NULL) {
    if (desc->packed_b->rows != desc->k || desc->packed_b->cols != desc->n ||
        desc->packed_b->kc <= 0 || desc->packed_b->nc <= 0 ||
        desc->packed_b->kBlocks <= 0 || desc->packed_b->nBlocks <= 0 ||
        desc->packed_b->offsets == NULL || desc->packed_b->data == NULL ||
        desc->packed_b->panelCount == 0) {
      set_last_error("CUDA tensor matmul packed-B metadata is invalid");
      return 0;
    }
    if (desc->packed_b->panelCount >
        (size_t)desc->packed_b->kBlocks * (size_t)desc->packed_b->nBlocks) {
      set_last_error("CUDA tensor matmul packed-B panel count is out of range");
      return 0;
    }
  }
  if (desc->activation != 0 && desc->activation != 1 && desc->activation != 2) {
    set_last_error("CUDA tensor matmul activation is unsupported: %d",
                   desc->activation);
    return 0;
  }
  if (desc->bias != NULL) {
    if (desc->bias_cols != desc->n ||
        (desc->bias_rows != 1 && desc->bias_rows != desc->m)) {
      set_last_error("CUDA tensor matmul bias shape mismatch");
      return 0;
    }
  }
  if (desc->residual != NULL) {
    if (desc->residual_cols != desc->n) {
      set_last_error("CUDA tensor matmul residual shape mismatch");
      return 0;
    }
  }
  return 1;
}

static int alloc_and_copy(CUdeviceptr *dst, const void *src, size_t bytes,
                          const char *label) {
  if (dst == NULL || src == NULL || bytes == 0) {
    set_last_error("Invalid CUDA allocation request for %s", label);
    return 0;
  }
  if (g_cuda_dispatch.cuMemAlloc(dst, bytes) != CUDA_SUCCESS) {
    set_last_error("cuMemAlloc failed for %s", label);
    return 0;
  }
  if (g_cuda_dispatch.cuMemcpyHtoD(*dst, src, bytes) != CUDA_SUCCESS) {
    set_last_error("cuMemcpyHtoD failed for %s", label);
    return 0;
  }
  return 1;
}

static int alloc_zeroed(CUdeviceptr *dst, size_t bytes, const char *label) {
  uint32_t zero = 0;
  if (dst == NULL || bytes == 0) {
    set_last_error("Invalid CUDA allocation request for %s", label);
    return 0;
  }
  if (g_cuda_dispatch.cuMemAlloc(dst, bytes) != CUDA_SUCCESS) {
    set_last_error("cuMemAlloc failed for %s", label);
    return 0;
  }
  if (g_cuda_dispatch.cuMemcpyHtoD(*dst, &zero, sizeof(zero)) != CUDA_SUCCESS) {
    set_last_error("cuMemcpyHtoD failed for %s", label);
    return 0;
  }
  return 1;
}

static size_t compute_packed_data_float_count(const NeuronPackedMatrix *packed) {
  size_t max_end = 0;
  const size_t panels = packed->panelCount;
  for (size_t panel_index = 0; panel_index < panels; ++panel_index) {
    const int32_t n_block = (int32_t)(panel_index / (size_t)packed->kBlocks);
    const int32_t k_block = (int32_t)(panel_index % (size_t)packed->kBlocks);
    if (n_block < 0 || n_block >= packed->nBlocks || k_block < 0 ||
        k_block >= packed->kBlocks) {
      return 0;
    }

    const int32_t nc0 = n_block * packed->nc;
    const int32_t kc0 = k_block * packed->kc;
    const int32_t nc_cur =
        (packed->cols - nc0) < packed->nc ? (packed->cols - nc0) : packed->nc;
    const int32_t kc_cur =
        (packed->rows - kc0) < packed->kc ? (packed->rows - kc0) : packed->kc;
    if (nc_cur <= 0 || kc_cur <= 0) {
      return 0;
    }

    const size_t panel_size = (size_t)nc_cur * (size_t)kc_cur;
    const size_t panel_offset = packed->offsets[panel_index];
    if (panel_offset > SIZE_MAX - panel_size) {
      return 0;
    }
    const size_t panel_end = panel_offset + panel_size;
    if (panel_end > max_end) {
      max_end = panel_end;
    }
  }
  return max_end;
}

static int launch_matmul_dense_kernel(const NeuronGpuMatMulDispatchDesc *desc) {
  CUdeviceptr d_a = 0;
  CUdeviceptr d_b = 0;
  CUdeviceptr d_out = 0;
  CUdeviceptr d_bias = 0;
  CUdeviceptr d_residual = 0;
  int ok = 0;

  const size_t a_bytes =
      (size_t)desc->m * (size_t)desc->k * sizeof(float);
  const size_t b_bytes =
      (size_t)desc->k * (size_t)desc->n * sizeof(float);
  const size_t out_bytes =
      (size_t)desc->m * (size_t)desc->n * sizeof(float);
  const size_t bias_bytes = (desc->bias != NULL && desc->bias_rows > 0 &&
                             desc->bias_cols > 0)
                                ? (size_t)desc->bias_rows *
                                      (size_t)desc->bias_cols * sizeof(float)
                                : sizeof(uint32_t);
  const size_t residual_bytes =
      (desc->residual != NULL && desc->m > 0 && desc->residual_cols > 0)
          ? (size_t)desc->m * (size_t)desc->residual_cols * sizeof(float)
          : sizeof(uint32_t);

  if (!alloc_and_copy(&d_a, desc->a, a_bytes, "CUDA matmul A") ||
      !alloc_and_copy(&d_b, desc->b, b_bytes, "CUDA matmul B")) {
    goto cleanup;
  }
  if (g_cuda_dispatch.cuMemAlloc(&d_out, out_bytes) != CUDA_SUCCESS) {
    set_last_error("cuMemAlloc failed for CUDA matmul output");
    goto cleanup;
  }
  if (desc->accumulate &&
      g_cuda_dispatch.cuMemcpyHtoD(d_out, desc->out, out_bytes) != CUDA_SUCCESS) {
    set_last_error("cuMemcpyHtoD failed for CUDA matmul accumulated output");
    goto cleanup;
  }
  if (desc->bias != NULL) {
    if (!alloc_and_copy(&d_bias, desc->bias, bias_bytes, "CUDA matmul bias")) {
      goto cleanup;
    }
  } else if (!alloc_zeroed(&d_bias, sizeof(uint32_t), "CUDA matmul bias placeholder")) {
    goto cleanup;
  }
  if (desc->residual != NULL) {
    if (!alloc_and_copy(&d_residual, desc->residual, residual_bytes,
                        "CUDA matmul residual")) {
      goto cleanup;
    }
  } else if (!alloc_zeroed(&d_residual, sizeof(uint32_t),
                           "CUDA matmul residual placeholder")) {
    goto cleanup;
  }

  int32_t m = desc->m;
  int32_t n = desc->n;
  int32_t k = desc->k;
  int32_t bias_rows = desc->bias != NULL ? desc->bias_rows : 0;
  int32_t bias_cols = desc->bias != NULL ? desc->bias_cols : 0;
  int32_t residual_cols = desc->residual != NULL ? desc->residual_cols : 0;
  int32_t activation = desc->activation;
  int32_t accumulate = desc->accumulate ? 1 : 0;

  void *params[] = {&d_out,       &d_a,        &d_b,        &d_bias,
                    &d_residual,  &m,          &n,          &k,
                    &bias_rows,   &bias_cols,  &residual_cols,
                    &activation,  &accumulate};

  const unsigned int block_x = 16u;
  const unsigned int block_y = 16u;
  const unsigned int grid_x = ((unsigned int)n + block_x - 1u) / block_x;
  const unsigned int grid_y = ((unsigned int)m + block_y - 1u) / block_y;

  if (g_cuda_dispatch.cuLaunchKernel(g_cuda_dispatch.kernel_matmul_dense, grid_x,
                                     grid_y, 1u, block_x, block_y, 1u, 0u,
                                     NULL, params, NULL) != CUDA_SUCCESS) {
    set_last_error("cuLaunchKernel failed for CUDA matmul dense dispatch");
    goto cleanup;
  }
  if (g_cuda_dispatch.cuCtxSynchronize() != CUDA_SUCCESS) {
    set_last_error("cuCtxSynchronize failed for CUDA matmul dense dispatch");
    goto cleanup;
  }
  if (g_cuda_dispatch.cuMemcpyDtoH(desc->out, d_out, out_bytes) != CUDA_SUCCESS) {
    set_last_error("cuMemcpyDtoH failed for CUDA matmul dense dispatch");
    goto cleanup;
  }

  ok = 1;

cleanup:
  if (d_residual != 0) {
    g_cuda_dispatch.cuMemFree(d_residual);
  }
  if (d_bias != 0) {
    g_cuda_dispatch.cuMemFree(d_bias);
  }
  if (d_out != 0) {
    g_cuda_dispatch.cuMemFree(d_out);
  }
  if (d_b != 0) {
    g_cuda_dispatch.cuMemFree(d_b);
  }
  if (d_a != 0) {
    g_cuda_dispatch.cuMemFree(d_a);
  }
  return ok;
}

static int launch_matmul_packed_kernel(const NeuronGpuMatMulDispatchDesc *desc) {
  CUdeviceptr d_a = 0;
  CUdeviceptr d_packed_data = 0;
  CUdeviceptr d_offsets = 0;
  CUdeviceptr d_out = 0;
  CUdeviceptr d_bias = 0;
  CUdeviceptr d_residual = 0;
  uint64_t *offsets64 = NULL;
  int ok = 0;

  const NeuronPackedMatrix *packed = desc->packed_b;
  const size_t packed_data_floats = compute_packed_data_float_count(packed);
  if (packed_data_floats == 0) {
    set_last_error("CUDA packed matmul failed to compute packed data footprint");
    goto cleanup;
  }
  if (packed_data_floats > SIZE_MAX / sizeof(float)) {
    set_last_error("CUDA packed matmul packed data footprint overflow");
    goto cleanup;
  }
  const size_t packed_data_bytes = packed_data_floats * sizeof(float);

  if (packed->panelCount > (size_t)INT32_MAX) {
    set_last_error("CUDA packed matmul panel count exceeds int32 range");
    goto cleanup;
  }
  if (packed->panelCount > SIZE_MAX / sizeof(uint64_t)) {
    set_last_error("CUDA packed matmul offsets footprint overflow");
    goto cleanup;
  }
  const size_t offsets_bytes = packed->panelCount * sizeof(uint64_t);
  offsets64 = (uint64_t *)malloc(offsets_bytes);
  if (offsets64 == NULL) {
    set_last_error("Failed to allocate temporary offsets buffer");
    goto cleanup;
  }
  for (size_t i = 0; i < packed->panelCount; ++i) {
    offsets64[i] = (uint64_t)packed->offsets[i];
  }

  const size_t a_bytes =
      (size_t)desc->m * (size_t)desc->k * sizeof(float);
  const size_t out_bytes =
      (size_t)desc->m * (size_t)desc->n * sizeof(float);
  const size_t bias_bytes = (desc->bias != NULL && desc->bias_rows > 0 &&
                             desc->bias_cols > 0)
                                ? (size_t)desc->bias_rows *
                                      (size_t)desc->bias_cols * sizeof(float)
                                : sizeof(uint32_t);
  const size_t residual_bytes =
      (desc->residual != NULL && desc->m > 0 && desc->residual_cols > 0)
          ? (size_t)desc->m * (size_t)desc->residual_cols * sizeof(float)
          : sizeof(uint32_t);

  if (!alloc_and_copy(&d_a, desc->a, a_bytes, "CUDA packed matmul A") ||
      !alloc_and_copy(&d_packed_data, packed->data, packed_data_bytes,
                      "CUDA packed matmul packed-data") ||
      !alloc_and_copy(&d_offsets, offsets64, offsets_bytes,
                      "CUDA packed matmul offsets")) {
    goto cleanup;
  }
  if (g_cuda_dispatch.cuMemAlloc(&d_out, out_bytes) != CUDA_SUCCESS) {
    set_last_error("cuMemAlloc failed for CUDA packed matmul output");
    goto cleanup;
  }
  if (desc->accumulate &&
      g_cuda_dispatch.cuMemcpyHtoD(d_out, desc->out, out_bytes) != CUDA_SUCCESS) {
    set_last_error("cuMemcpyHtoD failed for CUDA packed matmul accumulated output");
    goto cleanup;
  }
  if (desc->bias != NULL) {
    if (!alloc_and_copy(&d_bias, desc->bias, bias_bytes,
                        "CUDA packed matmul bias")) {
      goto cleanup;
    }
  } else if (!alloc_zeroed(&d_bias, sizeof(uint32_t),
                           "CUDA packed matmul bias placeholder")) {
    goto cleanup;
  }
  if (desc->residual != NULL) {
    if (!alloc_and_copy(&d_residual, desc->residual, residual_bytes,
                        "CUDA packed matmul residual")) {
      goto cleanup;
    }
  } else if (!alloc_zeroed(&d_residual, sizeof(uint32_t),
                           "CUDA packed matmul residual placeholder")) {
    goto cleanup;
  }

  int32_t m = desc->m;
  int32_t n = desc->n;
  int32_t k = desc->k;
  int32_t kc = packed->kc;
  int32_t nc = packed->nc;
  int32_t k_blocks = packed->kBlocks;
  int32_t n_blocks = packed->nBlocks;
  int32_t panel_count = (int32_t)packed->panelCount;
  int32_t bias_rows = desc->bias != NULL ? desc->bias_rows : 0;
  int32_t bias_cols = desc->bias != NULL ? desc->bias_cols : 0;
  int32_t residual_cols = desc->residual != NULL ? desc->residual_cols : 0;
  int32_t activation = desc->activation;
  int32_t accumulate = desc->accumulate ? 1 : 0;

  void *params[] = {&d_out,         &d_a,           &d_packed_data, &d_offsets,
                    &d_bias,        &d_residual,    &m,             &n,
                    &k,             &kc,            &nc,            &k_blocks,
                    &n_blocks,      &panel_count,   &bias_rows,     &bias_cols,
                    &residual_cols, &activation,    &accumulate};

  const unsigned int block_x = 16u;
  const unsigned int block_y = 16u;
  const unsigned int grid_x = ((unsigned int)n + block_x - 1u) / block_x;
  const unsigned int grid_y = ((unsigned int)m + block_y - 1u) / block_y;

  if (g_cuda_dispatch.cuLaunchKernel(g_cuda_dispatch.kernel_matmul_packed, grid_x,
                                     grid_y, 1u, block_x, block_y, 1u, 0u,
                                     NULL, params, NULL) != CUDA_SUCCESS) {
    set_last_error("cuLaunchKernel failed for CUDA matmul packed dispatch");
    goto cleanup;
  }
  if (g_cuda_dispatch.cuCtxSynchronize() != CUDA_SUCCESS) {
    set_last_error("cuCtxSynchronize failed for CUDA matmul packed dispatch");
    goto cleanup;
  }
  if (g_cuda_dispatch.cuMemcpyDtoH(desc->out, d_out, out_bytes) != CUDA_SUCCESS) {
    set_last_error("cuMemcpyDtoH failed for CUDA matmul packed dispatch");
    goto cleanup;
  }

  ok = 1;

cleanup:
  if (offsets64 != NULL) {
    free(offsets64);
  }
  if (d_residual != 0) {
    g_cuda_dispatch.cuMemFree(d_residual);
  }
  if (d_bias != 0) {
    g_cuda_dispatch.cuMemFree(d_bias);
  }
  if (d_out != 0) {
    g_cuda_dispatch.cuMemFree(d_out);
  }
  if (d_offsets != 0) {
    g_cuda_dispatch.cuMemFree(d_offsets);
  }
  if (d_packed_data != 0) {
    g_cuda_dispatch.cuMemFree(d_packed_data);
  }
  if (d_a != 0) {
    g_cuda_dispatch.cuMemFree(d_a);
  }
  return ok;
}

int neuron_gpu_cuda_try_initialize(char *error_buffer, size_t error_size) {
  const int ok = ensure_initialized();
  copy_error_out(error_buffer, error_size);
  return ok ? 1 : 0;
}

void neuron_gpu_cuda_shutdown(void) { cleanup_dispatch_state(); }

int neuron_gpu_cuda_supports_op(NeuronGpuOpKind op) {
  if (ensure_initialized() == 0) {
    return 0;
  }

  switch (op) {
  case NEURON_GPU_OP_TENSOR_ADD:
  case NEURON_GPU_OP_TENSOR_SUB:
  case NEURON_GPU_OP_TENSOR_MUL:
  case NEURON_GPU_OP_TENSOR_DIV:
  case NEURON_GPU_OP_TENSOR_FMA:
  case NEURON_GPU_OP_TENSOR_MATMUL:
    return 1;
  default:
    return 0;
  }
}

int neuron_gpu_cuda_dispatch_binary(NeuronGpuOpKind op, const float *a,
                                    const float *b, float *out,
                                    int32_t element_count, char *error_buffer,
                                    size_t error_size) {
  if (a == NULL || b == NULL || out == NULL || element_count <= 0) {
    set_last_error("Invalid CUDA tensor binary arguments");
    copy_error_out(error_buffer, error_size);
    return -1;
  }
  if (ensure_initialized() == 0) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  CUfunction kernel = NULL;
  switch (op) {
  case NEURON_GPU_OP_TENSOR_ADD:
    kernel = g_cuda_dispatch.kernel_add;
    break;
  case NEURON_GPU_OP_TENSOR_SUB:
    kernel = g_cuda_dispatch.kernel_sub;
    break;
  case NEURON_GPU_OP_TENSOR_MUL:
    kernel = g_cuda_dispatch.kernel_mul;
    break;
  case NEURON_GPU_OP_TENSOR_DIV:
    kernel = g_cuda_dispatch.kernel_div;
    break;
  default:
    set_last_error("Unsupported CUDA tensor binary op kind: %d", (int)op);
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  if (!launch_binary_kernel(kernel, a, b, out, element_count)) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  clear_last_error();
  copy_error_out(error_buffer, error_size);
  return 0;
}

int neuron_gpu_cuda_dispatch_matmul(const NeuronGpuMatMulDispatchDesc *desc,
                                    char *error_buffer, size_t error_size) {
  if (!validate_matmul_descriptor(desc)) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }
  if (ensure_initialized() == 0) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }
  if ((desc->packed_b != NULL && !launch_matmul_packed_kernel(desc)) ||
      (desc->packed_b == NULL && !launch_matmul_dense_kernel(desc))) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  clear_last_error();
  copy_error_out(error_buffer, error_size);
  return 0;
}

int neuron_gpu_cuda_dispatch_fma(const float *a, const float *b, const float *c,
                                 float *out, int32_t element_count,
                                 char *error_buffer, size_t error_size) {
  if (a == NULL || b == NULL || c == NULL || out == NULL || element_count <= 0) {
    set_last_error("Invalid CUDA tensor fma arguments");
    copy_error_out(error_buffer, error_size);
    return -1;
  }
  if (ensure_initialized() == 0) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  if (!launch_fma_kernel(a, b, c, out, element_count)) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  clear_last_error();
  copy_error_out(error_buffer, error_size);
  return 0;
}

#else

static void write_disabled_error(char *error_buffer, size_t error_size) {
  if (error_buffer != NULL && error_size > 0) {
    snprintf(error_buffer, error_size,
             "CUDA backend is disabled at build time");
  }
}

int neuron_gpu_cuda_try_initialize(char *error_buffer, size_t error_size) {
  write_disabled_error(error_buffer, error_size);
  return 0;
}

void neuron_gpu_cuda_shutdown(void) {}

int neuron_gpu_cuda_supports_op(NeuronGpuOpKind op) {
  (void)op;
  return 0;
}

int neuron_gpu_cuda_dispatch_binary(NeuronGpuOpKind op, const float *a,
                                    const float *b, float *out,
                                    int32_t element_count, char *error_buffer,
                                    size_t error_size) {
  (void)op;
  (void)a;
  (void)b;
  (void)out;
  (void)element_count;
  write_disabled_error(error_buffer, error_size);
  return -1;
}

int neuron_gpu_cuda_dispatch_fma(const float *a, const float *b, const float *c,
                                 float *out, int32_t element_count,
                                 char *error_buffer, size_t error_size) {
  (void)a;
  (void)b;
  (void)c;
  (void)out;
  (void)element_count;
  write_disabled_error(error_buffer, error_size);
  return -1;
}

int neuron_gpu_cuda_dispatch_matmul(const NeuronGpuMatMulDispatchDesc *desc,
                                    char *error_buffer, size_t error_size) {
  (void)desc;
  write_disabled_error(error_buffer, error_size);
  return -1;
}

#endif
