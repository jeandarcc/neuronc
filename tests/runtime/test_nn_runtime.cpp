#include <cmath>
#include <filesystem>

extern "C" {
#include "neuron_gpu.h"
#include "neuron_nn.h"
}

namespace fs = std::filesystem;

static bool approx_equal(float a, float b, float epsilon = 1.0e-4f) {
  return std::fabs(a - b) <= epsilon;
}

static NeuronDataset *create_linear_dataset() {
  const int32_t sample_count = 16;
  NeuronDataset *dataset = neuron_dataset_create(sample_count, 1, 1);
  if (dataset == nullptr) {
    return nullptr;
  }

  for (int32_t i = 0; i < sample_count; ++i) {
    float x = -1.0f + 2.0f * (float)i / (float)(sample_count - 1);
    float y = 2.0f * x + 1.0f;
    if (!neuron_dataset_set_sample(dataset, i, &x, &y)) {
      neuron_dataset_free(dataset);
      return nullptr;
    }
  }

  return dataset;
}

static NeuronDataset *create_three_class_dataset() {
  NeuronDataset *dataset = neuron_dataset_create(12, 3, 3);
  if (dataset == nullptr) {
    return nullptr;
  }

  for (int32_t i = 0; i < 12; ++i) {
    const int32_t klass = i % 3;
    float features[3] = {0.0f, 0.0f, 0.0f};
    float targets[3] = {0.0f, 0.0f, 0.0f};
    features[klass] = 1.0f;
    targets[klass] = 1.0f;
    if (!neuron_dataset_set_sample(dataset, i, features, targets)) {
      neuron_dataset_free(dataset);
      return nullptr;
    }
  }

  return dataset;
}

static NeuronTensor *create_4d_tensor(int32_t n, int32_t c, int32_t h,
                                      int32_t w, const float *values) {
  int32_t shape[4] = {n, c, h, w};
  NeuronTensor *tensor = neuron_tensor_create(4, shape);
  if (tensor == nullptr) {
    return nullptr;
  }
  for (int32_t i = 0; i < tensor->size; ++i) {
    tensor->data[i] = values[i];
  }
  return tensor;
}

static float evaluate_dataset_loss(NeuronSequentialModel *model,
                                   NeuronDataLoader *loader) {
  neuron_dataloader_reset(loader);
  float total_loss = 0.0f;
  int32_t total_batches = 0;

  while (true) {
    NeuronTensor *batch_x = nullptr;
    NeuronTensor *batch_y = nullptr;
    int32_t batch_count = 0;
    int next = neuron_dataloader_next(loader, &batch_x, &batch_y, &batch_count);
    if (next <= 0) {
      break;
    }

    NeuronTensor *pred = neuron_model_forward(model, batch_x, 0);
    if (pred != nullptr) {
      total_loss += neuron_autograd_mse_loss(pred, batch_y, nullptr);
      total_batches++;
    }
    neuron_tensor_free(pred);
    neuron_tensor_free(batch_x);
    neuron_tensor_free(batch_y);
  }

  if (total_batches == 0) {
    return -1.0f;
  }
  return total_loss / (float)total_batches;
}

static float evaluate_classification_loss(NeuronSequentialModel *model,
                                          NeuronDataset *dataset) {
  NeuronTensor *pred = neuron_model_forward(model, dataset->features, 0);
  if (pred == nullptr) {
    return -1.0f;
  }
  const float loss =
      neuron_autograd_cross_entropy_loss(pred, dataset->targets, nullptr);
  neuron_tensor_free(pred);
  return loss;
}

static int32_t evaluate_classification_accuracy(NeuronSequentialModel *model,
                                                NeuronDataset *dataset) {
  int32_t correct = 0;
  NeuronTensor *pred = neuron_model_forward(model, dataset->features, 0);
  if (pred == nullptr) {
    return 0;
  }

  for (int32_t r = 0; r < dataset->sample_count; ++r) {
    int32_t pred_index = 0;
    int32_t target_index = 0;
    const int32_t row_offset = r * dataset->target_dim;
    for (int32_t c = 1; c < dataset->target_dim; ++c) {
      if (pred->data[row_offset + c] > pred->data[row_offset + pred_index]) {
        pred_index = c;
      }
      if (dataset->targets->data[row_offset + c] >
          dataset->targets->data[row_offset + target_index]) {
        target_index = c;
      }
    }
    if (pred_index == target_index) {
      correct++;
    }
  }

  neuron_tensor_free(pred);
  return correct;
}

