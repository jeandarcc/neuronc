// "is" optional + dynamic shorthand demo

RunFmaBench method() as int {
  a;
  b 10;
  c as dynamic;
  d is 0 as dynamic;

  Print("is-optional + dynamic demo");
  return 0;
}

Init method() as int {
  return RunFmaBench();
}
