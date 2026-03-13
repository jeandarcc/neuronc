Join is method(left as string, right as string) as string {
  return left + Separator() + right;
}

Join3 is method(a as string, b as string, c as string) as string {
  return Join(Join(a, b), c);
}

EnsureTrailingSeparator is method(path as string) as string {
  return path + Separator();
}

