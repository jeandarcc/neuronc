#include "neuron_nn.h"
#include "neuron_gpu.h"
#include "neuron_runtime.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const float kDefaultLeakyReluAlpha = 0.01f;
static const float kCrossEntropyEpsilon = 1.0e-7f;
static const char kModelFileMagic[8] = {'N', 'P', 'P', 'N', 'N', '0', '1',
                                        '\0'};
static const uint32_t kModelFileVersion = 2u;

static void model_layer_param_tensors(int32_t layer_kind, void *layer_object,
                                      NeuronTensor **out_weights,
                                      NeuronTensor **out_grad_weights,
                                      NeuronTensor **out_bias,
                                      NeuronTensor **out_grad_bias);

static int tensor_is_valid(const NeuronTensor *t) {
  return t != NULL && t->data != NULL && t->shape != NULL && t->dimensions > 0 &&
         t->size >= 0;
}

static int tensor_is_matrix(const NeuronTensor *t) {
  return tensor_is_valid(t) && t->dimensions == 2 && t->shape[0] > 0 &&
         t->shape[1] > 0;
}

static NeuronTensor *tensor_clone(const NeuronTensor *src) {
  if (!tensor_is_valid(src)) {
    return NULL;
  }

  NeuronTensor *dst = neuron_tensor_create(src->dimensions, src->shape);
  if (dst == NULL) {
    return NULL;
  }
  memcpy(dst->data, src->data, (size_t)src->size * sizeof(float));
  return dst;
}

static NeuronTensor *tensor_create_like(const NeuronTensor *src) {
  if (!tensor_is_valid(src)) {
    return NULL;
  }
  return neuron_tensor_create(src->dimensions, src->shape);
}

static void tensor_zero(NeuronTensor *t) {
  if (!tensor_is_valid(t)) {
    return;
  }
  memset(t->data, 0, (size_t)t->size * sizeof(float));
}

static int tensor_has_same_shape(const NeuronTensor *a, const NeuronTensor *b) {
  int32_t i = 0;
  if (!tensor_is_valid(a) || !tensor_is_valid(b) ||
      a->dimensions != b->dimensions || a->size != b->size) {
    return 0;
  }
  for (i = 0; i < a->dimensions; ++i) {
    if (a->shape[i] != b->shape[i]) {
      return 0;
    }
  }
  return 1;
}

static int tensor_is_4d_nchw(const NeuronTensor *t) {
  return tensor_is_valid(t) && t->dimensions == 4 && t->shape[0] > 0 &&
         t->shape[1] > 0 && t->shape[2] > 0 && t->shape[3] > 0;
}

static int32_t tensor_offset_nchw(const NeuronTensor *t, int32_t n, int32_t c,
                                  int32_t h, int32_t w) {
  return (((n * t->shape[1] + c) * t->shape[2] + h) * t->shape[3] + w);
}

static NeuronTensor *tensor_create_4d(int32_t n, int32_t c, int32_t h,
                                      int32_t w) {
  int32_t shape[4] = {n, c, h, w};
  return neuron_tensor_create(4, shape);
}

static NeuronTensorExecHint nn_exec_hint(void) {
  return neuron_gpu_scope_is_active() ? NEURON_TENSOR_EXEC_GPU_PREFER
                                      : NEURON_TENSOR_EXEC_CPU_ONLY;
}

static NeuronTensor *tensor_transpose_2d(const NeuronTensor *src) {
  if (!tensor_is_matrix(src)) {
    return NULL;
  }

  int32_t out_shape[2] = {src->shape[1], src->shape[0]};
  NeuronTensor *out = neuron_tensor_create(2, out_shape);
  if (out == NULL) {
    return NULL;
  }

  int32_t rows = src->shape[0];
  int32_t cols = src->shape[1];
  for (int32_t r = 0; r < rows; ++r) {
    for (int32_t c = 0; c < cols; ++c) {
      out->data[c * rows + r] = src->data[r * cols + c];
    }
  }
  return out;
}

static void tensor_scale_inplace(NeuronTensor *t, float factor) {
  if (!tensor_is_valid(t)) {
    return;
  }
  for (int32_t i = 0; i < t->size; ++i) {
    t->data[i] *= factor;
  }
}

static void add_bias_inplace(NeuronTensor *matrix, const NeuronTensor *bias) {
  if (!tensor_is_matrix(matrix) || !tensor_is_matrix(bias)) {
    return;
  }
  if (bias->shape[0] != 1 || bias->shape[1] != matrix->shape[1]) {
    return;
  }

  int32_t rows = matrix->shape[0];
  int32_t cols = matrix->shape[1];
  for (int32_t r = 0; r < rows; ++r) {
    for (int32_t c = 0; c < cols; ++c) {
      matrix->data[r * cols + c] += bias->data[c];
    }
  }
}

static NeuronTensor *tensor_sum_rows(const NeuronTensor *matrix) {
  if (!tensor_is_matrix(matrix)) {
    return NULL;
  }

  int32_t out_shape[2] = {1, matrix->shape[1]};
  NeuronTensor *sum = neuron_tensor_create(2, out_shape);
  if (sum == NULL) {
    return NULL;
  }

  int32_t rows = matrix->shape[0];
  int32_t cols = matrix->shape[1];
  for (int32_t r = 0; r < rows; ++r) {
    for (int32_t c = 0; c < cols; ++c) {
      sum->data[c] += matrix->data[r * cols + c];
    }
  }
  return sum;
}

static float activation_param_or_default(NeuronActivationType activation,
                                         float activation_param) {
  if (activation == NEURON_ACTIVATION_LEAKY_RELU) {
    return activation_param > 0.0f ? activation_param
                                   : kDefaultLeakyReluAlpha;
  }
  return activation_param;
}

static void apply_activation_inplace(NeuronTensor *t,
                                     NeuronActivationType activation,
                                     float activation_param) {
  int32_t i = 0;
  if (!tensor_is_valid(t)) {
    return;
  }

  if (activation == NEURON_ACTIVATION_RELU) {
    for (int32_t i = 0; i < t->size; ++i) {
      if (t->data[i] < 0.0f) {
        t->data[i] = 0.0f;
      }
    }
    return;
  }

  if (activation == NEURON_ACTIVATION_SIGMOID) {
    for (i = 0; i < t->size; ++i) {
      t->data[i] = 1.0f / (1.0f + expf(-t->data[i]));
    }
    return;
  }

  if (activation == NEURON_ACTIVATION_TANH) {
    for (i = 0; i < t->size; ++i) {
      t->data[i] = tanhf(t->data[i]);
    }
    return;
  }

  if (activation == NEURON_ACTIVATION_LEAKY_RELU) {
    const float alpha =
        activation_param_or_default(activation, activation_param);
    for (i = 0; i < t->size; ++i) {
      if (t->data[i] < 0.0f) {
        t->data[i] *= alpha;
      }
    }
    return;
  }

  if (activation == NEURON_ACTIVATION_SOFTMAX) {
    int32_t rows = 1;
    int32_t cols = t->size;
    if (t->dimensions == 2) {
      rows = t->shape[0];
      cols = t->shape[1];
    }
    if (rows <= 0 || cols <= 0) {
      return;
    }

    for (int32_t r = 0; r < rows; ++r) {
      float row_max = t->data[r * cols];
      float sum_exp = 0.0f;
      for (int32_t c = 1; c < cols; ++c) {
        const float value = t->data[r * cols + c];
        if (value > row_max) {
          row_max = value;
        }
      }
      for (int32_t c = 0; c < cols; ++c) {
        float exp_value = expf(t->data[r * cols + c] - row_max);
        t->data[r * cols + c] = exp_value;
        sum_exp += exp_value;
      }
      if (sum_exp <= 0.0f) {
        const float uniform = 1.0f / (float)cols;
        for (int32_t c = 0; c < cols; ++c) {
          t->data[r * cols + c] = uniform;
        }
      } else {
        const float inv_sum = 1.0f / sum_exp;
        for (int32_t c = 0; c < cols; ++c) {
          t->data[r * cols + c] *= inv_sum;
        }
      }
    }
  }
}

static int32_t tensor_activation_from_nn(NeuronActivationType activation) {
  if (activation == NEURON_ACTIVATION_RELU) {
    return NEURON_TENSOR_ACTIVATION_RELU;
  }
  return NEURON_TENSOR_ACTIVATION_NONE;
}

static int apply_activation_backward_inplace(NeuronTensor *grad,
                                             const NeuronTensor *linear_cache,
                                             NeuronActivationType activation,
                                             float activation_param) {
  int32_t i = 0;
  if (!tensor_is_valid(grad) || !tensor_is_valid(linear_cache) ||
      !tensor_has_same_shape(grad, linear_cache)) {
    return 0;
  }

  if (activation == NEURON_ACTIVATION_LINEAR) {
    return 1;
  }

  if (activation == NEURON_ACTIVATION_RELU) {
    for (i = 0; i < grad->size; ++i) {
      if (linear_cache->data[i] <= 0.0f) {
        grad->data[i] = 0.0f;
      }
    }
    return 1;
  }

  if (activation == NEURON_ACTIVATION_SIGMOID) {
    for (i = 0; i < grad->size; ++i) {
      const float sigmoid = 1.0f / (1.0f + expf(-linear_cache->data[i]));
      grad->data[i] *= sigmoid * (1.0f - sigmoid);
    }
    return 1;
  }

  if (activation == NEURON_ACTIVATION_TANH) {
    for (i = 0; i < grad->size; ++i) {
      const float tanh_value = tanhf(linear_cache->data[i]);
      grad->data[i] *= 1.0f - tanh_value * tanh_value;
    }
    return 1;
  }

  if (activation == NEURON_ACTIVATION_LEAKY_RELU) {
    const float alpha =
        activation_param_or_default(activation, activation_param);
    for (i = 0; i < grad->size; ++i) {
      if (linear_cache->data[i] <= 0.0f) {
        grad->data[i] *= alpha;
      }
    }
    return 1;
  }

  if (activation == NEURON_ACTIVATION_SOFTMAX) {
    NeuronTensor *probs = NULL;
    if (!tensor_is_matrix(grad) || !tensor_is_matrix(linear_cache)) {
      return 0;
    }

    probs = tensor_clone(linear_cache);
    if (probs == NULL) {
      return 0;
    }
    apply_activation_inplace(probs, activation, activation_param);

    for (int32_t r = 0; r < grad->shape[0]; ++r) {
      float dot = 0.0f;
      for (int32_t c = 0; c < grad->shape[1]; ++c) {
        const int32_t index = r * grad->shape[1] + c;
        dot += grad->data[index] * probs->data[index];
      }
      for (int32_t c = 0; c < grad->shape[1]; ++c) {
        const int32_t index = r * grad->shape[1] + c;
        grad->data[index] = probs->data[index] * (grad->data[index] - dot);
      }
    }

    neuron_tensor_free(probs);
    return 1;
  }

  return 0;
}

