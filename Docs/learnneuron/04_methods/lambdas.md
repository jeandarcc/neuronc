# Lambdas (Anonymous Methods)

Lambdas are unnamed methods declared inline, typically passed as arguments to other methods.

---

## Syntax

```npp
method() {
    Print("I'm a lambda!");
}
```

A lambda is simply a `method` without a name.

---

## Passing as Argument

```npp
Execute method(action as method) {
    action();
}

// Call with a lambda
Execute(method() {
    Print("Executed!");
});
```

---

## With Parameters

```npp
Apply method(transform as method, value as int) as int {
    return transform(value);
}

result is Apply(method(x as int) as int {
    return x * 2;
}, 5);

Print(result);   // 10
```

---

## Assigning to a Variable

You can store a lambda in a variable using `is`:

```npp
doubler is method(x as int) as int {
    return x * 2;
};

Print(doubler(5));   // 10
```

---

## Next Steps

- [Callbacks](callbacks.md) — Using methods as parameter types
- [Async/Await](async_await.md) — Asynchronous methods