TEST(DataLoaderBatchesWithoutShuffle) {
  NeuronDataset *dataset = neuron_dataset_create(5, 1, 1);
  ASSERT_TRUE(dataset != nullptr);

  for (int32_t i = 0; i < 5; ++i) {
    float x = (float)i;
    float y = (float)(10 + i);
    ASSERT_TRUE(neuron_dataset_set_sample(dataset, i, &x, &y));
  }

  NeuronDataLoader *loader = neuron_dataloader_create(dataset, 2, 0, 1);
  ASSERT_TRUE(loader != nullptr);

  NeuronTensor *x = nullptr;
  NeuronTensor *y = nullptr;
  int32_t count = 0;

  ASSERT_EQ(neuron_dataloader_next(loader, &x, &y, &count), 1);
  ASSERT_EQ(count, 2);
  ASSERT_TRUE(x->data[0] == 0.0f && x->data[1] == 1.0f);
  ASSERT_TRUE(y->data[0] == 10.0f && y->data[1] == 11.0f);
  neuron_tensor_free(x);
  neuron_tensor_free(y);

  ASSERT_EQ(neuron_dataloader_next(loader, &x, &y, &count), 1);
  ASSERT_EQ(count, 2);
  ASSERT_TRUE(x->data[0] == 2.0f && x->data[1] == 3.0f);
  ASSERT_TRUE(y->data[0] == 12.0f && y->data[1] == 13.0f);
  neuron_tensor_free(x);
  neuron_tensor_free(y);

  ASSERT_EQ(neuron_dataloader_next(loader, &x, &y, &count), 1);
  ASSERT_EQ(count, 1);
  ASSERT_TRUE(x->data[0] == 4.0f);
  ASSERT_TRUE(y->data[0] == 14.0f);
  neuron_tensor_free(x);
  neuron_tensor_free(y);

  ASSERT_EQ(neuron_dataloader_next(loader, &x, &y, &count), 0);

  neuron_dataloader_free(loader);
  neuron_dataset_free(dataset);
  return true;
}

TEST(NeuralTrainingLinearRegressionConverges) {
  NeuronDataset *dataset = create_linear_dataset();
  ASSERT_TRUE(dataset != nullptr);

  NeuronSequentialModel *model = neuron_model_create();
  ASSERT_TRUE(model != nullptr);

  NeuronDenseLayer *layer =
      neuron_layer_dense_create(1, 1, NEURON_ACTIVATION_LINEAR);
  ASSERT_TRUE(layer != nullptr);
  layer->weights->data[0] = 0.0f;
  layer->bias->data[0] = 0.0f;
  ASSERT_TRUE(neuron_model_add_layer(model, layer));

  NeuronDataLoader *loader = neuron_dataloader_create(dataset, 4, 1, 123);
  ASSERT_TRUE(loader != nullptr);

  NeuronSGDOptimizer opt;
  neuron_optimizer_sgd_init(&opt, 0.05f, 0.0f);

  float initial_loss = evaluate_dataset_loss(model, loader);
  ASSERT_TRUE(initial_loss > 0.1f);

  for (int epoch = 0; epoch < 240; ++epoch) {
    neuron_dataloader_reset(loader);
    while (true) {
      NeuronTensor *batch_x = nullptr;
      NeuronTensor *batch_y = nullptr;
      int32_t batch_count = 0;
      int next =
          neuron_dataloader_next(loader, &batch_x, &batch_y, &batch_count);
      if (next <= 0) {
        break;
      }

      NeuronTensor *pred = neuron_model_forward(model, batch_x, 1);
      ASSERT_TRUE(pred != nullptr);

      NeuronTensor *loss_grad = nullptr;
      float loss = neuron_autograd_mse_loss(pred, batch_y, &loss_grad);
      ASSERT_TRUE(loss >= 0.0f);
      ASSERT_TRUE(loss_grad != nullptr);

      ASSERT_EQ(neuron_model_backward(model, loss_grad), 0);
      neuron_optimizer_sgd_step(model, &opt);
      neuron_model_zero_grad(model);

      neuron_tensor_free(loss_grad);
      neuron_tensor_free(pred);
      neuron_tensor_free(batch_x);
      neuron_tensor_free(batch_y);
    }
  }

  float final_loss = evaluate_dataset_loss(model, loader);
  ASSERT_TRUE(final_loss >= 0.0f);
  ASSERT_TRUE(final_loss < initial_loss * 0.2f);
  ASSERT_TRUE(std::fabs(layer->weights->data[0] - 2.0f) < 0.35f);
  ASSERT_TRUE(std::fabs(layer->bias->data[0] - 1.0f) < 0.35f);

  neuron_dataloader_free(loader);
  neuron_model_free(model);
  neuron_dataset_free(dataset);
  return true;
}

