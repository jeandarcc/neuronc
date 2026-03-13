module FileSystemNative;

ReadAllText is method(path as string) as string {
  return FileSystemNative.FileReadAllText(path);
}

WriteAllText is method(path as string, content as string) as bool {
  return FileSystemNative.FileWriteAllText(path, content);
}

AppendAllText is method(path as string, content as string) as bool {
  return FileSystemNative.FileAppendAllText(path, content);
}
