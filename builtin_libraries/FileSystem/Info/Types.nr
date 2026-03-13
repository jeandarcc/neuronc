PathInfo is class {
  FullPath as string;
  Exists as bool;
  IsFile as bool;
  IsDirectory as bool;
}

FileInfo is class {
  FullPath as string;
  Exists as bool;
  Size as int;
}

DirectoryInfo is class {
  FullPath as string;
  Exists as bool;
}