TEST(ModelSupportsReluBackprop) {
  NeuronSequentialModel *model = neuron_model_create();
  ASSERT_TRUE(model != nullptr);

  NeuronDenseLayer *l1 = neuron_layer_dense_create(1, 4, NEURON_ACTIVATION_RELU);
  NeuronDenseLayer *l2 =
      neuron_layer_dense_create(4, 1, NEURON_ACTIVATION_LINEAR);
  ASSERT_TRUE(l1 != nullptr && l2 != nullptr);
  ASSERT_TRUE(neuron_model_add_layer(model, l1));
  ASSERT_TRUE(neuron_model_add_layer(model, l2));

  int32_t x_shape[2] = {3, 1};
  NeuronTensor *x = neuron_tensor_create(2, x_shape);
  ASSERT_TRUE(x != nullptr);
  x->data[0] = -1.0f;
  x->data[1] = 0.0f;
  x->data[2] = 2.0f;

  NeuronTensor *y = neuron_model_forward(model, x, 1);
  ASSERT_TRUE(y != nullptr);
  ASSERT_EQ(y->shape[0], 3);
  ASSERT_EQ(y->shape[1], 1);

  int32_t gy_shape[2] = {3, 1};
  NeuronTensor *gy = neuron_tensor_create(2, gy_shape);
  ASSERT_TRUE(gy != nullptr);
  gy->data[0] = 1.0f;
  gy->data[1] = 1.0f;
  gy->data[2] = 1.0f;

  ASSERT_EQ(neuron_model_backward(model, gy), 0);
  ASSERT_TRUE(l1->grad_weights != nullptr);
  ASSERT_TRUE(l2->grad_weights != nullptr);
  ASSERT_TRUE(l1->grad_bias != nullptr);
  ASSERT_TRUE(l2->grad_bias != nullptr);

  neuron_tensor_free(gy);
  neuron_tensor_free(y);
  neuron_tensor_free(x);
  neuron_model_free(model);
  return true;
}

TEST(ActivationsSupportSigmoidTanhLeakyReluAndSoftmax) {
  int32_t scalar_shape[2] = {1, 1};
  int32_t vector_shape[2] = {1, 3};

  NeuronTensor *scalar_input = neuron_tensor_create(2, scalar_shape);
  ASSERT_TRUE(scalar_input != nullptr);
  scalar_input->data[0] = 0.0f;

  NeuronDenseLayer *sigmoid =
      neuron_layer_dense_create(1, 1, NEURON_ACTIVATION_SIGMOID);
  ASSERT_TRUE(sigmoid != nullptr);
  sigmoid->weights->data[0] = 1.0f;
  sigmoid->bias->data[0] = 0.0f;
  NeuronTensor *sigmoid_out = neuron_layer_dense_forward(sigmoid, scalar_input, 0);
  ASSERT_TRUE(sigmoid_out != nullptr);
  ASSERT_TRUE(approx_equal(sigmoid_out->data[0], 0.5f, 1.0e-4f));

  NeuronDenseLayer *tanh_layer =
      neuron_layer_dense_create(1, 1, NEURON_ACTIVATION_TANH);
  ASSERT_TRUE(tanh_layer != nullptr);
  tanh_layer->weights->data[0] = 1.0f;
  tanh_layer->bias->data[0] = 0.0f;
  NeuronTensor *tanh_out =
      neuron_layer_dense_forward(tanh_layer, scalar_input, 0);
  ASSERT_TRUE(tanh_out != nullptr);
  ASSERT_TRUE(approx_equal(tanh_out->data[0], 0.0f, 1.0e-5f));

  scalar_input->data[0] = -2.0f;
  NeuronDenseLayer *leaky =
      neuron_layer_dense_create(1, 1, NEURON_ACTIVATION_LEAKY_RELU);
  ASSERT_TRUE(leaky != nullptr);
  leaky->activation_param = 0.1f;
  leaky->weights->data[0] = 1.0f;
  leaky->bias->data[0] = 0.0f;
  NeuronTensor *leaky_out =
      neuron_layer_dense_forward(leaky, scalar_input, 0);
  ASSERT_TRUE(leaky_out != nullptr);
  ASSERT_TRUE(approx_equal(leaky_out->data[0], -0.2f, 1.0e-4f));

  NeuronTensor *vector_input = neuron_tensor_create(2, vector_shape);
  ASSERT_TRUE(vector_input != nullptr);
  vector_input->data[0] = 1.0f;
  vector_input->data[1] = 2.0f;
  vector_input->data[2] = 3.0f;

  NeuronDenseLayer *softmax =
      neuron_layer_dense_create(3, 3, NEURON_ACTIVATION_SOFTMAX);
  ASSERT_TRUE(softmax != nullptr);
  for (int32_t i = 0; i < softmax->weights->size; ++i) {
    softmax->weights->data[i] = 0.0f;
  }
  softmax->weights->data[0] = 1.0f;
  softmax->weights->data[4] = 1.0f;
  softmax->weights->data[8] = 1.0f;
  for (int32_t i = 0; i < softmax->bias->size; ++i) {
    softmax->bias->data[i] = 0.0f;
  }
  NeuronTensor *softmax_out =
      neuron_layer_dense_forward(softmax, vector_input, 0);
  ASSERT_TRUE(softmax_out != nullptr);
  ASSERT_TRUE(approx_equal(softmax_out->data[0] + softmax_out->data[1] +
                               softmax_out->data[2],
                           1.0f, 1.0e-4f));
  ASSERT_TRUE(softmax_out->data[2] > softmax_out->data[1]);
  ASSERT_TRUE(softmax_out->data[1] > softmax_out->data[0]);

  neuron_tensor_free(softmax_out);
  neuron_tensor_free(vector_input);
  neuron_layer_dense_free(softmax);
  neuron_tensor_free(leaky_out);
  neuron_layer_dense_free(leaky);
  neuron_tensor_free(tanh_out);
  neuron_layer_dense_free(tanh_layer);
  neuron_tensor_free(sigmoid_out);
  neuron_layer_dense_free(sigmoid);
  neuron_tensor_free(scalar_input);
  return true;
}

