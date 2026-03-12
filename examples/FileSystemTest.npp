module FileSystem;

Init method() {
  /// Test FileSystem native provider contract resolution
  testPath is "test.txt" as string;
  
  /// These should resolve to FileSystemNative exports
  exists is FileExists(testPath);
  
  if exists {
    content is ReadFile(testPath);
    Print(content);
  };
  
  /// Create directory test
  dirCreated is CreateDirectory("test_dir");
  
  /// Get absolute path
  absPath is GetAbsolutePath(".");
  Print(absPath);
  
  return;
}

