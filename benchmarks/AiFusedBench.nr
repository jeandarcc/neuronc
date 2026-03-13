module Tensor;
module Time;
module System;

RunFusedBench is method() as int {
    a is Tensor<float>.Random(1024, 1024);
    b is Tensor<float>.Random(1024, 1024);
    bias is Tensor<float>.Random(1, 1024);
    residual is Tensor<float>.Random(1024, 1024);

    start is Time.Now();
    out0 is a @ b + bias + residual;
    out1 is a @ b + bias + residual;
    out2 is a @ b + bias + residual;
    out3 is a @ b + bias + residual;
    out4 is a @ b + bias + residual;
    out5 is a @ b + bias + residual;
    out6 is a @ b + bias + residual;
    out7 is a @ b + bias + residual;
    finish is Time.Now();
    return (finish - start) / 8;
}

Init is method() {
    fusedMs is RunFusedBench();
    Print("Neuron_FUSED_MS");
    Print(fusedMs);
}