TEST(CrossEntropyLossReturnsStableGradient) {
  int32_t shape[2] = {2, 3};
  NeuronTensor *prediction = neuron_tensor_create(2, shape);
  NeuronTensor *target = neuron_tensor_create(2, shape);
  ASSERT_TRUE(prediction != nullptr && target != nullptr);

  prediction->data[0] = 0.7f;
  prediction->data[1] = 0.2f;
  prediction->data[2] = 0.1f;
  prediction->data[3] = 0.1f;
  prediction->data[4] = 0.3f;
  prediction->data[5] = 0.6f;

  target->data[0] = 1.0f;
  target->data[1] = 0.0f;
  target->data[2] = 0.0f;
  target->data[3] = 0.0f;
  target->data[4] = 0.0f;
  target->data[5] = 1.0f;

  NeuronTensor *grad = nullptr;
  const float loss =
      neuron_autograd_cross_entropy_loss(prediction, target, &grad);
  ASSERT_TRUE(loss > 0.0f);
  ASSERT_TRUE(grad != nullptr);
  ASSERT_TRUE(approx_equal(
      loss, (-std::log(0.7f) - std::log(0.6f)) / 2.0f, 1.0e-4f));
  ASSERT_TRUE(approx_equal(grad->data[0], -1.0f / 0.7f, 1.0e-4f));
  ASSERT_TRUE(approx_equal(grad->data[5], -1.0f / 0.6f, 1.0e-4f));
  ASSERT_TRUE(approx_equal(grad->data[1], 0.0f, 1.0e-6f));
  ASSERT_TRUE(approx_equal(grad->data[4], 0.0f, 1.0e-6f));

  neuron_tensor_free(grad);
  neuron_tensor_free(target);
  neuron_tensor_free(prediction);
  return true;
}

TEST(AdamOptimizerTrainsSoftmaxClassifier) {
  NeuronDataset *dataset = create_three_class_dataset();
  ASSERT_TRUE(dataset != nullptr);

  NeuronSequentialModel *model = neuron_model_create();
  ASSERT_TRUE(model != nullptr);

  NeuronDenseLayer *layer =
      neuron_layer_dense_create(3, 3, NEURON_ACTIVATION_SOFTMAX);
  ASSERT_TRUE(layer != nullptr);
  for (int32_t i = 0; i < layer->weights->size; ++i) {
    layer->weights->data[i] = 0.0f;
  }
  for (int32_t i = 0; i < layer->bias->size; ++i) {
    layer->bias->data[i] = 0.0f;
  }
  ASSERT_TRUE(neuron_model_add_layer(model, layer));

  NeuronDataLoader *loader = neuron_dataloader_create(dataset, 3, 1, 321);
  ASSERT_TRUE(loader != nullptr);

  NeuronAdamOptimizer opt;
  ASSERT_TRUE(neuron_optimizer_adam_init(&opt, model, 0.05f, 0.0f));

  const float initial_loss = evaluate_classification_loss(model, dataset);
  ASSERT_TRUE(initial_loss > 0.5f);

  for (int epoch = 0; epoch < 180; ++epoch) {
    neuron_dataloader_reset(loader);
    while (true) {
      NeuronTensor *batch_x = nullptr;
      NeuronTensor *batch_y = nullptr;
      int32_t batch_count = 0;
      int next =
          neuron_dataloader_next(loader, &batch_x, &batch_y, &batch_count);
      if (next <= 0) {
        break;
      }

      NeuronTensor *pred = neuron_model_forward(model, batch_x, 1);
      ASSERT_TRUE(pred != nullptr);

      NeuronTensor *loss_grad = nullptr;
      const float loss =
          neuron_autograd_cross_entropy_loss(pred, batch_y, &loss_grad);
      ASSERT_TRUE(loss >= 0.0f);
      ASSERT_TRUE(loss_grad != nullptr);

      ASSERT_EQ(neuron_model_backward(model, loss_grad), 0);
      neuron_optimizer_adam_step(model, &opt);
      neuron_model_zero_grad(model);

      neuron_tensor_free(loss_grad);
      neuron_tensor_free(pred);
      neuron_tensor_free(batch_x);
      neuron_tensor_free(batch_y);
    }
  }

  const float final_loss = evaluate_classification_loss(model, dataset);
  ASSERT_TRUE(final_loss >= 0.0f);
  ASSERT_TRUE(final_loss < initial_loss * 0.25f);
  ASSERT_EQ(evaluate_classification_accuracy(model, dataset),
            dataset->sample_count);

  neuron_optimizer_adam_free(&opt);
  neuron_dataloader_free(loader);
  neuron_model_free(model);
  neuron_dataset_free(dataset);
  return true;
}

