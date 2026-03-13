module System;
module Tensor;
module Time;

RunFmaBench is method() as int {
    a is Tensor<float>.Random(1024, 1024);
    b is Tensor<float>.Random(1024, 1024);
    c is Tensor<float>.Random(1024, 1024);

    start is Time.Now();
    mix0 is a * b + c; out0 is mix0 @ b;
    mix1 is a * b + c; out1 is mix1 @ b;
    mix2 is a * b + c; out2 is mix2 @ b;
    mix3 is a * b + c; out3 is mix3 @ b;
    mix4 is a * b + c; out4 is mix4 @ b;
    mix5 is a * b + c; out5 is mix5 @ b;
    mix6 is a * b + c; out6 is mix6 @ b;
    mix7 is a * b + c; out7 is mix7 @ b;
    finish is Time.Now();
    return (finish - start) / 8;
}

RunMatMulBench is method() as int {
    a is Tensor<float>.Random(1024, 1024);
    b is Tensor<float>.Random(1024, 1024);

    start is Time.Now();
    out0 is a @ b;
    out1 is a @ b;
    out2 is a @ b;
    out3 is a @ b;
    out4 is a @ b;
    out5 is a @ b;
    out6 is a @ b;
    out7 is a @ b;
    finish is Time.Now();
    return (finish - start) / 8;
}

Init is method() {
    fmaMs is RunFmaBench();
    matmulMs is RunMatMulBench();

    Print("Neuron_FMA_MS");
    Print(fmaMs);
    Print("Neuron_MATMUL_MS");
    Print(matmulMs);
}
