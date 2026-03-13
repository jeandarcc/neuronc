/// File existence check
extern FileExists method(path as string) as bool;

/// File read operation
extern ReadFile method(path as string) as string;

/// File write operation
extern WriteFile method(path as string, content as string) as bool;

/// File deletion
extern DeleteFile method(path as string) as bool;

/// Directory existence check
extern DirectoryExists method(path as string) as bool;

/// Create directory
extern CreateDirectory method(path as string) as bool;

/// List directory contents
extern ListDirectory method(path as string) as Array<string>;

/// Get absolute path
extern GetAbsolutePath method(path as string) as string;

/// Get file size
extern GetFileSize method(path as string) as int;


