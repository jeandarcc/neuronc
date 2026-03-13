module FileSystemNative;

Start is method(path as string) as int {
  return FileSystemNative.WatchStart(path);
}

Poll is method(handle as int) as string {
  return FileSystemNative.WatchPoll(handle);
}

Stop is method(handle as int) as bool {
  return FileSystemNative.WatchStop(handle);
}