static NeuronInitStrategy activation_default_init_strategy(
    NeuronActivationType activation) {
  if (activation == NEURON_ACTIVATION_RELU ||
      activation == NEURON_ACTIVATION_LEAKY_RELU) {
    return NEURON_INIT_KAIMING_UNIFORM;
  }
  return NEURON_INIT_XAVIER_UNIFORM;
}

static float init_limit_from_strategy(int32_t fan_in, int32_t fan_out,
                                      NeuronActivationType activation,
                                      float activation_param,
                                      NeuronInitStrategy strategy) {
  const float fan_in_f = (float)fan_in;
  const float fan_out_f = (float)fan_out;
  NeuronInitStrategy effective = strategy;
  if (effective == NEURON_INIT_DEFAULT) {
    effective = activation_default_init_strategy(activation);
  }
  if (fan_in_f <= 0.0f) {
    return 0.05f;
  }
  if (effective == NEURON_INIT_KAIMING_UNIFORM) {
    if (activation == NEURON_ACTIVATION_LEAKY_RELU) {
      const float alpha =
          activation_param_or_default(activation, activation_param);
      return sqrtf(6.0f / ((1.0f + alpha * alpha) * fan_in_f));
    }
    return sqrtf(6.0f / fan_in_f);
  }
  return sqrtf(6.0f / (fan_in_f + fan_out_f));
}

static float dense_init_limit(int32_t input_size, int32_t output_size,
                              NeuronActivationType activation,
                              float activation_param,
                              NeuronInitStrategy strategy) {
  const float fan_in = (float)input_size;
  if (fan_in <= 0.0f) {
    return 0.05f;
  }
  return init_limit_from_strategy(input_size, output_size, activation,
                                  activation_param, strategy);
}

static int activation_is_supported(int32_t activation) {
  return activation >= (int32_t)NEURON_ACTIVATION_LINEAR &&
         activation <= (int32_t)NEURON_ACTIVATION_SOFTMAX;
}

static int init_strategy_is_supported(int32_t strategy) {
  return strategy >= (int32_t)NEURON_INIT_DEFAULT &&
         strategy <= (int32_t)NEURON_INIT_KAIMING_UNIFORM;
}

static int write_bytes(FILE *file, const void *data, size_t size) {
  return file != NULL &&
         (size == 0 || (data != NULL && fwrite(data, 1, size, file) == size));
}

static int read_bytes(FILE *file, void *data, size_t size) {
  return file != NULL &&
         (size == 0 || (data != NULL && fread(data, 1, size, file) == size));
}

static void free_tensor_slots(NeuronTensor **slots, int32_t count) {
  int32_t i = 0;
  if (slots == NULL) {
    return;
  }
  for (i = 0; i < count; ++i) {
    neuron_tensor_free(slots[i]);
  }
  neuron_dealloc(slots);
}

static int ensure_adam_slot(NeuronTensor **slot, const NeuronTensor *reference) {
  if (slot == NULL) {
    return 0;
  }
  if (!tensor_is_valid(reference)) {
    neuron_tensor_free(*slot);
    *slot = NULL;
    return 1;
  }
  if (*slot != NULL && !tensor_has_same_shape(*slot, reference)) {
    neuron_tensor_free(*slot);
    *slot = NULL;
  }
  if (*slot == NULL) {
    *slot = tensor_create_like(reference);
    if (*slot == NULL) {
      return 0;
    }
    tensor_zero(*slot);
  }
  return 1;
}

static int ensure_adam_state(NeuronAdamOptimizer *opt,
                             const NeuronSequentialModel *model) {
  if (opt == NULL || model == NULL) {
    return 0;
  }

  if (opt->layer_capacity != model->count || opt->model != model) {
    free_tensor_slots(opt->weight_moments, opt->layer_capacity);
    free_tensor_slots(opt->weight_velocities, opt->layer_capacity);
    free_tensor_slots(opt->bias_moments, opt->layer_capacity);
    free_tensor_slots(opt->bias_velocities, opt->layer_capacity);
    opt->weight_moments = NULL;
    opt->weight_velocities = NULL;
    opt->bias_moments = NULL;
    opt->bias_velocities = NULL;
    opt->layer_capacity = 0;

    if (model->count > 0) {
      const size_t slot_bytes =
          sizeof(NeuronTensor *) * (size_t)model->count;
      opt->weight_moments = (NeuronTensor **)neuron_alloc(slot_bytes);
      if (opt->weight_moments != NULL) {
        memset(opt->weight_moments, 0, slot_bytes);
      }
      opt->weight_velocities = (NeuronTensor **)neuron_alloc(slot_bytes);
      if (opt->weight_velocities != NULL) {
        memset(opt->weight_velocities, 0, slot_bytes);
      }
      opt->bias_moments = (NeuronTensor **)neuron_alloc(slot_bytes);
      if (opt->bias_moments != NULL) {
        memset(opt->bias_moments, 0, slot_bytes);
      }
      opt->bias_velocities = (NeuronTensor **)neuron_alloc(slot_bytes);
      if (opt->bias_velocities != NULL) {
        memset(opt->bias_velocities, 0, slot_bytes);
      }
      if (opt->weight_moments == NULL || opt->weight_velocities == NULL ||
          opt->bias_moments == NULL || opt->bias_velocities == NULL) {
        free_tensor_slots(opt->weight_moments, model->count);
        free_tensor_slots(opt->weight_velocities, model->count);
        free_tensor_slots(opt->bias_moments, model->count);
        free_tensor_slots(opt->bias_velocities, model->count);
        opt->weight_moments = NULL;
        opt->weight_velocities = NULL;
        opt->bias_moments = NULL;
        opt->bias_velocities = NULL;
        return 0;
      }
    }

    opt->layer_capacity = model->count;
  }

  opt->model = model;
  for (int32_t i = 0; i < model->count; ++i) {
    NeuronTensor *weights = NULL;
    NeuronTensor *grad_weights = NULL;
    NeuronTensor *bias = NULL;
    NeuronTensor *grad_bias = NULL;
    model_layer_param_tensors(model->layer_kinds[i], model->layer_objects[i],
                              &weights, &grad_weights, &bias, &grad_bias);
    (void)grad_weights;
    (void)grad_bias;
    if (!ensure_adam_slot(&opt->weight_moments[i], weights) ||
        !ensure_adam_slot(&opt->weight_velocities[i], weights) ||
        !ensure_adam_slot(&opt->bias_moments[i], bias) ||
        !ensure_adam_slot(&opt->bias_velocities[i], bias)) {
      return 0;
    }
  }

  return 1;
}

static void layer_clear_cache(NeuronDenseLayer *layer) {
  if (layer == NULL) {
    return;
  }
  if (layer->cache_input != NULL) {
    neuron_tensor_free(layer->cache_input);
    layer->cache_input = NULL;
  }
  if (layer->cache_linear != NULL) {
    neuron_tensor_free(layer->cache_linear);
    layer->cache_linear = NULL;
  }
}

static void layer_set_grad(NeuronTensor **slot, NeuronTensor *grad) {
  if (slot == NULL) {
    if (grad != NULL) {
      neuron_tensor_free(grad);
    }
    return;
  }
  if (*slot != NULL) {
    neuron_tensor_free(*slot);
  }
  *slot = grad;
}

