# ModuleCpp (C++ Interop)

The `modulecpp` keyword allows importing native C++ libraries into Neuron++.

---

## Syntax

```npp
modulecpp NativeLib;
```

---

## How It Works

1. Define a C++ module with a manifest in your project
2. Configure it in `neuron.toml` under `[ncon.native.modules]`
3. Build the native code with CMake
4. Import it with `modulecpp`

---

## Configuration in `neuron.toml`

```toml
[ncon.native.modules.mylib]
manifest_path = "native/mylib/manifest.toml"
source_dir = "native/mylib"
build_system = "cmake"
cmake_target = "mylib"
```

---

## Usage

```npp
modulecpp MyNativeLib;

Init method() {
    result is MyNativeLib.Compute(42);
    Print(result);
}
```

---

## Next Steps

- [Standard Library](standard_library.md)
