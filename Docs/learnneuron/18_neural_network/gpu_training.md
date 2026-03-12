# GPU Training

Accelerate neural network training using GPU blocks.

---

## GPU-Accelerated Training Loop

```npp
module NN;

Init method() {
    model is Classifier();
    optimizer is NN.Adam(model, 0.001);

    gpu {
        for (epoch is 0; epoch < 100; epoch++) {
            predictions is model.Forward(trainData);
            loss is NN.CrossEntropy(predictions, labels);
            loss.Backward();
            optimizer.Step();
        }
    }
}
```

---

## Model Save / Load

```npp
// Save trained model
NN.Save(model, "model.bin");

// Load saved model
loaded is NN.Load("model.bin");
```

---

## Mixed Precision

For faster training on supported GPUs:

```npp
gpu {
    // Float16 operations where possible
    predictions is model.Forward(data as Tensor<half>);
}
```

---

## Best Practices

1. Keep data on GPU — avoid unnecessary transfers
2. Use batch sizes that are powers of 2 (32, 64, 128, 256)
3. Monitor loss with periodic CPU readback
4. Save checkpoints regularly

---

## Next Steps

- [Toolchain](../19_toolchain/cli_reference.md) — CLI reference
