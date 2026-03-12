module FileSystemNative;

Exists is method(path as string) as bool {
  return FileSystemNative.FileExists(path);
}

Delete is method(path as string) as bool {
  return FileSystemNative.FileDelete(path);
}

Copy is method(source as string, destination as string) as bool {
  return FileSystemNative.FileCopy(source, destination);
}

Move is method(source as string, destination as string) as bool {
  return FileSystemNative.FileMove(source, destination);
}
