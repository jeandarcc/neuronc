# Benchmark Suite (`benchmarks/`)

This directory contains the automated performance tracking suite for the Neuron
compiler, specifically targeting tensor ops and AI workloads compared against
equivalent pure C++ or Python implementations.

## Files & Directories

| File/Subdirectory | Purpose |
|-------------------|---------|
| `run_ai_tensor_benchmarks.ps1` | The main powershell runner. Executes the `.nr` scripts, parses outputs, and orchestrates the suite. |
| `perf_regression_gate.ps1` | CI script that analyzes the output of the benchmarks to block PRs that degrade tensor performance beyond a set threshold. |
| `AiTensorBench.nr`, `AiFusedBench.nr` | Source code for the specific algorithms acting as the Neuron benchmark payload. |
| `cpp/`, `python/` | The baseline implementations in standard languages used as the control for relative benchmark scaling. |
| `results/` | Output directory where parsed CSV/JSON data from the latest benchmark run is deposited. |
| `bin/` | Scratch directory for compiled benchmark binaries. |

## Running Benchmarks

```powershell
powershell -File benchmarks/run_ai_tensor_benchmarks.ps1
```

> Note: Benchmarks require a fully built, Release-mode compiler. Run `scripts\build.bat` in a clean environment before benchmarking to ensure consistent numbers.
