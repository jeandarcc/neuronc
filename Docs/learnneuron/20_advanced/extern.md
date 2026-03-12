# Extern (FFI)

The `extern` keyword declares functions defined in external C/C++ libraries.

---

## Syntax

```npp
extern method Add(a as float, b as float) as float;
```

This declares `Add` as an external function with the given signature. The implementation is provided by a linked C/C++ library.

---

## Usage

```npp
extern method NativeCompute(x as float) as float;

Init method() {
    result is NativeCompute(3.14);
    Print(result);
}
```

---

## Linking

External functions are resolved at link time. Configure the linker path in `neuron.toml`:

```toml
[build]
extra_link_dirs = ["lib/"]
extra_link_libs = ["mylib"]
```

---

## Restrictions

- `extern` functions are **blocked** in NCON sandbox mode
- No body is provided — the function must exist in a linked library

---

## Next Steps

- [Metaprogramming](metaprogramming.md) — macro, typeof, static_assert
