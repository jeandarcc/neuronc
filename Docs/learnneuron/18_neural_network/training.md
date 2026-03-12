# Training

Train neural networks with forward pass, backward pass, and optimizer step.

---

## Training Loop Pattern

```npp
module NN;
module Tensor;

Train method(model as Classifier, data as Tensor<float>, labels as Tensor<float>) {
    optimizer is NN.Adam(model, 0.001);

    for (epoch is 0; epoch < 100; epoch++) {
        // Forward pass
        predictions is model.Forward(data);

        // Compute loss
        loss is NN.CrossEntropy(predictions, labels);

        // Backward pass (compute gradients)
        loss.Backward();

        // Update weights
        optimizer.Step();

        Print("Epoch " + epoch + " Loss: " + loss);
    }
}
```

---

## Steps Explained

1. **Forward** — Pass data through the model
2. **Loss** — Compute how wrong the predictions are
3. **Backward** — Compute gradients via automatic differentiation
4. **Step** — Update model weights using the optimizer

---

## Optimizers

| Optimizer | Usage |
|-----------|-------|
| `NN.SGD(model, lr)` | Stochastic Gradient Descent |
| `NN.Adam(model, lr)` | Adam optimizer |
| `NN.RMSProp(model, lr)` | RMSProp optimizer |

---

## Loss Functions

| Function | Usage |
|----------|-------|
| `NN.CrossEntropy(pred, label)` | Classification |
| `NN.MSE(pred, target)` | Regression |

---

## Next Steps

- [Dataset](dataset.md) — Loading data
- [GPU Training](gpu_training.md) — Training on GPU
