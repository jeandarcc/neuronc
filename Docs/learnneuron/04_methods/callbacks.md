# Callbacks

A callback is a method passed as an argument to another method. In Neuron, use the `method` type for callback parameters.

---

## Declaring a Callback Parameter

```npp
Execute method(action as method) {
    action();
}
```

The parameter `action` has type `method` â€” it accepts any callable.

---

## Calling with a Named Method

```npp
SayHello method() {
    Print("Hello!");
}

Execute(SayHello);   // prints "Hello!"
```

---

## Calling with a Lambda

```npp
Execute(method() {
    Print("Lambda executed!");
});
```

---

## Callback with Return Value

```npp
Compute method(op as method) as int {
    return op();
}

result is Compute(method() as int {
    return 42;
});

Print(result);   // 42
```

---

## Pattern: Event Handlers

```npp
OnClick method(handler as method) {
    // ... when click detected:
    handler();
}

OnClick(method() {
    Print("Button clicked!");
});
```

---

## Next Steps

- [Async/Await](async_await.md) â€” Asynchronous methods
