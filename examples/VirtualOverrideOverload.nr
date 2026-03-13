// virtual / override / overload syntax demo (single class)
VirtualOverrideOverload class {
  overload Add method(a as int, b as int) as int {
    return a + b;
  }

  virtual Negate method(a as int) as int {
    return 0 - a;
  }

  // Syntax demonstration: parser + semantic flags
  override Normalize method(a as int) as int {
    return a;
  }
}

Init method() as int {
  return 0;
}