TEST(ModelSaveLoadRoundTripsParameters) {
  const fs::path modelPath =
      fs::temp_directory_path() / "npp_nn_roundtrip_test.nppmodel";
  fs::remove(modelPath);

  NeuronSequentialModel *model = neuron_model_create();
  ASSERT_TRUE(model != nullptr);

  NeuronDenseLayer *hidden =
      neuron_layer_dense_create(2, 2, NEURON_ACTIVATION_LEAKY_RELU);
  ASSERT_TRUE(hidden != nullptr);
  hidden->activation_param = 0.2f;
  hidden->weights->data[0] = 0.5f;
  hidden->weights->data[1] = -0.25f;
  hidden->weights->data[2] = 0.75f;
  hidden->weights->data[3] = 0.1f;
  hidden->bias->data[0] = 0.3f;
  hidden->bias->data[1] = -0.4f;
  ASSERT_TRUE(neuron_model_add_layer(model, hidden));

  NeuronDenseLayer *output =
      neuron_layer_dense_create(2, 2, NEURON_ACTIVATION_SOFTMAX);
  ASSERT_TRUE(output != nullptr);
  output->weights->data[0] = 1.0f;
  output->weights->data[1] = -0.5f;
  output->weights->data[2] = 0.25f;
  output->weights->data[3] = 0.8f;
  output->bias->data[0] = 0.0f;
  output->bias->data[1] = 0.1f;
  ASSERT_TRUE(neuron_model_add_layer(model, output));

  int32_t input_shape[2] = {2, 2};
  NeuronTensor *input = neuron_tensor_create(2, input_shape);
  ASSERT_TRUE(input != nullptr);
  input->data[0] = 1.0f;
  input->data[1] = -2.0f;
  input->data[2] = -0.5f;
  input->data[3] = 0.25f;

  NeuronTensor *before = neuron_model_forward(model, input, 0);
  ASSERT_TRUE(before != nullptr);
  ASSERT_TRUE(neuron_model_save(model, modelPath.string().c_str()) == 1);

  NeuronSequentialModel *loaded =
      neuron_model_load(modelPath.string().c_str());
  ASSERT_TRUE(loaded != nullptr);
  ASSERT_EQ(loaded->count, 2);
  ASSERT_EQ(loaded->layers[0]->activation, NEURON_ACTIVATION_LEAKY_RELU);
  ASSERT_EQ(loaded->layers[1]->activation, NEURON_ACTIVATION_SOFTMAX);
  ASSERT_TRUE(approx_equal(loaded->layers[0]->activation_param, 0.2f, 1.0e-6f));

  NeuronTensor *after = neuron_model_forward(loaded, input, 0);
  ASSERT_TRUE(after != nullptr);
  ASSERT_EQ(before->size, after->size);
  for (int32_t i = 0; i < before->size; ++i) {
    ASSERT_TRUE(approx_equal(before->data[i], after->data[i], 1.0e-5f));
  }

  neuron_tensor_free(after);
  neuron_model_free(loaded);
  neuron_tensor_free(before);
  neuron_tensor_free(input);
  neuron_model_free(model);
  fs::remove(modelPath);
  return true;
}

