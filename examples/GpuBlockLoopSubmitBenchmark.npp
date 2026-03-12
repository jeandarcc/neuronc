module Time;

Init method() {
  a is Tensor<float>.Random(1, 65536);
  b is Tensor<float>.Random(1, 65536);
  c is Tensor<float>.Random(1, 65536);
  d is Tensor<float>.Random(1, 65536);

  loops is 12 as int;
  startPerIter is Time.Now();
  for(i is 0 as int; i < loops; i++) {
    gpu {
      x is a + b;
      y is x * c;
      perIterSink is (y * a) + d;
    }
  }
  endPerIter is Time.Now();
  perIterMs is endPerIter - startPerIter;

  startOuter is Time.Now();
  gpu {
    for(j is 0 as int; j < loops; j++) {
      gx is a + b;
      gy is gx * c;
      outerSink is (gy * a) + d;
    }
  }
  endOuter is Time.Now();
  outerMs is endOuter - startOuter;

  Print("loops=12");
  Print(perIterMs);
  Print(outerMs);

  loops is 120 as int;
  startPerIter120 is Time.Now();
  for(i120 is 0 as int; i120 < loops; i120++) {
    gpu {
      x120 is a + b;
      y120 is x120 * c;
      perIterSink120 is (y120 * a) + d;
    }
  }
  endPerIter120 is Time.Now();
  perIterMs120 is endPerIter120 - startPerIter120;

  startOuter120 is Time.Now();
  gpu {
    for(j120 is 0 as int; j120 < loops; j120++) {
      gx120 is a + b;
      gy120 is gx120 * c;
      outerSink120 is (gy120 * a) + d;
    }
  }
  endOuter120 is Time.Now();
  outerMs120 is endOuter120 - startOuter120;

  Print("loops=120");
  Print(perIterMs120);
  Print(outerMs120);

  loops is 1200 as int;
  startPerIter1200 is Time.Now();
  for(i1200 is 0 as int; i1200 < loops; i1200++) {
    gpu {
      x1200 is a + b;
      y1200 is x1200 * c;
      perIterSink1200 is (y1200 * a) + d;
    }
  }
  endPerIter1200 is Time.Now();
  perIterMs1200 is endPerIter1200 - startPerIter1200;

  startOuter1200 is Time.Now();
  gpu {
    for(j1200 is 0 as int; j1200 < loops; j1200++) {
      gx1200 is a + b;
      gy1200 is gx1200 * c;
      outerSink1200 is (gy1200 * a) + d;
    }
  }
  endOuter1200 is Time.Now();
  outerMs1200 is endOuter1200 - startOuter1200;

  Print("loops=1200");
  Print(perIterMs1200);
  Print(outerMs1200);
}