static uint32_t rng_next(uint32_t *state) {
  uint32_t x = (*state == 0U) ? 0x6d2b79f5U : *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

NeuronDenseLayer *neuron_layer_dense_create(int32_t input_size,
                                            int32_t output_size,
                                            NeuronActivationType activation) {
  if (input_size <= 0 || output_size <= 0) {
    return NULL;
  }

  NeuronDenseLayer *layer = (NeuronDenseLayer *)neuron_alloc(sizeof(*layer));
  if (layer == NULL) {
    return NULL;
  }
  memset(layer, 0, sizeof(*layer));

  layer->input_size = input_size;
  layer->output_size = output_size;
  layer->activation = activation;
  layer->activation_param = activation_param_or_default(activation, 0.0f);
  layer->init_strategy = NEURON_INIT_DEFAULT;

  int32_t w_shape[2] = {input_size, output_size};
  int32_t b_shape[2] = {1, output_size};
  layer->weights = neuron_tensor_create(2, w_shape);
  layer->bias = neuron_tensor_create(2, b_shape);

  if (layer->weights == NULL || layer->bias == NULL) {
    neuron_layer_dense_free(layer);
    return NULL;
  }

  float limit = dense_init_limit(input_size, output_size, activation,
                                 layer->activation_param,
                                 layer->init_strategy);
  for (int32_t i = 0; i < layer->weights->size; ++i) {
    float r = (float)neuron_random_float();
    layer->weights->data[i] = (2.0f * r - 1.0f) * limit;
  }
  tensor_zero(layer->bias);

  return layer;
}

void neuron_layer_dense_apply_init(NeuronDenseLayer *layer,
                                   NeuronInitStrategy strategy) {
  if (layer == NULL || !tensor_is_matrix(layer->weights)) {
    return;
  }
  layer->init_strategy = strategy;
  {
    const float limit =
        dense_init_limit(layer->input_size, layer->output_size,
                         layer->activation, layer->activation_param, strategy);
    for (int32_t i = 0; i < layer->weights->size; ++i) {
      const float r = (float)neuron_random_float();
      layer->weights->data[i] = (2.0f * r - 1.0f) * limit;
    }
  }
  if (layer->bias != NULL) {
    tensor_zero(layer->bias);
  }
  neuron_tensor_packed_free(layer->packed_weights);
  layer->packed_weights = NULL;
  layer->packed_weights_version++;
}

void neuron_layer_dense_free(NeuronDenseLayer *layer) {
  if (layer == NULL) {
    return;
  }
  neuron_tensor_packed_free(layer->packed_weights);
  neuron_tensor_free(layer->weights);
  neuron_tensor_free(layer->bias);
  neuron_tensor_free(layer->grad_weights);
  neuron_tensor_free(layer->grad_bias);
  neuron_tensor_free(layer->cache_input);
  neuron_tensor_free(layer->cache_linear);
  neuron_dealloc(layer);
}

NeuronTensor *neuron_layer_dense_forward(NeuronDenseLayer *layer,
                                         NeuronTensor *input, int training) {
  if (layer == NULL || !tensor_is_matrix(input)) {
    return NULL;
  }
  if (input->shape[1] != layer->input_size) {
    return NULL;
  }

  if (training) {
    layer_clear_cache(layer);
  }

  if (!training && layer->packed_weights == NULL && layer->weights != NULL) {
    (void)neuron_tensor_pack_b(layer->weights, &layer->packed_weights);
  }

  const int32_t fusedActivation =
      (!training && layer->activation == NEURON_ACTIVATION_RELU)
          ? tensor_activation_from_nn(layer->activation)
          : NEURON_TENSOR_ACTIVATION_NONE;
  const NeuronPackedMatrix *packedWeights =
      training ? NULL : layer->packed_weights;

  NeuronTensor *linear = neuron_tensor_linear_fused_ex_hint(
      input, layer->weights, packedWeights, layer->bias, NULL, fusedActivation,
      NULL, 0, nn_exec_hint());
  if (linear == NULL) {
    return NULL;
  }

  if (training) {
    layer->cache_input = tensor_clone(input);
    layer->cache_linear = tensor_clone(linear);
    if (layer->cache_input == NULL || layer->cache_linear == NULL) {
      neuron_tensor_free(linear);
      layer_clear_cache(layer);
      return NULL;
    }
    apply_activation_inplace(linear, layer->activation, layer->activation_param);
  } else if (layer->activation != NEURON_ACTIVATION_LINEAR &&
             fusedActivation == NEURON_TENSOR_ACTIVATION_NONE) {
    apply_activation_inplace(linear, layer->activation, layer->activation_param);
  }

  return linear;
}

NeuronTensor *neuron_layer_dense_backward(NeuronDenseLayer *layer,
                                          NeuronTensor *grad_output) {
  if (layer == NULL || !tensor_is_matrix(grad_output)) {
    return NULL;
  }
  if (layer->cache_input == NULL || layer->cache_linear == NULL) {
    return NULL;
  }
  if (grad_output->shape[1] != layer->output_size ||
      grad_output->shape[0] != layer->cache_input->shape[0]) {
    return NULL;
  }

  int32_t batch = layer->cache_input->shape[0];
  if (batch <= 0) {
    return NULL;
  }

  NeuronTensor *grad_z = tensor_clone(grad_output);
  NeuronTensor *x_t = NULL;
  NeuronTensor *grad_w = NULL;
  NeuronTensor *grad_b = NULL;
  NeuronTensor *w_t = NULL;
  NeuronTensor *grad_input = NULL;

  if (grad_z == NULL) {
    return NULL;
  }

  if (!apply_activation_backward_inplace(grad_z, layer->cache_linear,
                                         layer->activation,
                                         layer->activation_param)) {
    goto cleanup;
  }

  x_t = tensor_transpose_2d(layer->cache_input);
  if (x_t == NULL) {
    goto cleanup;
  }
  grad_w = neuron_tensor_matmul_ex_hint(x_t, grad_z, NULL, 0, nn_exec_hint());
  if (grad_w == NULL) {
    goto cleanup;
  }
  tensor_scale_inplace(grad_w, 1.0f / (float)batch);

  grad_b = tensor_sum_rows(grad_z);
  if (grad_b == NULL) {
    goto cleanup;
  }
  tensor_scale_inplace(grad_b, 1.0f / (float)batch);

  w_t = tensor_transpose_2d(layer->weights);
  if (w_t == NULL) {
    goto cleanup;
  }
  grad_input =
      neuron_tensor_matmul_ex_hint(grad_z, w_t, NULL, 0, nn_exec_hint());
  if (grad_input == NULL) {
    goto cleanup;
  }

  layer_set_grad(&layer->grad_weights, grad_w);
  layer_set_grad(&layer->grad_bias, grad_b);
  grad_w = NULL;
  grad_b = NULL;

cleanup:
  neuron_tensor_free(grad_z);
  neuron_tensor_free(x_t);
  neuron_tensor_free(grad_w);
  neuron_tensor_free(grad_b);
  neuron_tensor_free(w_t);
  return grad_input;
}

static void conv_layer_clear_cache(NeuronConv2DLayer *layer) {
  if (layer == NULL) {
    return;
  }
  neuron_tensor_free(layer->cache_input);
  neuron_tensor_free(layer->cache_linear);
  neuron_tensor_free(layer->cache_columns);
  layer->cache_input = NULL;
  layer->cache_linear = NULL;
  layer->cache_columns = NULL;
}

static int32_t conv_output_extent(int32_t input_size, int32_t kernel_size,
                                  int32_t stride, int32_t padding) {
  const int32_t padded = input_size + padding * 2;
  if (input_size <= 0 || kernel_size <= 0 || stride <= 0 || padding < 0 ||
      padded < kernel_size) {
    return -1;
  }
  return 1 + (padded - kernel_size) / stride;
}

NeuronConv2DLayer *neuron_layer_conv2d_create(int32_t input_channels,
                                              int32_t output_channels,
                                              int32_t kernel_h,
                                              int32_t kernel_w,
                                              int32_t stride_h,
                                              int32_t stride_w,
                                              int32_t padding_h,
                                              int32_t padding_w,
                                              NeuronActivationType activation) {
  int32_t w_shape[4];
  int32_t b_shape[2];
  NeuronConv2DLayer *layer = NULL;
  if (input_channels <= 0 || output_channels <= 0 || kernel_h <= 0 ||
      kernel_w <= 0 || stride_h <= 0 || stride_w <= 0 || padding_h < 0 ||
      padding_w < 0) {
    return NULL;
  }

  layer = (NeuronConv2DLayer *)neuron_alloc(sizeof(*layer));
  if (layer == NULL) {
    return NULL;
  }
  memset(layer, 0, sizeof(*layer));

  layer->input_channels = input_channels;
  layer->output_channels = output_channels;
  layer->kernel_h = kernel_h;
  layer->kernel_w = kernel_w;
  layer->stride_h = stride_h;
  layer->stride_w = stride_w;
  layer->padding_h = padding_h;
  layer->padding_w = padding_w;
  layer->activation = activation;
  layer->activation_param = activation_param_or_default(activation, 0.0f);
  layer->init_strategy = NEURON_INIT_DEFAULT;

  w_shape[0] = output_channels;
  w_shape[1] = input_channels;
  w_shape[2] = kernel_h;
  w_shape[3] = kernel_w;
  b_shape[0] = 1;
  b_shape[1] = output_channels;
  layer->weights = neuron_tensor_create(4, w_shape);
  layer->bias = neuron_tensor_create(2, b_shape);
  if (layer->weights == NULL || layer->bias == NULL) {
    neuron_layer_conv2d_free(layer);
    return NULL;
  }

  neuron_layer_conv2d_apply_init(layer, NEURON_INIT_DEFAULT);
  return layer;
}

void neuron_layer_conv2d_free(NeuronConv2DLayer *layer) {
  if (layer == NULL) {
    return;
  }
  neuron_tensor_free(layer->weights);
  neuron_tensor_free(layer->bias);
  neuron_tensor_free(layer->grad_weights);
  neuron_tensor_free(layer->grad_bias);
  conv_layer_clear_cache(layer);
  neuron_dealloc(layer);
}

void neuron_layer_conv2d_apply_init(NeuronConv2DLayer *layer,
                                    NeuronInitStrategy strategy) {
  const int32_t fan_in =
      layer == NULL ? 0 : layer->input_channels * layer->kernel_h * layer->kernel_w;
  const int32_t fan_out =
      layer == NULL ? 0
                    : layer->output_channels * layer->kernel_h * layer->kernel_w;
  if (layer == NULL || !tensor_is_4d_nchw(layer->weights)) {
    return;
  }
  layer->init_strategy = strategy;
  {
    const float limit = init_limit_from_strategy(
        fan_in, fan_out, layer->activation, layer->activation_param, strategy);
    for (int32_t i = 0; i < layer->weights->size; ++i) {
      const float r = (float)neuron_random_float();
      layer->weights->data[i] = (2.0f * r - 1.0f) * limit;
    }
  }
  if (layer->bias != NULL) {
    tensor_zero(layer->bias);
  }
}

NeuronTensor *neuron_layer_conv2d_forward(NeuronConv2DLayer *layer,
                                          NeuronTensor *input, int training) {
  int32_t out_h = 0;
  int32_t out_w = 0;
  NeuronTensor *linear = NULL;
  if (layer == NULL || !tensor_is_4d_nchw(input) ||
      input->shape[1] != layer->input_channels) {
    return NULL;
  }

  out_h = conv_output_extent(input->shape[2], layer->kernel_h, layer->stride_h,
                             layer->padding_h);
  out_w = conv_output_extent(input->shape[3], layer->kernel_w, layer->stride_w,
                             layer->padding_w);
  if (out_h <= 0 || out_w <= 0) {
    return NULL;
  }

  if (training) {
    conv_layer_clear_cache(layer);
  }

  linear = tensor_create_4d(input->shape[0], layer->output_channels, out_h, out_w);
  if (linear == NULL) {
    return NULL;
  }

  for (int32_t n = 0; n < input->shape[0]; ++n) {
    for (int32_t oc = 0; oc < layer->output_channels; ++oc) {
      for (int32_t oh = 0; oh < out_h; ++oh) {
        for (int32_t ow = 0; ow < out_w; ++ow) {
          float sum = layer->bias != NULL ? layer->bias->data[oc] : 0.0f;
          for (int32_t ic = 0; ic < layer->input_channels; ++ic) {
            for (int32_t kh = 0; kh < layer->kernel_h; ++kh) {
              for (int32_t kw = 0; kw < layer->kernel_w; ++kw) {
                const int32_t ih = oh * layer->stride_h + kh - layer->padding_h;
                const int32_t iw = ow * layer->stride_w + kw - layer->padding_w;
                if (ih < 0 || iw < 0 || ih >= input->shape[2] ||
                    iw >= input->shape[3]) {
                  continue;
                }
                sum += input->data[tensor_offset_nchw(input, n, ic, ih, iw)] *
                       layer->weights
                           ->data[tensor_offset_nchw(layer->weights, oc, ic, kh, kw)];
              }
            }
          }
          linear->data[tensor_offset_nchw(linear, n, oc, oh, ow)] = sum;
        }
      }
    }
  }

  if (training) {
    layer->cache_input = tensor_clone(input);
    layer->cache_linear = tensor_clone(linear);
    if (layer->cache_input == NULL || layer->cache_linear == NULL) {
      neuron_tensor_free(linear);
      conv_layer_clear_cache(layer);
      return NULL;
    }
  }
  apply_activation_inplace(linear, layer->activation, layer->activation_param);
  return linear;
}

NeuronTensor *neuron_layer_conv2d_backward(NeuronConv2DLayer *layer,
                                           NeuronTensor *grad_output) {
  int32_t batch = 0;
  NeuronTensor *grad_z = NULL;
  NeuronTensor *grad_input = NULL;
  NeuronTensor *grad_w = NULL;
  NeuronTensor *grad_b = NULL;
  if (layer == NULL || !tensor_is_4d_nchw(grad_output) ||
      !tensor_is_4d_nchw(layer->cache_input) ||
      !tensor_is_4d_nchw(layer->cache_linear)) {
    return NULL;
  }
  if (!tensor_has_same_shape(grad_output, layer->cache_linear)) {
    return NULL;
  }

  batch = layer->cache_input->shape[0];
  if (batch <= 0) {
    return NULL;
  }

  grad_z = tensor_clone(grad_output);
  grad_input = tensor_create_like(layer->cache_input);
  grad_w = tensor_create_like(layer->weights);
  grad_b = tensor_create_like(layer->bias);
  if (grad_z == NULL || grad_input == NULL || grad_w == NULL || grad_b == NULL) {
    neuron_tensor_free(grad_z);
    neuron_tensor_free(grad_input);
    neuron_tensor_free(grad_w);
    neuron_tensor_free(grad_b);
    return NULL;
  }
  tensor_zero(grad_input);
  tensor_zero(grad_w);
  tensor_zero(grad_b);

  if (!apply_activation_backward_inplace(grad_z, layer->cache_linear,
                                         layer->activation,
                                         layer->activation_param)) {
    neuron_tensor_free(grad_z);
    neuron_tensor_free(grad_input);
    neuron_tensor_free(grad_w);
    neuron_tensor_free(grad_b);
    return NULL;
  }

  for (int32_t n = 0; n < grad_z->shape[0]; ++n) {
    for (int32_t oc = 0; oc < grad_z->shape[1]; ++oc) {
      for (int32_t oh = 0; oh < grad_z->shape[2]; ++oh) {
        for (int32_t ow = 0; ow < grad_z->shape[3]; ++ow) {
          const float grad =
              grad_z->data[tensor_offset_nchw(grad_z, n, oc, oh, ow)];
          grad_b->data[oc] += grad;
          for (int32_t ic = 0; ic < layer->input_channels; ++ic) {
            for (int32_t kh = 0; kh < layer->kernel_h; ++kh) {
              for (int32_t kw = 0; kw < layer->kernel_w; ++kw) {
                const int32_t ih = oh * layer->stride_h + kh - layer->padding_h;
                const int32_t iw = ow * layer->stride_w + kw - layer->padding_w;
                const int32_t weight_index =
                    tensor_offset_nchw(layer->weights, oc, ic, kh, kw);
                if (ih < 0 || iw < 0 || ih >= layer->cache_input->shape[2] ||
                    iw >= layer->cache_input->shape[3]) {
                  continue;
                }
                grad_w->data[weight_index] +=
                    layer->cache_input
                        ->data[tensor_offset_nchw(layer->cache_input, n, ic, ih, iw)] *
                    grad;
                grad_input
                    ->data[tensor_offset_nchw(grad_input, n, ic, ih, iw)] +=
                    layer->weights->data[weight_index] * grad;
              }
            }
          }
        }
      }
    }
  }

  tensor_scale_inplace(grad_w, 1.0f / (float)batch);
  tensor_scale_inplace(grad_b, 1.0f / (float)batch);
  layer_set_grad(&layer->grad_weights, grad_w);
  layer_set_grad(&layer->grad_bias, grad_b);
  grad_w = NULL;
  grad_b = NULL;

  neuron_tensor_free(grad_z);
  neuron_tensor_free(grad_w);
  neuron_tensor_free(grad_b);
  return grad_input;
}

static void maxpool_layer_clear_cache(NeuronMaxPool2DLayer *layer) {
  if (layer == NULL) {
    return;
  }
  neuron_dealloc(layer->cache_indices);
  layer->cache_indices = NULL;
  layer->cache_index_count = 0;
  memset(layer->cache_input_shape, 0, sizeof(layer->cache_input_shape));
}

NeuronMaxPool2DLayer *neuron_layer_maxpool2d_create(int32_t kernel_h,
                                                    int32_t kernel_w,
                                                    int32_t stride_h,
                                                    int32_t stride_w) {
  NeuronMaxPool2DLayer *layer = NULL;
  if (kernel_h <= 0 || kernel_w <= 0 || stride_h <= 0 || stride_w <= 0) {
    return NULL;
  }
  layer = (NeuronMaxPool2DLayer *)neuron_alloc(sizeof(*layer));
  if (layer == NULL) {
    return NULL;
  }
  memset(layer, 0, sizeof(*layer));
  layer->kernel_h = kernel_h;
  layer->kernel_w = kernel_w;
  layer->stride_h = stride_h;
  layer->stride_w = stride_w;
  return layer;
}

void neuron_layer_maxpool2d_free(NeuronMaxPool2DLayer *layer) {
  if (layer == NULL) {
    return;
  }
  maxpool_layer_clear_cache(layer);
  neuron_dealloc(layer);
}

NeuronTensor *neuron_layer_maxpool2d_forward(NeuronMaxPool2DLayer *layer,
                                             NeuronTensor *input,
                                             int training) {
  int32_t out_h = 0;
  int32_t out_w = 0;
  NeuronTensor *output = NULL;
  if (layer == NULL || !tensor_is_4d_nchw(input)) {
    return NULL;
  }
  out_h = conv_output_extent(input->shape[2], layer->kernel_h, layer->stride_h, 0);
  out_w = conv_output_extent(input->shape[3], layer->kernel_w, layer->stride_w, 0);
  if (out_h <= 0 || out_w <= 0) {
    return NULL;
  }

  if (training) {
    maxpool_layer_clear_cache(layer);
  }
  output = tensor_create_4d(input->shape[0], input->shape[1], out_h, out_w);
  if (output == NULL) {
    return NULL;
  }

  if (training) {
    layer->cache_index_count = output->size;
    layer->cache_indices =
        (int32_t *)neuron_alloc(sizeof(int32_t) * (size_t)output->size);
    if (layer->cache_indices == NULL) {
      neuron_tensor_free(output);
      return NULL;
    }
    memcpy(layer->cache_input_shape, input->shape, sizeof(layer->cache_input_shape));
  }

  for (int32_t n = 0; n < input->shape[0]; ++n) {
    for (int32_t c = 0; c < input->shape[1]; ++c) {
      for (int32_t oh = 0; oh < out_h; ++oh) {
        for (int32_t ow = 0; ow < out_w; ++ow) {
          float max_value = 0.0f;
          int32_t max_index = -1;
          for (int32_t kh = 0; kh < layer->kernel_h; ++kh) {
            for (int32_t kw = 0; kw < layer->kernel_w; ++kw) {
              const int32_t ih = oh * layer->stride_h + kh;
              const int32_t iw = ow * layer->stride_w + kw;
              const int32_t input_index =
                  tensor_offset_nchw(input, n, c, ih, iw);
              if (max_index < 0 || input->data[input_index] > max_value) {
                max_value = input->data[input_index];
                max_index = input_index;
              }
            }
          }
          output->data[tensor_offset_nchw(output, n, c, oh, ow)] = max_value;
          if (training) {
            layer->cache_indices[tensor_offset_nchw(output, n, c, oh, ow)] =
                max_index;
          }
        }
      }
    }
  }

  return output;
}

NeuronTensor *neuron_layer_maxpool2d_backward(NeuronMaxPool2DLayer *layer,
                                              NeuronTensor *grad_output) {
  NeuronTensor *grad_input = NULL;
  if (layer == NULL || !tensor_is_4d_nchw(grad_output) ||
      layer->cache_indices == NULL || layer->cache_index_count != grad_output->size) {
    return NULL;
  }
  grad_input = tensor_create_4d(layer->cache_input_shape[0],
                                layer->cache_input_shape[1],
                                layer->cache_input_shape[2],
                                layer->cache_input_shape[3]);
  if (grad_input == NULL) {
    return NULL;
  }
  tensor_zero(grad_input);
  for (int32_t i = 0; i < grad_output->size; ++i) {
    const int32_t input_index = layer->cache_indices[i];
    if (input_index >= 0 && input_index < grad_input->size) {
      grad_input->data[input_index] += grad_output->data[i];
    }
  }
  return grad_input;
}

NeuronDropoutLayer *neuron_layer_dropout_create(float drop_probability,
                                                uint32_t seed) {
  NeuronDropoutLayer *layer = NULL;
  if (drop_probability < 0.0f || drop_probability >= 1.0f) {
    return NULL;
  }
  layer = (NeuronDropoutLayer *)neuron_alloc(sizeof(*layer));
  if (layer == NULL) {
    return NULL;
  }
  memset(layer, 0, sizeof(*layer));
  layer->drop_probability = drop_probability;
  layer->scale = drop_probability < 1.0f ? 1.0f / (1.0f - drop_probability) : 0.0f;
  layer->seed = seed == 0U ? 0x13579bdfU : seed;
  layer->rng_state = layer->seed;
  return layer;
}

void neuron_layer_dropout_free(NeuronDropoutLayer *layer) {
  if (layer == NULL) {
    return;
  }
  neuron_tensor_free(layer->mask);
  neuron_dealloc(layer);
}

NeuronTensor *neuron_layer_dropout_forward(NeuronDropoutLayer *layer,
                                           NeuronTensor *input, int training) {
  NeuronTensor *output = NULL;
  if (layer == NULL || !tensor_is_valid(input)) {
    return NULL;
  }

  if (!training || layer->drop_probability <= 0.0f) {
    return tensor_clone(input);
  }

  neuron_tensor_free(layer->mask);
  layer->mask = tensor_create_like(input);
  output = tensor_create_like(input);
  if (layer->mask == NULL || output == NULL) {
    neuron_tensor_free(output);
    neuron_tensor_free(layer->mask);
    layer->mask = NULL;
    return NULL;
  }

  for (int32_t i = 0; i < input->size; ++i) {
    const float keep =
        ((rng_next(&layer->rng_state) & 0xffffU) / 65535.0f) >=
                layer->drop_probability
            ? layer->scale
            : 0.0f;
    layer->mask->data[i] = keep;
    output->data[i] = input->data[i] * keep;
  }
  return output;
}

NeuronTensor *neuron_layer_dropout_backward(NeuronDropoutLayer *layer,
                                            NeuronTensor *grad_output) {
  NeuronTensor *grad_input = NULL;
  if (layer == NULL || !tensor_is_valid(grad_output)) {
    return NULL;
  }
  if (layer->mask == NULL || !tensor_has_same_shape(layer->mask, grad_output)) {
    return tensor_clone(grad_output);
  }
  grad_input = tensor_clone(grad_output);
  if (grad_input == NULL) {
    return NULL;
  }
  for (int32_t i = 0; i < grad_input->size; ++i) {
    grad_input->data[i] *= layer->mask->data[i];
  }
  return grad_input;
}

static void layer_object_free(int32_t layer_kind, void *layer_object) {
  if (layer_object == NULL) {
    return;
  }
  if (layer_kind == NEURON_LAYER_DENSE) {
    neuron_layer_dense_free((NeuronDenseLayer *)layer_object);
  } else if (layer_kind == NEURON_LAYER_CONV2D) {
    neuron_layer_conv2d_free((NeuronConv2DLayer *)layer_object);
  } else if (layer_kind == NEURON_LAYER_MAXPOOL2D) {
    neuron_layer_maxpool2d_free((NeuronMaxPool2DLayer *)layer_object);
  } else if (layer_kind == NEURON_LAYER_DROPOUT) {
    neuron_layer_dropout_free((NeuronDropoutLayer *)layer_object);
  }
}

static NeuronTensor *model_layer_forward(int32_t layer_kind, void *layer_object,
                                         NeuronTensor *input, int training) {
  if (layer_kind == NEURON_LAYER_DENSE) {
    return neuron_layer_dense_forward((NeuronDenseLayer *)layer_object, input,
                                      training);
  }
  if (layer_kind == NEURON_LAYER_CONV2D) {
    return neuron_layer_conv2d_forward((NeuronConv2DLayer *)layer_object, input,
                                       training);
  }
  if (layer_kind == NEURON_LAYER_MAXPOOL2D) {
    return neuron_layer_maxpool2d_forward((NeuronMaxPool2DLayer *)layer_object,
                                          input, training);
  }
  if (layer_kind == NEURON_LAYER_DROPOUT) {
    return neuron_layer_dropout_forward((NeuronDropoutLayer *)layer_object,
                                        input, training);
  }
  return NULL;
}

static NeuronTensor *model_layer_backward(int32_t layer_kind, void *layer_object,
                                          NeuronTensor *grad_output) {
  if (layer_kind == NEURON_LAYER_DENSE) {
    return neuron_layer_dense_backward((NeuronDenseLayer *)layer_object,
                                       grad_output);
  }
  if (layer_kind == NEURON_LAYER_CONV2D) {
    return neuron_layer_conv2d_backward((NeuronConv2DLayer *)layer_object,
                                        grad_output);
  }
  if (layer_kind == NEURON_LAYER_MAXPOOL2D) {
    return neuron_layer_maxpool2d_backward((NeuronMaxPool2DLayer *)layer_object,
                                           grad_output);
  }
  if (layer_kind == NEURON_LAYER_DROPOUT) {
    return neuron_layer_dropout_backward((NeuronDropoutLayer *)layer_object,
                                         grad_output);
  }
  return NULL;
}

static void model_layer_zero_grad(int32_t layer_kind, void *layer_object) {
  if (layer_object == NULL) {
    return;
  }
  if (layer_kind == NEURON_LAYER_DENSE) {
    NeuronDenseLayer *layer = (NeuronDenseLayer *)layer_object;
    if (layer->grad_weights != NULL) {
      tensor_zero(layer->grad_weights);
    }
    if (layer->grad_bias != NULL) {
      tensor_zero(layer->grad_bias);
    }
  } else if (layer_kind == NEURON_LAYER_CONV2D) {
    NeuronConv2DLayer *layer = (NeuronConv2DLayer *)layer_object;
    if (layer->grad_weights != NULL) {
      tensor_zero(layer->grad_weights);
    }
    if (layer->grad_bias != NULL) {
      tensor_zero(layer->grad_bias);
    }
  }
}

static void model_layer_param_tensors(int32_t layer_kind, void *layer_object,
                                      NeuronTensor **out_weights,
                                      NeuronTensor **out_grad_weights,
                                      NeuronTensor **out_bias,
                                      NeuronTensor **out_grad_bias) {
  if (out_weights != NULL) {
    *out_weights = NULL;
  }
  if (out_grad_weights != NULL) {
    *out_grad_weights = NULL;
  }
  if (out_bias != NULL) {
    *out_bias = NULL;
  }
  if (out_grad_bias != NULL) {
    *out_grad_bias = NULL;
  }
  if (layer_object == NULL) {
    return;
  }
  if (layer_kind == NEURON_LAYER_DENSE) {
    NeuronDenseLayer *layer = (NeuronDenseLayer *)layer_object;
    if (out_weights != NULL) {
      *out_weights = layer->weights;
    }
    if (out_grad_weights != NULL) {
      *out_grad_weights = layer->grad_weights;
    }
    if (out_bias != NULL) {
      *out_bias = layer->bias;
    }
    if (out_grad_bias != NULL) {
      *out_grad_bias = layer->grad_bias;
    }
  } else if (layer_kind == NEURON_LAYER_CONV2D) {
    NeuronConv2DLayer *layer = (NeuronConv2DLayer *)layer_object;
    if (out_weights != NULL) {
      *out_weights = layer->weights;
    }
    if (out_grad_weights != NULL) {
      *out_grad_weights = layer->grad_weights;
    }
    if (out_bias != NULL) {
      *out_bias = layer->bias;
    }
    if (out_grad_bias != NULL) {
      *out_grad_bias = layer->grad_bias;
    }
  }
}

static int model_ensure_capacity(NeuronSequentialModel *model) {
  const int32_t new_capacity = model->capacity * 2;
  NeuronDenseLayer **new_layers = NULL;
  int32_t *new_kinds = NULL;
  void **new_objects = NULL;
  if (model == NULL) {
    return 0;
  }
  if (model->count < model->capacity) {
    return 1;
  }
  new_layers = (NeuronDenseLayer **)neuron_alloc(sizeof(NeuronDenseLayer *) *
                                                 (size_t)new_capacity);
  new_kinds =
      (int32_t *)neuron_alloc(sizeof(int32_t) * (size_t)new_capacity);
  new_objects = (void **)neuron_alloc(sizeof(void *) * (size_t)new_capacity);
  if (new_layers == NULL || new_kinds == NULL || new_objects == NULL) {
    neuron_dealloc(new_layers);
    neuron_dealloc(new_kinds);
    neuron_dealloc(new_objects);
    return 0;
  }
  memcpy(new_layers, model->layers,
         sizeof(NeuronDenseLayer *) * (size_t)model->count);
  memcpy(new_kinds, model->layer_kinds, sizeof(int32_t) * (size_t)model->count);
  memcpy(new_objects, model->layer_objects, sizeof(void *) * (size_t)model->count);
  neuron_dealloc(model->layers);
  neuron_dealloc(model->layer_kinds);
  neuron_dealloc(model->layer_objects);
  model->layers = new_layers;
  model->layer_kinds = new_kinds;
  model->layer_objects = new_objects;
  model->capacity = new_capacity;
  return 1;
}

static int model_append_layer(NeuronSequentialModel *model, int32_t layer_kind,
                              void *layer_object, NeuronDenseLayer *dense_layer) {
  if (model == NULL || layer_object == NULL || !model_ensure_capacity(model)) {
    return 0;
  }
  model->layers[model->count] = dense_layer;
  model->layer_kinds[model->count] = layer_kind;
  model->layer_objects[model->count] = layer_object;
  model->count++;
  return 1;
}

NeuronSequentialModel *neuron_model_create(void) {
  NeuronSequentialModel *model = (NeuronSequentialModel *)neuron_alloc(
      sizeof(NeuronSequentialModel));
  if (model == NULL) {
    return NULL;
  }

  model->count = 0;
  model->capacity = 4;
  model->layer_kinds = NULL;
  model->layer_objects = NULL;
  model->layers = (NeuronDenseLayer **)neuron_alloc(
      sizeof(NeuronDenseLayer *) * (size_t)model->capacity);
  model->layer_kinds =
      (int32_t *)neuron_alloc(sizeof(int32_t) * (size_t)model->capacity);
  model->layer_objects =
      (void **)neuron_alloc(sizeof(void *) * (size_t)model->capacity);
  if (model->layers == NULL || model->layer_kinds == NULL ||
      model->layer_objects == NULL) {
    neuron_dealloc(model->layer_kinds);
    neuron_dealloc(model->layer_objects);
    neuron_dealloc(model);
    return NULL;
  }
  return model;
}

void neuron_model_free(NeuronSequentialModel *model) {
  if (model == NULL) {
    return;
  }
  for (int32_t i = 0; i < model->count; ++i) {
    layer_object_free(model->layer_kinds[i], model->layer_objects[i]);
  }
  neuron_dealloc(model->layers);
  neuron_dealloc(model->layer_kinds);
  neuron_dealloc(model->layer_objects);
  neuron_dealloc(model);
}

int neuron_model_add_layer(NeuronSequentialModel *model,
                           NeuronDenseLayer *layer) {
  if (model == NULL || layer == NULL) {
    return 0;
  }
  return model_append_layer(model, NEURON_LAYER_DENSE, layer, layer);
}

int neuron_model_add_conv2d_layer(NeuronSequentialModel *model,
                                  NeuronConv2DLayer *layer) {
  if (model == NULL || layer == NULL) {
    return 0;
  }
  return model_append_layer(model, NEURON_LAYER_CONV2D, layer, NULL);
}

int neuron_model_add_maxpool2d_layer(NeuronSequentialModel *model,
                                     NeuronMaxPool2DLayer *layer) {
  if (model == NULL || layer == NULL) {
    return 0;
  }
  return model_append_layer(model, NEURON_LAYER_MAXPOOL2D, layer, NULL);
}

int neuron_model_add_dropout_layer(NeuronSequentialModel *model,
                                   NeuronDropoutLayer *layer) {
  if (model == NULL || layer == NULL) {
    return 0;
  }
  return model_append_layer(model, NEURON_LAYER_DROPOUT, layer, NULL);
}

NeuronTensor *neuron_model_forward(NeuronSequentialModel *model,
                                   NeuronTensor *input, int training) {
  if (model == NULL || input == NULL) {
    return NULL;
  }
  if (model->count == 0) {
    return tensor_clone(input);
  }

  NeuronTensor *current = input;
  for (int32_t i = 0; i < model->count; ++i) {
    NeuronTensor *next = model_layer_forward(model->layer_kinds[i],
                                             model->layer_objects[i], current,
                                             training);
    if (next == NULL) {
      if (i > 0) {
        neuron_tensor_free(current);
      }
      return NULL;
    }
    if (i > 0) {
      neuron_tensor_free(current);
    }
    current = next;
  }
  return current;
}

int neuron_model_backward(NeuronSequentialModel *model, NeuronTensor *grad) {
  if (model == NULL || grad == NULL) {
    return -1;
  }
  if (model->count == 0) {
    return 0;
  }

  NeuronTensor *current_grad = grad;
  for (int32_t i = model->count - 1; i >= 0; --i) {
    NeuronTensor *prev_grad = model_layer_backward(
        model->layer_kinds[i], model->layer_objects[i], current_grad);
    if (i != model->count - 1) {
      neuron_tensor_free(current_grad);
    }
    if (prev_grad == NULL) {
      return -1;
    }
    current_grad = prev_grad;
  }

  neuron_tensor_free(current_grad);
  return 0;
}

void neuron_model_zero_grad(NeuronSequentialModel *model) {
  if (model == NULL) {
    return;
  }
  for (int32_t i = 0; i < model->count; ++i) {
    model_layer_zero_grad(model->layer_kinds[i], model->layer_objects[i]);
  }
}

int neuron_model_save(const NeuronSequentialModel *model, const char *path) {
  FILE *file = NULL;
  int ok = 1;

  if (model == NULL || path == NULL || path[0] == '\0') {
    return 0;
  }

  file = fopen(path, "wb");
  if (file == NULL) {
    return 0;
  }

  ok = write_bytes(file, kModelFileMagic, sizeof(kModelFileMagic));
  ok = ok && write_bytes(file, &kModelFileVersion, sizeof(kModelFileVersion));
  ok = ok && write_bytes(file, &model->count, sizeof(model->count));

  for (int32_t i = 0; ok && i < model->count; ++i) {
    const int32_t layer_kind = model->layer_kinds[i];
    ok = ok && write_bytes(file, &layer_kind, sizeof(layer_kind));

    if (layer_kind == NEURON_LAYER_DENSE) {
      const NeuronDenseLayer *layer =
          (const NeuronDenseLayer *)model->layer_objects[i];
      if (layer == NULL || !tensor_is_matrix(layer->weights) ||
          !tensor_is_matrix(layer->bias)) {
        ok = 0;
        break;
      }
      {
        const int32_t activation = (int32_t)layer->activation;
        const int32_t init_strategy = (int32_t)layer->init_strategy;
        ok = ok &&
             write_bytes(file, &layer->input_size, sizeof(layer->input_size));
        ok = ok &&
             write_bytes(file, &layer->output_size, sizeof(layer->output_size));
        ok = ok && write_bytes(file, &activation, sizeof(activation));
        ok = ok &&
             write_bytes(file, &layer->activation_param,
                         sizeof(layer->activation_param));
        ok = ok && write_bytes(file, &init_strategy, sizeof(init_strategy));
        ok = ok && write_bytes(file, layer->weights->data,
                               sizeof(float) * (size_t)layer->weights->size);
        ok = ok && write_bytes(file, layer->bias->data,
                               sizeof(float) * (size_t)layer->bias->size);
      }
    } else if (layer_kind == NEURON_LAYER_CONV2D) {
      const NeuronConv2DLayer *layer =
          (const NeuronConv2DLayer *)model->layer_objects[i];
      if (layer == NULL || !tensor_is_4d_nchw(layer->weights) ||
          !tensor_is_matrix(layer->bias)) {
        ok = 0;
        break;
      }
      {
        const int32_t activation = (int32_t)layer->activation;
        const int32_t init_strategy = (int32_t)layer->init_strategy;
        ok = ok && write_bytes(file, &layer->input_channels,
                               sizeof(layer->input_channels));
        ok = ok && write_bytes(file, &layer->output_channels,
                               sizeof(layer->output_channels));
        ok = ok && write_bytes(file, &layer->kernel_h, sizeof(layer->kernel_h));
        ok = ok && write_bytes(file, &layer->kernel_w, sizeof(layer->kernel_w));
        ok = ok && write_bytes(file, &layer->stride_h, sizeof(layer->stride_h));
        ok = ok && write_bytes(file, &layer->stride_w, sizeof(layer->stride_w));
        ok = ok &&
             write_bytes(file, &layer->padding_h, sizeof(layer->padding_h));
        ok = ok &&
             write_bytes(file, &layer->padding_w, sizeof(layer->padding_w));
        ok = ok && write_bytes(file, &activation, sizeof(activation));
        ok = ok &&
             write_bytes(file, &layer->activation_param,
                         sizeof(layer->activation_param));
        ok = ok && write_bytes(file, &init_strategy, sizeof(init_strategy));
        ok = ok && write_bytes(file, layer->weights->data,
                               sizeof(float) * (size_t)layer->weights->size);
        ok = ok && write_bytes(file, layer->bias->data,
                               sizeof(float) * (size_t)layer->bias->size);
      }
    } else if (layer_kind == NEURON_LAYER_MAXPOOL2D) {
      const NeuronMaxPool2DLayer *layer =
          (const NeuronMaxPool2DLayer *)model->layer_objects[i];
      if (layer == NULL) {
        ok = 0;
        break;
      }
      ok = ok && write_bytes(file, &layer->kernel_h, sizeof(layer->kernel_h));
      ok = ok && write_bytes(file, &layer->kernel_w, sizeof(layer->kernel_w));
      ok = ok && write_bytes(file, &layer->stride_h, sizeof(layer->stride_h));
      ok = ok && write_bytes(file, &layer->stride_w, sizeof(layer->stride_w));
    } else if (layer_kind == NEURON_LAYER_DROPOUT) {
      const NeuronDropoutLayer *layer =
          (const NeuronDropoutLayer *)model->layer_objects[i];
      if (layer == NULL) {
        ok = 0;
        break;
      }
      ok = ok &&
           write_bytes(file, &layer->drop_probability,
                       sizeof(layer->drop_probability));
      ok = ok && write_bytes(file, &layer->seed, sizeof(layer->seed));
      ok = ok && write_bytes(file, &layer->rng_state, sizeof(layer->rng_state));
    } else {
      ok = 0;
      break;
    }
  }

  fclose(file);
  return ok ? 1 : 0;
}

NeuronSequentialModel *neuron_model_load(const char *path) {
  FILE *file = NULL;
  NeuronSequentialModel *model = NULL;
  char magic[sizeof(kModelFileMagic)];
  uint32_t version = 0;
  int32_t layer_count = 0;

  if (path == NULL || path[0] == '\0') {
    return NULL;
  }

  file = fopen(path, "rb");
  if (file == NULL) {
    return NULL;
  }

  if (!read_bytes(file, magic, sizeof(magic)) ||
      memcmp(magic, kModelFileMagic, sizeof(magic)) != 0 ||
      !read_bytes(file, &version, sizeof(version)) ||
      (version != 1u && version != kModelFileVersion) ||
      !read_bytes(file, &layer_count, sizeof(layer_count)) ||
      layer_count < 0) {
    fclose(file);
    return NULL;
  }

  model = neuron_model_create();
  if (model == NULL) {
    fclose(file);
    return NULL;
  }

  for (int32_t i = 0; i < layer_count; ++i) {
    int32_t layer_kind = NEURON_LAYER_DENSE;

    if (version >= 2u && !read_bytes(file, &layer_kind, sizeof(layer_kind))) {
      goto fail;
    }

    if (layer_kind == NEURON_LAYER_DENSE) {
      int32_t input_size = 0;
      int32_t output_size = 0;
      int32_t activation = 0;
      int32_t init_strategy = (int32_t)NEURON_INIT_DEFAULT;
      float activation_param = 0.0f;
      NeuronDenseLayer *layer = NULL;
      if (!read_bytes(file, &input_size, sizeof(input_size)) ||
          !read_bytes(file, &output_size, sizeof(output_size)) ||
          !read_bytes(file, &activation, sizeof(activation)) ||
          !read_bytes(file, &activation_param, sizeof(activation_param)) ||
          (version >= 2u &&
           !read_bytes(file, &init_strategy, sizeof(init_strategy))) ||
          input_size <= 0 || output_size <= 0 ||
          !activation_is_supported(activation) ||
          !init_strategy_is_supported(init_strategy)) {
        goto fail;
      }
      layer = neuron_layer_dense_create(input_size, output_size,
                                        (NeuronActivationType)activation);
      if (layer == NULL) {
        goto fail;
      }
      layer->activation_param =
          activation_param_or_default((NeuronActivationType)activation,
                                      activation_param);
      layer->init_strategy = (NeuronInitStrategy)init_strategy;
      if (!read_bytes(file, layer->weights->data,
                      sizeof(float) * (size_t)layer->weights->size) ||
          !read_bytes(file, layer->bias->data,
                      sizeof(float) * (size_t)layer->bias->size) ||
          !neuron_model_add_layer(model, layer)) {
        neuron_layer_dense_free(layer);
        goto fail;
      }
    } else if (layer_kind == NEURON_LAYER_CONV2D) {
      int32_t input_channels = 0;
      int32_t output_channels = 0;
      int32_t kernel_h = 0;
      int32_t kernel_w = 0;
      int32_t stride_h = 0;
      int32_t stride_w = 0;
      int32_t padding_h = 0;
      int32_t padding_w = 0;
      int32_t activation = 0;
      int32_t init_strategy = 0;
      float activation_param = 0.0f;
      NeuronConv2DLayer *layer = NULL;
      if (!read_bytes(file, &input_channels, sizeof(input_channels)) ||
          !read_bytes(file, &output_channels, sizeof(output_channels)) ||
          !read_bytes(file, &kernel_h, sizeof(kernel_h)) ||
          !read_bytes(file, &kernel_w, sizeof(kernel_w)) ||
          !read_bytes(file, &stride_h, sizeof(stride_h)) ||
          !read_bytes(file, &stride_w, sizeof(stride_w)) ||
          !read_bytes(file, &padding_h, sizeof(padding_h)) ||
          !read_bytes(file, &padding_w, sizeof(padding_w)) ||
          !read_bytes(file, &activation, sizeof(activation)) ||
          !read_bytes(file, &activation_param, sizeof(activation_param)) ||
          !read_bytes(file, &init_strategy, sizeof(init_strategy)) ||
          !activation_is_supported(activation) ||
          !init_strategy_is_supported(init_strategy)) {
        goto fail;
      }
      layer = neuron_layer_conv2d_create(
          input_channels, output_channels, kernel_h, kernel_w, stride_h,
          stride_w, padding_h, padding_w, (NeuronActivationType)activation);
      if (layer == NULL) {
        goto fail;
      }
      layer->activation_param =
          activation_param_or_default((NeuronActivationType)activation,
                                      activation_param);
      layer->init_strategy = (NeuronInitStrategy)init_strategy;
      if (!read_bytes(file, layer->weights->data,
                      sizeof(float) * (size_t)layer->weights->size) ||
          !read_bytes(file, layer->bias->data,
                      sizeof(float) * (size_t)layer->bias->size) ||
          !neuron_model_add_conv2d_layer(model, layer)) {
        neuron_layer_conv2d_free(layer);
        goto fail;
      }
    } else if (layer_kind == NEURON_LAYER_MAXPOOL2D) {
      int32_t kernel_h = 0;
      int32_t kernel_w = 0;
      int32_t stride_h = 0;
      int32_t stride_w = 0;
      NeuronMaxPool2DLayer *layer = NULL;
      if (!read_bytes(file, &kernel_h, sizeof(kernel_h)) ||
          !read_bytes(file, &kernel_w, sizeof(kernel_w)) ||
          !read_bytes(file, &stride_h, sizeof(stride_h)) ||
          !read_bytes(file, &stride_w, sizeof(stride_w))) {
        goto fail;
      }
      layer =
          neuron_layer_maxpool2d_create(kernel_h, kernel_w, stride_h, stride_w);
      if (layer == NULL || !neuron_model_add_maxpool2d_layer(model, layer)) {
        neuron_layer_maxpool2d_free(layer);
        goto fail;
      }
    } else if (layer_kind == NEURON_LAYER_DROPOUT) {
      float drop_probability = 0.0f;
      uint32_t seed = 0;
      uint32_t rng_state = 0;
      NeuronDropoutLayer *layer = NULL;
      if (!read_bytes(file, &drop_probability, sizeof(drop_probability)) ||
          !read_bytes(file, &seed, sizeof(seed)) ||
          !read_bytes(file, &rng_state, sizeof(rng_state))) {
        goto fail;
      }
      layer = neuron_layer_dropout_create(drop_probability, seed);
      if (layer == NULL) {
        goto fail;
      }
      layer->rng_state = rng_state;
      if (!neuron_model_add_dropout_layer(model, layer)) {
        neuron_layer_dropout_free(layer);
        goto fail;
      }
    } else {
      goto fail;
    }
  }

  fclose(file);
  return model;

fail:
  fclose(file);
  neuron_model_free(model);
  return NULL;
}

float neuron_autograd_mse_loss(NeuronTensor *prediction, NeuronTensor *target,
                               NeuronTensor **out_grad) {
  if (!tensor_is_valid(prediction) || !tensor_is_valid(target) ||
      prediction->size != target->size ||
      prediction->dimensions != target->dimensions) {
    if (out_grad != NULL) {
      *out_grad = NULL;
    }
    return -1.0f;
  }

  float loss = 0.0f;
  float scale = 2.0f / (float)prediction->size;

  NeuronTensor *grad = NULL;
  if (out_grad != NULL) {
    grad = neuron_tensor_create(prediction->dimensions, prediction->shape);
    if (grad == NULL) {
      *out_grad = NULL;
      return -1.0f;
    }
  }

  for (int32_t i = 0; i < prediction->size; ++i) {
    float diff = prediction->data[i] - target->data[i];
    loss += diff * diff;
    if (grad != NULL) {
      grad->data[i] = scale * diff;
    }
  }
  loss /= (float)prediction->size;

  if (out_grad != NULL) {
    *out_grad = grad;
  }
  return loss;
}

float neuron_autograd_cross_entropy_loss(NeuronTensor *prediction,
                                         NeuronTensor *target,
                                         NeuronTensor **out_grad) {
  float loss = 0.0f;
  NeuronTensor *grad = NULL;

  if (!tensor_is_matrix(prediction) || !tensor_is_matrix(target) ||
      !tensor_has_same_shape(prediction, target) || prediction->shape[0] <= 0) {
    if (out_grad != NULL) {
      *out_grad = NULL;
    }
    return -1.0f;
  }

  if (out_grad != NULL) {
    grad = tensor_create_like(prediction);
    if (grad == NULL) {
      *out_grad = NULL;
      return -1.0f;
    }
  }

  for (int32_t r = 0; r < prediction->shape[0]; ++r) {
    const int32_t row_offset = r * prediction->shape[1];
    for (int32_t c = 0; c < prediction->shape[1]; ++c) {
      const int32_t index = row_offset + c;
      const float prob = prediction->data[index] < kCrossEntropyEpsilon
                             ? kCrossEntropyEpsilon
                             : prediction->data[index];
      const float label = target->data[index];
      if (label != 0.0f) {
        loss -= label * logf(prob);
      }
      if (grad != NULL) {
        grad->data[index] = label == 0.0f ? 0.0f : -(label / prob);
      }
    }
  }

  loss /= (float)prediction->shape[0];
  if (out_grad != NULL) {
    *out_grad = grad;
  }
  return loss;
}

void neuron_optimizer_sgd_init(NeuronSGDOptimizer *opt, float learning_rate,
                               float weight_decay) {
  if (opt == NULL) {
    return;
  }
  opt->learning_rate = learning_rate > 0.0f ? learning_rate : 0.01f;
  opt->weight_decay = weight_decay >= 0.0f ? weight_decay : 0.0f;
}

void neuron_optimizer_sgd_step(NeuronSequentialModel *model,
                               const NeuronSGDOptimizer *opt) {
  if (model == NULL || opt == NULL) {
    return;
  }

  for (int32_t i = 0; i < model->count; ++i) {
    NeuronTensor *weights = NULL;
    NeuronTensor *grad_weights = NULL;
    NeuronTensor *bias = NULL;
    NeuronTensor *grad_bias = NULL;
    int weightsUpdated = 0;
    model_layer_param_tensors(model->layer_kinds[i], model->layer_objects[i],
                              &weights, &grad_weights, &bias, &grad_bias);
    if (weights != NULL && grad_weights != NULL &&
        tensor_has_same_shape(weights, grad_weights)) {
      for (int32_t j = 0; j < weights->size; ++j) {
        float grad = grad_weights->data[j];
        if (opt->weight_decay > 0.0f) {
          grad += opt->weight_decay * weights->data[j];
        }
        weights->data[j] -= opt->learning_rate * grad;
      }
      weightsUpdated = 1;
    }

    if (weightsUpdated && model->layer_kinds[i] == NEURON_LAYER_DENSE &&
        model->layers[i] != NULL) {
      neuron_tensor_packed_free(model->layers[i]->packed_weights);
      model->layers[i]->packed_weights = NULL;
      model->layers[i]->packed_weights_version++;
    }

    if (bias != NULL && grad_bias != NULL && tensor_has_same_shape(bias, grad_bias)) {
      for (int32_t j = 0; j < bias->size; ++j) {
        bias->data[j] -= opt->learning_rate * grad_bias->data[j];
      }
    }
  }
}

int neuron_optimizer_adam_init(NeuronAdamOptimizer *opt,
                               const NeuronSequentialModel *model,
                               float learning_rate, float weight_decay) {
  if (opt == NULL) {
    return 0;
  }

  memset(opt, 0, sizeof(*opt));
  opt->learning_rate = learning_rate > 0.0f ? learning_rate : 0.001f;
  opt->weight_decay = weight_decay >= 0.0f ? weight_decay : 0.0f;
  opt->beta1 = 0.9f;
  opt->beta2 = 0.999f;
  opt->epsilon = 1.0e-8f;
  opt->timestep = 0;

  if (model == NULL) {
    return 1;
  }
  return ensure_adam_state(opt, model);
}

void neuron_optimizer_adam_free(NeuronAdamOptimizer *opt) {
  if (opt == NULL) {
    return;
  }

  free_tensor_slots(opt->weight_moments, opt->layer_capacity);
  free_tensor_slots(opt->weight_velocities, opt->layer_capacity);
  free_tensor_slots(opt->bias_moments, opt->layer_capacity);
  free_tensor_slots(opt->bias_velocities, opt->layer_capacity);
  memset(opt, 0, sizeof(*opt));
}

void neuron_optimizer_adam_step(NeuronSequentialModel *model,
                                NeuronAdamOptimizer *opt) {
  if (model == NULL || opt == NULL || model->count == 0) {
    return;
  }
  if (!ensure_adam_state(opt, model)) {
    return;
  }

  opt->timestep++;
  {
    float beta1_correction = 1.0f - powf(opt->beta1, (float)opt->timestep);
    float beta2_correction = 1.0f - powf(opt->beta2, (float)opt->timestep);
    if (beta1_correction <= 0.0f) {
      beta1_correction = 1.0e-8f;
    }
    if (beta2_correction <= 0.0f) {
      beta2_correction = 1.0e-8f;
    }

    for (int32_t i = 0; i < model->count; ++i) {
      NeuronTensor *weights = NULL;
      NeuronTensor *grad_weights = NULL;
      NeuronTensor *bias = NULL;
      NeuronTensor *grad_bias = NULL;
      model_layer_param_tensors(model->layer_kinds[i], model->layer_objects[i],
                                &weights, &grad_weights, &bias, &grad_bias);

      if (weights != NULL && grad_weights != NULL &&
          tensor_has_same_shape(weights, grad_weights) &&
          opt->weight_moments[i] != NULL && opt->weight_velocities[i] != NULL) {
        for (int32_t j = 0; j < weights->size; ++j) {
          float grad = grad_weights->data[j];
          float *moment = &opt->weight_moments[i]->data[j];
          float *velocity = &opt->weight_velocities[i]->data[j];
          if (opt->weight_decay > 0.0f) {
            grad += opt->weight_decay * weights->data[j];
          }
          *moment = opt->beta1 * (*moment) + (1.0f - opt->beta1) * grad;
          *velocity =
              opt->beta2 * (*velocity) + (1.0f - opt->beta2) * grad * grad;
          weights->data[j] -=
              opt->learning_rate * ((*moment) / beta1_correction) /
              (sqrtf((*velocity) / beta2_correction) + opt->epsilon);
        }
        if (model->layer_kinds[i] == NEURON_LAYER_DENSE &&
            model->layers[i] != NULL) {
          neuron_tensor_packed_free(model->layers[i]->packed_weights);
          model->layers[i]->packed_weights = NULL;
          model->layers[i]->packed_weights_version++;
        }
      }

      if (bias != NULL && grad_bias != NULL &&
          tensor_has_same_shape(bias, grad_bias) &&
          opt->bias_moments[i] != NULL && opt->bias_velocities[i] != NULL) {
        for (int32_t j = 0; j < bias->size; ++j) {
          const float grad = grad_bias->data[j];
          float *moment = &opt->bias_moments[i]->data[j];
          float *velocity = &opt->bias_velocities[i]->data[j];
          *moment = opt->beta1 * (*moment) + (1.0f - opt->beta1) * grad;
          *velocity =
              opt->beta2 * (*velocity) + (1.0f - opt->beta2) * grad * grad;
          bias->data[j] -=
              opt->learning_rate * ((*moment) / beta1_correction) /
              (sqrtf((*velocity) / beta2_correction) + opt->epsilon);
        }
      }
    }
  }
}

NeuronDataset *neuron_dataset_create(int32_t sample_count, int32_t feature_dim,
                                     int32_t target_dim) {
  if (sample_count <= 0 || feature_dim <= 0 || target_dim <= 0) {
    return NULL;
  }

  NeuronDataset *dataset = (NeuronDataset *)neuron_alloc(sizeof(*dataset));
  if (dataset == NULL) {
    return NULL;
  }
  memset(dataset, 0, sizeof(*dataset));

  dataset->sample_count = sample_count;
  dataset->feature_dim = feature_dim;
  dataset->target_dim = target_dim;

  int32_t feature_shape[2] = {sample_count, feature_dim};
  int32_t target_shape[2] = {sample_count, target_dim};
  dataset->features = neuron_tensor_create(2, feature_shape);
  dataset->targets = neuron_tensor_create(2, target_shape);

  if (dataset->features == NULL || dataset->targets == NULL) {
    neuron_dataset_free(dataset);
    return NULL;
  }

  return dataset;
}

void neuron_dataset_free(NeuronDataset *dataset) {
  if (dataset == NULL) {
    return;
  }
  neuron_tensor_free(dataset->features);
  neuron_tensor_free(dataset->targets);
  neuron_dealloc(dataset);
}

int neuron_dataset_set_sample(NeuronDataset *dataset, int32_t index,
                              const float *features, const float *targets) {
  if (dataset == NULL || features == NULL || targets == NULL) {
    return 0;
  }
  if (index < 0 || index >= dataset->sample_count) {
    return 0;
  }

  float *feature_row = dataset->features->data + index * dataset->feature_dim;
  float *target_row = dataset->targets->data + index * dataset->target_dim;

  memcpy(feature_row, features, sizeof(float) * (size_t)dataset->feature_dim);
  memcpy(target_row, targets, sizeof(float) * (size_t)dataset->target_dim);
  return 1;
}

NeuronDataLoader *neuron_dataloader_create(const NeuronDataset *dataset,
                                           int32_t batch_size, int shuffle,
                                           uint32_t seed) {
  if (dataset == NULL || batch_size <= 0) {
    return NULL;
  }

  NeuronDataLoader *loader = (NeuronDataLoader *)neuron_alloc(sizeof(*loader));
  if (loader == NULL) {
    return NULL;
  }
  memset(loader, 0, sizeof(*loader));

  loader->dataset = dataset;
  loader->batch_size = batch_size;
  loader->shuffle = shuffle ? 1 : 0;
  loader->rng_state = seed == 0U ? 0x12345678U : seed;

  loader->indices = (int32_t *)neuron_alloc(sizeof(int32_t) *
                                            (size_t)dataset->sample_count);
  if (loader->indices == NULL) {
    neuron_dealloc(loader);
    return NULL;
  }

  neuron_dataloader_reset(loader);
  return loader;
}

void neuron_dataloader_free(NeuronDataLoader *loader) {
  if (loader == NULL) {
    return;
  }
  neuron_dealloc(loader->indices);
  neuron_dealloc(loader);
}

void neuron_dataloader_reset(NeuronDataLoader *loader) {
  if (loader == NULL || loader->dataset == NULL || loader->indices == NULL) {
    return;
  }

  loader->cursor = 0;
  for (int32_t i = 0; i < loader->dataset->sample_count; ++i) {
    loader->indices[i] = i;
  }

  if (!loader->shuffle) {
    return;
  }

  for (int32_t i = loader->dataset->sample_count - 1; i > 0; --i) {
    uint32_t r = rng_next(&loader->rng_state);
    int32_t j = (int32_t)(r % (uint32_t)(i + 1));
    int32_t tmp = loader->indices[i];
    loader->indices[i] = loader->indices[j];
    loader->indices[j] = tmp;
  }
}

int neuron_dataloader_next(NeuronDataLoader *loader, NeuronTensor **out_features,
                           NeuronTensor **out_targets, int32_t *out_count) {
  if (loader == NULL || loader->dataset == NULL || out_features == NULL ||
      out_targets == NULL) {
    return -1;
  }

  *out_features = NULL;
  *out_targets = NULL;
  if (out_count != NULL) {
    *out_count = 0;
  }

  int32_t total = loader->dataset->sample_count;
  if (loader->cursor >= total) {
    return 0;
  }

  int32_t remaining = total - loader->cursor;
  int32_t actual_batch =
      remaining < loader->batch_size ? remaining : loader->batch_size;

  int32_t x_shape[2] = {actual_batch, loader->dataset->feature_dim};
  int32_t y_shape[2] = {actual_batch, loader->dataset->target_dim};
  NeuronTensor *batch_x = neuron_tensor_create(2, x_shape);
  NeuronTensor *batch_y = neuron_tensor_create(2, y_shape);
  if (batch_x == NULL || batch_y == NULL) {
    neuron_tensor_free(batch_x);
    neuron_tensor_free(batch_y);
    return -1;
  }

  for (int32_t i = 0; i < actual_batch; ++i) {
    int32_t sample_idx = loader->indices[loader->cursor + i];
    const float *src_x =
        loader->dataset->features->data + sample_idx * loader->dataset->feature_dim;
    const float *src_y =
        loader->dataset->targets->data + sample_idx * loader->dataset->target_dim;
    float *dst_x = batch_x->data + i * loader->dataset->feature_dim;
    float *dst_y = batch_y->data + i * loader->dataset->target_dim;
    memcpy(dst_x, src_x, sizeof(float) * (size_t)loader->dataset->feature_dim);
    memcpy(dst_y, src_y, sizeof(float) * (size_t)loader->dataset->target_dim);
  }

  loader->cursor += actual_batch;
  *out_features = batch_x;
  *out_targets = batch_y;
  if (out_count != NULL) {
    *out_count = actual_batch;
  }
  return 1;
}

static float evaluate_dataset_loss(NeuronSequentialModel *model,
                                   NeuronDataLoader *loader) {
  if (model == NULL || loader == NULL) {
    return -1.0f;
  }

  neuron_dataloader_reset(loader);

  float loss_sum = 0.0f;
  int32_t batches = 0;

  while (1) {
    NeuronTensor *x = NULL;
    NeuronTensor *y = NULL;
    int32_t count = 0;
    int next = neuron_dataloader_next(loader, &x, &y, &count);
    if (next <= 0) {
      break;
    }

    NeuronTensor *pred = neuron_model_forward(model, x, 0);
    if (pred != NULL) {
      float loss = neuron_autograd_mse_loss(pred, y, NULL);
      if (loss >= 0.0f) {
        loss_sum += loss;
        batches++;
      }
    }

    neuron_tensor_free(pred);
    neuron_tensor_free(x);
    neuron_tensor_free(y);
  }

  if (batches == 0) {
    return -1.0f;
  }
  return loss_sum / (float)batches;
}

int64_t neuron_nn_self_test(void) {
  const int32_t sample_count = 16;
  const int32_t epochs = 220;
  const int32_t batch_size = 4;

  int64_t success = 0;
  NeuronDataset *dataset = neuron_dataset_create(sample_count, 1, 1);
  NeuronSequentialModel *model = NULL;
  NeuronDenseLayer *layer = NULL;
  NeuronDataLoader *loader = NULL;

  if (dataset == NULL) {
    return 0;
  }

  for (int32_t i = 0; i < sample_count; ++i) {
    float x = -1.0f + 2.0f * (float)i / (float)(sample_count - 1);
    float y = 2.0f * x + 1.0f;
    if (!neuron_dataset_set_sample(dataset, i, &x, &y)) {
      goto cleanup;
    }
  }

  model = neuron_model_create();
  if (model == NULL) {
    goto cleanup;
  }

  layer = neuron_layer_dense_create(1, 1, NEURON_ACTIVATION_LINEAR);
  if (layer == NULL) {
    goto cleanup;
  }
  layer->weights->data[0] = 0.0f;
  layer->bias->data[0] = 0.0f;
  if (!neuron_model_add_layer(model, layer)) {
    goto cleanup;
  }
  layer = NULL; // ownership moved to model

  loader = neuron_dataloader_create(dataset, batch_size, 1, 123u);
  if (loader == NULL) {
    goto cleanup;
  }

  NeuronSGDOptimizer opt;
  neuron_optimizer_sgd_init(&opt, 0.05f, 0.0f);

  float initial_loss = evaluate_dataset_loss(model, loader);
  if (initial_loss <= 0.0f) {
    goto cleanup;
  }

  for (int32_t epoch = 0; epoch < epochs; ++epoch) {
    neuron_dataloader_reset(loader);
    while (1) {
      NeuronTensor *batch_x = NULL;
      NeuronTensor *batch_y = NULL;
      NeuronTensor *grad = NULL;
      int32_t count = 0;

      int next = neuron_dataloader_next(loader, &batch_x, &batch_y, &count);
      if (next <= 0) {
        break;
      }

      NeuronTensor *pred = neuron_model_forward(model, batch_x, 1);
      if (pred == NULL) {
        neuron_tensor_free(batch_x);
        neuron_tensor_free(batch_y);
        goto cleanup;
      }

      if (neuron_autograd_mse_loss(pred, batch_y, &grad) < 0.0f || grad == NULL) {
        neuron_tensor_free(pred);
        neuron_tensor_free(batch_x);
        neuron_tensor_free(batch_y);
        neuron_tensor_free(grad);
        goto cleanup;
      }

      if (neuron_model_backward(model, grad) != 0) {
        neuron_tensor_free(pred);
        neuron_tensor_free(batch_x);
        neuron_tensor_free(batch_y);
        neuron_tensor_free(grad);
        goto cleanup;
      }

      neuron_optimizer_sgd_step(model, &opt);
      neuron_model_zero_grad(model);

      neuron_tensor_free(pred);
      neuron_tensor_free(batch_x);
      neuron_tensor_free(batch_y);
      neuron_tensor_free(grad);
    }
  }

  {
    float final_loss = evaluate_dataset_loss(model, loader);
    float w = model->layers[0]->weights->data[0];
    float b = model->layers[0]->bias->data[0];

    if (final_loss >= 0.0f && final_loss < initial_loss * 0.25f &&
        fabsf(w - 2.0f) < 0.5f && fabsf(b - 1.0f) < 0.5f) {
      success = 1;
    }
  }

cleanup:
  neuron_dataloader_free(loader);
  neuron_model_free(model);
  neuron_layer_dense_free(layer);
  neuron_dataset_free(dataset);
  return success;
}
