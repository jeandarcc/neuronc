# Neural Network Model

Build neural networks using Neuron's built-in NN module.

---

## Model Structure

A neural network model is a class with a `Forward` method:

```npp
module NN;

Classifier class {
    layer1 is NN.Linear(784, 128);
    layer2 is NN.Linear(128, 10);

    Forward method(input as Tensor<float>) as Tensor<float> {
        x is NN.ReLU(layer1.Forward(input));
        return NN.Softmax(layer2.Forward(x));
    }
}
```

---

## Creating a Model

```npp
model is Classifier();
output is model.Forward(inputData);
```

---

## Module Import

```npp
module NN;
```

This provides access to layers, optimizers, activation functions, and the training API.

---

## Self-Test

Verify the NN runtime is working:

```npp
module NN;

Init method() {
    result is NN.SelfTest();
    if (result == 1) {
        Print("NN self-test passed.");
    }
}
```

---

## Next Steps

- [Layers](layers.md) â€” Available layer types
- [Training](training.md) â€” Training loop
