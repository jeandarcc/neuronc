module Time;

Init method() {
  loops is 12 as int;

  a64k is Tensor<float>.Random(1, 65536);
  b64k is Tensor<float>.Random(1, 65536);
  c64k is Tensor<float>.Random(1, 65536);
  d64k is Tensor<float>.Random(1, 65536);

  start64k is Time.Now();
  gpu {
    for(i64k is 0 as int; i64k < loops; i64k++) {
      x64k is a64k + b64k;
      y64k is x64k * c64k;
      sink64k is (y64k * a64k) + d64k;
    }
  }
  end64k is Time.Now();
  ms64k is end64k - start64k;

  a256k is Tensor<float>.Random(1, 262144);
  b256k is Tensor<float>.Random(1, 262144);
  c256k is Tensor<float>.Random(1, 262144);
  d256k is Tensor<float>.Random(1, 262144);

  start256k is Time.Now();
  gpu {
    for(i256k is 0 as int; i256k < loops; i256k++) {
      x256k is a256k + b256k;
      y256k is x256k * c256k;
      sink256k is (y256k * a256k) + d256k;
    }
  }
  end256k is Time.Now();
  ms256k is end256k - start256k;

  a1m is Tensor<float>.Random(1, 1048576);
  b1m is Tensor<float>.Random(1, 1048576);
  c1m is Tensor<float>.Random(1, 1048576);
  d1m is Tensor<float>.Random(1, 1048576);

  start1m is Time.Now();
  gpu {
    for(i1m is 0 as int; i1m < loops; i1m++) {
      x1m is a1m + b1m;
      y1m is x1m * c1m;
      sink1m is (y1m * a1m) + d1m;
    }
  }
  end1m is Time.Now();
  ms1m is end1m - start1m;

  Print("outer_gpu_for_size_ms");
  Print(ms64k);
  Print(ms256k);
  Print(ms1m);
}
