#!/usr/bin/env python3
import argparse
import os
import sys
import time

os.environ.setdefault("CUDA_VISIBLE_DEVICES", "-1")
os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "2")

try:
    import tensorflow as tf
except Exception as exc:  # pragma: no cover - runtime availability check
    print(f"TensorFlow import failed: {exc}", file=sys.stderr)
    sys.exit(2)


def _materialize(tensor: tf.Tensor) -> None:
    _ = float(tf.reduce_sum(tensor).numpy())


def _avg_ms(fn, warmup: int, repeats: int) -> float:
    for _ in range(warmup):
        _materialize(fn())

    start = time.perf_counter()
    out = None
    for _ in range(repeats):
        out = fn()
    _materialize(out)
    end = time.perf_counter()
    return ((end - start) * 1000.0) / float(repeats)


def run_pure(size: int, warmup: int, repeats: int) -> tuple[float, float]:
    a = tf.random.uniform((size, size), dtype=tf.float32, seed=1337)
    b = tf.random.uniform((size, size), dtype=tf.float32, seed=4242)
    c = tf.random.uniform((size, size), dtype=tf.float32, seed=9001)

    @tf.function(jit_compile=False)
    def fma_step():
        mix = a * b + c
        return tf.matmul(mix, b)

    @tf.function(jit_compile=False)
    def matmul_step():
        return tf.matmul(a, b)

    fma_ms = _avg_ms(fma_step, warmup, repeats)
    matmul_ms = _avg_ms(matmul_step, warmup, repeats)
    return fma_ms, matmul_ms


def run_fused(size: int, warmup: int, repeats: int) -> float:
    a = tf.random.uniform((size, size), dtype=tf.float32, seed=1337)
    b = tf.random.uniform((size, size), dtype=tf.float32, seed=4242)
    bias = tf.random.uniform((1, size), dtype=tf.float32, seed=777)
    residual = tf.random.uniform((size, size), dtype=tf.float32, seed=888)

    @tf.function(jit_compile=False)
    def fused_step():
        return tf.matmul(a, b) + bias + residual

    return _avg_ms(fused_step, warmup, repeats)


def main() -> int:
    parser = argparse.ArgumentParser(description="TensorFlow CPU tensor benchmark")
    parser.add_argument("--mode", choices=("pure", "fused"), required=True)
    parser.add_argument("--size", type=int, default=1024)
    parser.add_argument("--threads", type=int, default=1)
    parser.add_argument("--inter-op-threads", type=int, default=1)
    parser.add_argument("--warmup", type=int, default=1)
    parser.add_argument("--repeats", type=int, default=8)
    args = parser.parse_args()

    tf.random.set_seed(1234)
    tf.config.threading.set_intra_op_parallelism_threads(max(args.threads, 1))
    tf.config.threading.set_inter_op_parallelism_threads(max(args.inter_op_threads, 1))

    if args.mode == "pure":
        fma_ms, matmul_ms = run_pure(args.size, args.warmup, args.repeats)
        print("TF_FMA_MS")
        print(f"{fma_ms:.6f}")
        print("TF_MATMUL_MS")
        print(f"{matmul_ms:.6f}")
    else:
        fused_ms = run_fused(args.size, args.warmup, args.repeats)
        print("TF_FUSED_MS")
        print(f"{fused_ms:.6f}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
