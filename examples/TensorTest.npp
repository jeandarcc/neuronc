module System;
module Tensor;

CreateTensor is method() as Tensor<float> {
}

TensorDemo is method(Tensor<float> t1, Tensor<float> t2) {
    t3 is t1 + t2;
    t4 is t1 - t2;
    t5 is t1 * t2;
    t6 is t1 / t2;
    t7 is t1 @ t2;
    Print(t3);
    Print(t4);
    Print(t5);
    Print(t6);
    Print(t7);
}

Init is method() {
    Print("Running Tensor Mock Runtime:");
    t1 is CreateTensor();
    t2 is CreateTensor();
    TensorDemo(t1, t2);
    Print("Done.");
}
