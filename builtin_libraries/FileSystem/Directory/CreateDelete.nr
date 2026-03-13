module FileSystemNative;

Create is method(path as string) as bool {
  return FileSystemNative.DirectoryCreate(path);
}

Delete is method(path as string) as bool {
  return FileSystemNative.DirectoryDelete(path);
}
