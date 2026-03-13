# Async / Await

Neuron supports asynchronous programming with the `async` and `await` keywords.

---

## Async Methods

Prefix a method declaration with `async` to make it asynchronous:

```npp
LoadData async method() as Tensor<float> {
    return ReadDataset();
}
```

An `async` method runs concurrently and returns a future/promise.

---

## Awaiting Results

Use `await` to wait for an async method to complete and get its result:

```npp
Init method() {
    result is await LoadData();
    Print(result);
}
```

---

## Why Async?

Async methods allow the program to perform other work while waiting for long-running operations:

- File I/O
- Network requests
- Dataset loading
- GPU computation

---

## Example: Parallel Data Loading

```npp
LoadTraining async method() as Tensor<float> {
    return ReadDataset("train.csv");
}

LoadValidation async method() as Tensor<float> {
    return ReadDataset("val.csv");
}

Init method() {
    trainData is await LoadTraining();
    valData is await LoadValidation();
    Print("Data loaded!");
}
```

---

## Next Steps

- [Classes](../05_classes/declaration.md) — Object-oriented programming