TEST(InitStrategiesReinitializeDenseAndConvWeights) {
  NeuronDenseLayer *dense =
      neuron_layer_dense_create(8, 4, NEURON_ACTIVATION_RELU);
  ASSERT_TRUE(dense != nullptr);
  neuron_layer_dense_apply_init(dense, NEURON_INIT_XAVIER_UNIFORM);
  const float dense_limit = std::sqrt(6.0f / 12.0f);
  for (int32_t i = 0; i < dense->weights->size; ++i) {
    ASSERT_TRUE(std::fabs(dense->weights->data[i]) <= dense_limit + 1.0e-4f);
  }
  for (int32_t i = 0; i < dense->bias->size; ++i) {
    ASSERT_TRUE(approx_equal(dense->bias->data[i], 0.0f, 1.0e-6f));
  }

  NeuronConv2DLayer *conv = neuron_layer_conv2d_create(
      3, 5, 3, 3, 1, 1, 1, 1, NEURON_ACTIVATION_RELU);
  ASSERT_TRUE(conv != nullptr);
  neuron_layer_conv2d_apply_init(conv, NEURON_INIT_KAIMING_UNIFORM);
  const float conv_limit = std::sqrt(6.0f / 27.0f);
  for (int32_t i = 0; i < conv->weights->size; ++i) {
    ASSERT_TRUE(std::fabs(conv->weights->data[i]) <= conv_limit + 1.0e-4f);
  }
  for (int32_t i = 0; i < conv->bias->size; ++i) {
    ASSERT_TRUE(approx_equal(conv->bias->data[i], 0.0f, 1.0e-6f));
  }

  neuron_layer_conv2d_free(conv);
  neuron_layer_dense_free(dense);
  return true;
}

