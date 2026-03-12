#ifndef NEURON_NN_H
#define NEURON_NN_H

#include "neuron_runtime_export.h"
#include "neuron_tensor.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  NEURON_ACTIVATION_LINEAR = 0,
  NEURON_ACTIVATION_RELU = 1,
  NEURON_ACTIVATION_SIGMOID = 2,
  NEURON_ACTIVATION_TANH = 3,
  NEURON_ACTIVATION_LEAKY_RELU = 4,
  NEURON_ACTIVATION_SOFTMAX = 5,
} NeuronActivationType;

typedef enum {
  NEURON_INIT_DEFAULT = 0,
  NEURON_INIT_XAVIER_UNIFORM = 1,
  NEURON_INIT_KAIMING_UNIFORM = 2,
} NeuronInitStrategy;

typedef enum {
  NEURON_LAYER_DENSE = 0,
  NEURON_LAYER_CONV2D = 1,
  NEURON_LAYER_MAXPOOL2D = 2,
  NEURON_LAYER_DROPOUT = 3,
} NeuronLayerKind;

typedef struct {
  int32_t input_size;
  int32_t output_size;
  NeuronActivationType activation;
  float activation_param;
  NeuronInitStrategy init_strategy;

  NeuronTensor *weights;
  NeuronTensor *bias;
  NeuronPackedMatrix *packed_weights;
  uint64_t packed_weights_version;

  NeuronTensor *grad_weights;
  NeuronTensor *grad_bias;

  NeuronTensor *cache_input;
  NeuronTensor *cache_linear;
} NeuronDenseLayer;

typedef struct {
  int32_t input_channels;
  int32_t output_channels;
  int32_t kernel_h;
  int32_t kernel_w;
  int32_t stride_h;
  int32_t stride_w;
  int32_t padding_h;
  int32_t padding_w;
  NeuronActivationType activation;
  float activation_param;
  NeuronInitStrategy init_strategy;

  NeuronTensor *weights;
  NeuronTensor *bias;
  NeuronTensor *grad_weights;
  NeuronTensor *grad_bias;

  NeuronTensor *cache_input;
  NeuronTensor *cache_linear;
  NeuronTensor *cache_columns;
} NeuronConv2DLayer;

typedef struct {
  int32_t kernel_h;
  int32_t kernel_w;
  int32_t stride_h;
  int32_t stride_w;

  int32_t *cache_indices;
  int32_t cache_index_count;
  int32_t cache_input_shape[4];
} NeuronMaxPool2DLayer;

typedef struct {
  float drop_probability;
  float scale;
  uint32_t seed;
  uint32_t rng_state;
  NeuronTensor *mask;
} NeuronDropoutLayer;

typedef struct {
  NeuronDenseLayer **layers;
  int32_t count;
  int32_t capacity;
  int32_t *layer_kinds;
  void **layer_objects;
} NeuronSequentialModel;

typedef struct {
  float learning_rate;
  float weight_decay;
} NeuronSGDOptimizer;

typedef struct {
  float learning_rate;
  float weight_decay;
  float beta1;
  float beta2;
  float epsilon;
  uint64_t timestep;
  const NeuronSequentialModel *model;
  int32_t layer_capacity;
  NeuronTensor **weight_moments;
  NeuronTensor **weight_velocities;
  NeuronTensor **bias_moments;
  NeuronTensor **bias_velocities;
} NeuronAdamOptimizer;

typedef struct {
  NeuronTensor *features;
  NeuronTensor *targets;
  int32_t sample_count;
  int32_t feature_dim;
  int32_t target_dim;
} NeuronDataset;

typedef struct {
  const NeuronDataset *dataset;
  int32_t batch_size;
  int32_t cursor;
  int32_t shuffle;
  uint32_t rng_state;
  int32_t *indices;
} NeuronDataLoader;

// Layers
NEURON_RUNTIME_API NeuronDenseLayer *
neuron_layer_dense_create(int32_t input_size, int32_t output_size,
                          NeuronActivationType activation);
NEURON_RUNTIME_API void neuron_layer_dense_free(NeuronDenseLayer *layer);
NEURON_RUNTIME_API void
neuron_layer_dense_apply_init(NeuronDenseLayer *layer,
                              NeuronInitStrategy strategy);
NEURON_RUNTIME_API NeuronTensor *
neuron_layer_dense_forward(NeuronDenseLayer *layer, NeuronTensor *input,
                           int training);
NEURON_RUNTIME_API NeuronTensor *
neuron_layer_dense_backward(NeuronDenseLayer *layer,
                            NeuronTensor *grad_output);

NEURON_RUNTIME_API NeuronConv2DLayer *
neuron_layer_conv2d_create(int32_t input_channels, int32_t output_channels,
                           int32_t kernel_h, int32_t kernel_w,
                           int32_t stride_h, int32_t stride_w,
                           int32_t padding_h, int32_t padding_w,
                           NeuronActivationType activation);
NEURON_RUNTIME_API void neuron_layer_conv2d_free(NeuronConv2DLayer *layer);
NEURON_RUNTIME_API void
neuron_layer_conv2d_apply_init(NeuronConv2DLayer *layer,
                               NeuronInitStrategy strategy);
NEURON_RUNTIME_API NeuronTensor *
neuron_layer_conv2d_forward(NeuronConv2DLayer *layer, NeuronTensor *input,
                            int training);
