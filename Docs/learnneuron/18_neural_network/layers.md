# Layers

Neuron provides built-in neural network layers through the NN module.

---

## Available Layers

| Layer | Description | Creation |
|-------|-------------|----------|
| `NN.Linear(in, out)` | Fully-connected layer | `NN.Linear(784, 128)` |
| `NN.Conv2D(in, out, kernel)` | 2D convolution | `NN.Conv2D(3, 16, 3)` |
| `NN.BatchNorm(features)` | Batch normalization | `NN.BatchNorm(128)` |
| `NN.Dropout(rate)` | Dropout regularization | `NN.Dropout(0.5)` |

---

## Layer Usage

```npp
layer is NN.Linear(784, 128);
output is layer.Forward(input);
```

---

## Activation Functions

| Function | Usage |
|----------|-------|
| `NN.ReLU(x)` | Rectified Linear Unit |
| `NN.Sigmoid(x)` | Sigmoid activation |
| `NN.Tanh(x)` | Hyperbolic tangent |
| `NN.Softmax(x)` | Softmax normalization |

---

## Building a Network

```npp
MyNetwork class {
    conv1 is NN.Conv2D(1, 32, 3);
    bn1 is NN.BatchNorm(32);
    fc1 is NN.Linear(32 * 26 * 26, 10);
    drop1 is NN.Dropout(0.25);

    Forward method(x as Tensor<float>) as Tensor<float> {
        x is NN.ReLU(bn1.Forward(conv1.Forward(x)));
        x is drop1.Forward(x);
        return NN.Softmax(fc1.Forward(x));
    }
}
```

---

## Next Steps

- [Training](training.md) â€” Training loop and optimization
