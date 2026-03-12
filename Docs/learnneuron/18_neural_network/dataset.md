# Dataset & DataLoader

Load and process data for neural network training.

---

## Dataset Pipeline

```npp
module NN;

data is NN.LoadCSV("train.csv");
loader is NN.DataLoader(data, batchSize: 32, shuffle: true);

for (batch in loader) {
    predictions is model.Forward(batch.input);
    loss is NN.CrossEntropy(predictions, batch.labels);
    loss.Backward();
    optimizer.Step();
}
```

---

## Data Loading

| Method | Description |
|--------|-------------|
| `NN.LoadCSV(path)` | Load data from CSV file |
| `NN.LoadTensor(path)` | Load serialized tensor data |

---

## DataLoader

The DataLoader handles:
- **Batching** — Splits data into mini-batches
- **Shuffling** — Randomize order each epoch
- **Iteration** — Works with `for-in` loops

---

## Next Steps

- [GPU Training](gpu_training.md) — Accelerate training on GPU