NEURON_RUNTIME_API NeuronTensor *
neuron_layer_conv2d_backward(NeuronConv2DLayer *layer,
                             NeuronTensor *grad_output);

NEURON_RUNTIME_API NeuronMaxPool2DLayer *
neuron_layer_maxpool2d_create(int32_t kernel_h, int32_t kernel_w,
                              int32_t stride_h, int32_t stride_w);
NEURON_RUNTIME_API void
neuron_layer_maxpool2d_free(NeuronMaxPool2DLayer *layer);
NEURON_RUNTIME_API NeuronTensor *
neuron_layer_maxpool2d_forward(NeuronMaxPool2DLayer *layer,
                               NeuronTensor *input, int training);
NEURON_RUNTIME_API NeuronTensor *
neuron_layer_maxpool2d_backward(NeuronMaxPool2DLayer *layer,
                                NeuronTensor *grad_output);

NEURON_RUNTIME_API NeuronDropoutLayer *
neuron_layer_dropout_create(float drop_probability, uint32_t seed);
NEURON_RUNTIME_API void
neuron_layer_dropout_free(NeuronDropoutLayer *layer);
NEURON_RUNTIME_API NeuronTensor *
neuron_layer_dropout_forward(NeuronDropoutLayer *layer, NeuronTensor *input,
                             int training);
NEURON_RUNTIME_API NeuronTensor *
neuron_layer_dropout_backward(NeuronDropoutLayer *layer,
                              NeuronTensor *grad_output);

// Model
NEURON_RUNTIME_API NeuronSequentialModel *neuron_model_create(void);
NEURON_RUNTIME_API void neuron_model_free(NeuronSequentialModel *model);
NEURON_RUNTIME_API int neuron_model_add_layer(NeuronSequentialModel *model,
                                              NeuronDenseLayer *layer);
NEURON_RUNTIME_API int
neuron_model_add_conv2d_layer(NeuronSequentialModel *model,
                              NeuronConv2DLayer *layer);
NEURON_RUNTIME_API int
neuron_model_add_maxpool2d_layer(NeuronSequentialModel *model,
                                 NeuronMaxPool2DLayer *layer);
NEURON_RUNTIME_API int
neuron_model_add_dropout_layer(NeuronSequentialModel *model,
                               NeuronDropoutLayer *layer);
NEURON_RUNTIME_API NeuronTensor *
neuron_model_forward(NeuronSequentialModel *model, NeuronTensor *input,
                     int training);
NEURON_RUNTIME_API int neuron_model_backward(NeuronSequentialModel *model,
                                             NeuronTensor *grad);
NEURON_RUNTIME_API void
neuron_model_zero_grad(NeuronSequentialModel *model);
NEURON_RUNTIME_API int
neuron_model_save(const NeuronSequentialModel *model, const char *path);
NEURON_RUNTIME_API NeuronSequentialModel *
neuron_model_load(const char *path);

// Autograd
NEURON_RUNTIME_API float
neuron_autograd_mse_loss(NeuronTensor *prediction, NeuronTensor *target,
                         NeuronTensor **out_grad);
NEURON_RUNTIME_API float
neuron_autograd_cross_entropy_loss(NeuronTensor *prediction,
                                   NeuronTensor *target,
                                   NeuronTensor **out_grad);

// Optimizer
NEURON_RUNTIME_API void
neuron_optimizer_sgd_init(NeuronSGDOptimizer *opt, float learning_rate,
                          float weight_decay);
NEURON_RUNTIME_API void
neuron_optimizer_sgd_step(NeuronSequentialModel *model,
                          const NeuronSGDOptimizer *opt);
NEURON_RUNTIME_API int
neuron_optimizer_adam_init(NeuronAdamOptimizer *opt,
                           const NeuronSequentialModel *model,
                           float learning_rate, float weight_decay);
NEURON_RUNTIME_API void
neuron_optimizer_adam_free(NeuronAdamOptimizer *opt);
NEURON_RUNTIME_API void
neuron_optimizer_adam_step(NeuronSequentialModel *model,
                           NeuronAdamOptimizer *opt);

// Dataset / DataLoader
NEURON_RUNTIME_API NeuronDataset *
neuron_dataset_create(int32_t sample_count, int32_t feature_dim,
                      int32_t target_dim);
NEURON_RUNTIME_API void neuron_dataset_free(NeuronDataset *dataset);
NEURON_RUNTIME_API int
neuron_dataset_set_sample(NeuronDataset *dataset, int32_t index,
                          const float *features, const float *targets);

NEURON_RUNTIME_API NeuronDataLoader *
neuron_dataloader_create(const NeuronDataset *dataset, int32_t batch_size,
                         int shuffle, uint32_t seed);
NEURON_RUNTIME_API void neuron_dataloader_free(NeuronDataLoader *loader);
NEURON_RUNTIME_API void neuron_dataloader_reset(NeuronDataLoader *loader);

// Returns: 1 => batch produced, 0 => end-of-data, -1 => error.
NEURON_RUNTIME_API int
neuron_dataloader_next(NeuronDataLoader *loader, NeuronTensor **out_features,
                       NeuronTensor **out_targets, int32_t *out_count);

// End-to-end runtime smoke test helper for NPP integration.
NEURON_RUNTIME_API int64_t neuron_nn_self_test(void);

#ifdef __cplusplus
}
#endif

#endif // NEURON_NN_H