TEST(Conv2DForwardBackwardProducesExpectedValues) {
  NeuronConv2DLayer *conv = neuron_layer_conv2d_create(
      1, 1, 2, 2, 1, 1, 0, 0, NEURON_ACTIVATION_LINEAR);
  ASSERT_TRUE(conv != nullptr);
  for (int32_t i = 0; i < conv->weights->size; ++i) {
    conv->weights->data[i] = 0.0f;
  }
  conv->weights->data[0] = 1.0f;
  conv->weights->data[3] = 1.0f;
  conv->bias->data[0] = 0.0f;

  const float input_values[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  NeuronTensor *input = create_4d_tensor(1, 1, 3, 3, input_values);
  ASSERT_TRUE(input != nullptr);

  NeuronTensor *output = neuron_layer_conv2d_forward(conv, input, 1);
  ASSERT_TRUE(output != nullptr);
  ASSERT_EQ(output->shape[0], 1);
  ASSERT_EQ(output->shape[1], 1);
  ASSERT_EQ(output->shape[2], 2);
  ASSERT_EQ(output->shape[3], 2);
  ASSERT_TRUE(approx_equal(output->data[0], 6.0f, 1.0e-4f));
  ASSERT_TRUE(approx_equal(output->data[1], 8.0f, 1.0e-4f));
  ASSERT_TRUE(approx_equal(output->data[2], 12.0f, 1.0e-4f));
  ASSERT_TRUE(approx_equal(output->data[3], 14.0f, 1.0e-4f));

  const float grad_values[4] = {1, 1, 1, 1};
  NeuronTensor *grad_output = create_4d_tensor(1, 1, 2, 2, grad_values);
  ASSERT_TRUE(grad_output != nullptr);
  NeuronTensor *grad_input = neuron_layer_conv2d_backward(conv, grad_output);
  ASSERT_TRUE(grad_input != nullptr);
  ASSERT_TRUE(conv->grad_weights != nullptr);
  ASSERT_TRUE(conv->grad_bias != nullptr);
  ASSERT_TRUE(approx_equal(conv->grad_weights->data[0], 12.0f, 1.0e-4f));
  ASSERT_TRUE(approx_equal(conv->grad_weights->data[1], 16.0f, 1.0e-4f));
  ASSERT_TRUE(approx_equal(conv->grad_weights->data[2], 24.0f, 1.0e-4f));
  ASSERT_TRUE(approx_equal(conv->grad_weights->data[3], 28.0f, 1.0e-4f));
  ASSERT_TRUE(approx_equal(conv->grad_bias->data[0], 4.0f, 1.0e-4f));
  ASSERT_EQ(grad_input->shape[2], 3);
  ASSERT_EQ(grad_input->shape[3], 3);

  neuron_tensor_free(grad_input);
  neuron_tensor_free(grad_output);
  neuron_tensor_free(output);
  neuron_tensor_free(input);
  neuron_layer_conv2d_free(conv);
  return true;
}

TEST(MaxPool2DBackwardRoutesGradientToMaxValue) {
  NeuronMaxPool2DLayer *pool = neuron_layer_maxpool2d_create(2, 2, 2, 2);
  ASSERT_TRUE(pool != nullptr);

  const float input_values[4] = {1, 5, 3, 4};
  NeuronTensor *input = create_4d_tensor(1, 1, 2, 2, input_values);
  ASSERT_TRUE(input != nullptr);
  NeuronTensor *output = neuron_layer_maxpool2d_forward(pool, input, 1);
  ASSERT_TRUE(output != nullptr);
  ASSERT_EQ(output->size, 1);
  ASSERT_TRUE(approx_equal(output->data[0], 5.0f, 1.0e-4f));

  const float grad_values[1] = {7};
  NeuronTensor *grad_output = create_4d_tensor(1, 1, 1, 1, grad_values);
  ASSERT_TRUE(grad_output != nullptr);
  NeuronTensor *grad_input = neuron_layer_maxpool2d_backward(pool, grad_output);
  ASSERT_TRUE(grad_input != nullptr);
  ASSERT_TRUE(approx_equal(grad_input->data[0], 0.0f, 1.0e-6f));
  ASSERT_TRUE(approx_equal(grad_input->data[1], 7.0f, 1.0e-6f));
  ASSERT_TRUE(approx_equal(grad_input->data[2], 0.0f, 1.0e-6f));
  ASSERT_TRUE(approx_equal(grad_input->data[3], 0.0f, 1.0e-6f));

  neuron_tensor_free(grad_input);
  neuron_tensor_free(grad_output);
  neuron_tensor_free(output);
  neuron_tensor_free(input);
  neuron_layer_maxpool2d_free(pool);
  return true;
}

TEST(DropoutMasksTrainingButNotInference) {
  NeuronDropoutLayer *dropout = neuron_layer_dropout_create(0.5f, 42u);
  ASSERT_TRUE(dropout != nullptr);

  int32_t shape[2] = {1, 6};
  NeuronTensor *input = neuron_tensor_create(2, shape);
  ASSERT_TRUE(input != nullptr);
  for (int32_t i = 0; i < input->size; ++i) {
    input->data[i] = 1.0f;
  }

  NeuronTensor *train_out = neuron_layer_dropout_forward(dropout, input, 1);
  ASSERT_TRUE(train_out != nullptr);
  ASSERT_TRUE(dropout->mask != nullptr);

  int32_t kept = 0;
  int32_t dropped = 0;
  for (int32_t i = 0; i < train_out->size; ++i) {
    if (approx_equal(train_out->data[i], 0.0f, 1.0e-6f)) {
      dropped++;
    } else {
      kept++;
      ASSERT_TRUE(approx_equal(train_out->data[i], 2.0f, 1.0e-6f));
    }
  }
  ASSERT_TRUE(kept > 0);
  ASSERT_TRUE(dropped > 0);

  NeuronTensor *grad = neuron_tensor_create(2, shape);
  ASSERT_TRUE(grad != nullptr);
  for (int32_t i = 0; i < grad->size; ++i) {
    grad->data[i] = 1.0f;
  }
  NeuronTensor *grad_input = neuron_layer_dropout_backward(dropout, grad);
  ASSERT_TRUE(grad_input != nullptr);
  for (int32_t i = 0; i < grad_input->size; ++i) {
    ASSERT_TRUE(approx_equal(grad_input->data[i], dropout->mask->data[i],
                             1.0e-6f));
  }

  NeuronTensor *eval_out = neuron_layer_dropout_forward(dropout, input, 0);
  ASSERT_TRUE(eval_out != nullptr);
  for (int32_t i = 0; i < eval_out->size; ++i) {
    ASSERT_TRUE(approx_equal(eval_out->data[i], 1.0f, 1.0e-6f));
  }

  neuron_tensor_free(eval_out);
  neuron_tensor_free(grad_input);
  neuron_tensor_free(grad);
  neuron_tensor_free(train_out);
  neuron_tensor_free(input);
  neuron_layer_dropout_free(dropout);
  return true;
}

TEST(ModelSupportsConvPoolDropoutPipeline) {
  NeuronSequentialModel *model = neuron_model_create();
  ASSERT_TRUE(model != nullptr);

  NeuronConv2DLayer *conv = neuron_layer_conv2d_create(
      1, 1, 2, 2, 1, 1, 0, 0, NEURON_ACTIVATION_RELU);
  ASSERT_TRUE(conv != nullptr);
  for (int32_t i = 0; i < conv->weights->size; ++i) {
    conv->weights->data[i] = 0.25f;
  }
  conv->bias->data[0] = 0.0f;

  NeuronMaxPool2DLayer *pool = neuron_layer_maxpool2d_create(2, 2, 2, 2);
  ASSERT_TRUE(pool != nullptr);
  NeuronDropoutLayer *dropout = neuron_layer_dropout_create(0.25f, 7u);
  ASSERT_TRUE(dropout != nullptr);

  ASSERT_TRUE(neuron_model_add_conv2d_layer(model, conv));
  ASSERT_TRUE(neuron_model_add_maxpool2d_layer(model, pool));
  ASSERT_TRUE(neuron_model_add_dropout_layer(model, dropout));

  const float input_values[16] = {
      1, 2, 3, 4, 5, 6, 7, 8,
      9, 10, 11, 12, 13, 14, 15, 16};
  NeuronTensor *input = create_4d_tensor(1, 1, 4, 4, input_values);
  ASSERT_TRUE(input != nullptr);

  NeuronTensor *output = neuron_model_forward(model, input, 1);
  ASSERT_TRUE(output != nullptr);
  ASSERT_EQ(output->shape[0], 1);
  ASSERT_EQ(output->shape[1], 1);
  ASSERT_EQ(output->shape[2], 1);
  ASSERT_EQ(output->shape[3], 1);

  const float grad_values[1] = {1.0f};
  NeuronTensor *grad = create_4d_tensor(1, 1, 1, 1, grad_values);
  ASSERT_TRUE(grad != nullptr);
  ASSERT_EQ(neuron_model_backward(model, grad), 0);
  ASSERT_TRUE(conv->grad_weights != nullptr);
  ASSERT_TRUE(conv->grad_bias != nullptr);

  neuron_tensor_free(grad);
  neuron_tensor_free(output);
  neuron_tensor_free(input);
  neuron_model_free(model);
  return true;
}

TEST(ModelSaveLoadRoundTripsCompositePhase2Model) {
  const fs::path modelPath =
      fs::temp_directory_path() / "npp_nn_phase2_roundtrip.nppmodel";
  fs::remove(modelPath);

  NeuronSequentialModel *model = neuron_model_create();
  ASSERT_TRUE(model != nullptr);
  NeuronConv2DLayer *conv = neuron_layer_conv2d_create(
      1, 1, 2, 2, 1, 1, 0, 0, NEURON_ACTIVATION_LINEAR);
  NeuronMaxPool2DLayer *pool = neuron_layer_maxpool2d_create(2, 2, 2, 2);
  NeuronDropoutLayer *dropout = neuron_layer_dropout_create(0.2f, 11u);
  ASSERT_TRUE(conv != nullptr && pool != nullptr && dropout != nullptr);
  conv->weights->data[0] = 1.0f;
  conv->weights->data[1] = 0.0f;
  conv->weights->data[2] = 0.0f;
  conv->weights->data[3] = 1.0f;
  conv->bias->data[0] = 0.5f;
  ASSERT_TRUE(neuron_model_add_conv2d_layer(model, conv));
  ASSERT_TRUE(neuron_model_add_maxpool2d_layer(model, pool));
  ASSERT_TRUE(neuron_model_add_dropout_layer(model, dropout));

  const float input_values[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  NeuronTensor *input = create_4d_tensor(1, 1, 3, 3, input_values);
  ASSERT_TRUE(input != nullptr);

  NeuronTensor *before = neuron_model_forward(model, input, 0);
  ASSERT_TRUE(before != nullptr);
  ASSERT_TRUE(neuron_model_save(model, modelPath.string().c_str()) == 1);

  NeuronSequentialModel *loaded =
      neuron_model_load(modelPath.string().c_str());
  ASSERT_TRUE(loaded != nullptr);
  ASSERT_EQ(loaded->count, 3);

  NeuronTensor *after = neuron_model_forward(loaded, input, 0);
  ASSERT_TRUE(after != nullptr);
  ASSERT_EQ(before->size, after->size);
  for (int32_t i = 0; i < before->size; ++i) {
    ASSERT_TRUE(approx_equal(before->data[i], after->data[i], 1.0e-5f));
  }

  neuron_tensor_free(after);
  neuron_model_free(loaded);
  neuron_tensor_free(before);
  neuron_tensor_free(input);
  neuron_model_free(model);
  fs::remove(modelPath);
  return true;
}

TEST(GpuScopePhase2ModelPathStillRuns) {
  NeuronSequentialModel *model = neuron_model_create();
  ASSERT_TRUE(model != nullptr);
  NeuronDenseLayer *dense =
      neuron_layer_dense_create(2, 2, NEURON_ACTIVATION_RELU);
  ASSERT_TRUE(dense != nullptr);
  ASSERT_TRUE(neuron_model_add_layer(model, dense));

  int32_t shape[2] = {2, 2};
  NeuronTensor *input = neuron_tensor_create(2, shape);
  ASSERT_TRUE(input != nullptr);
  input->data[0] = 1.0f;
  input->data[1] = -1.0f;
  input->data[2] = 0.5f;
  input->data[3] = 2.0f;

  NeuronTensor *grad = neuron_tensor_create(2, shape);
  ASSERT_TRUE(grad != nullptr);
  for (int32_t i = 0; i < grad->size; ++i) {
    grad->data[i] = 1.0f;
  }

  ASSERT_EQ(neuron_gpu_scope_begin(), 0);
  NeuronTensor *output = neuron_model_forward(model, input, 1);
  ASSERT_TRUE(output != nullptr);
  ASSERT_EQ(neuron_model_backward(model, grad), 0);
  ASSERT_EQ(neuron_gpu_scope_end(), 0);
  ASSERT_TRUE(dense->grad_weights != nullptr);

  neuron_tensor_free(output);
  neuron_tensor_free(grad);
  neuron_tensor_free(input);
  neuron_model_free(model);
  return true;
}
