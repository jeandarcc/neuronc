NormalizeSlashes is method(path as string) as string {
  return path;
}

Normalize is method(path as string) as string {
  return NormalizeSlashes(path);
}

