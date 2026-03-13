#define AppName "Neuron"
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef SourceDir
  #define SourceDir "stage"
#endif
#ifndef OutputDir
  #define OutputDir "..\\..\\build\\installer\\out"
#endif

[Setup]
AppId={{A312B684-8A5E-4F3D-80E4-061E9BA6C9F6}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Neuron
DefaultDirName={autopf}\Neuron
DefaultGroupName=Neuron
DisableProgramGroupPage=yes
UsePreviousAppDir=yes
UsePreviousTasks=yes
OutputDir={#OutputDir}
OutputBaseFilename=Neuron-{#AppVersion}-windows-x64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#ifndef CompressionLevel
  #define CompressionLevel "lzma2/ultra64"
#endif
#ifndef SolidLevel
  #define SolidLevel "yes"
#endif
Compression={#CompressionLevel}
SolidCompression={#SolidLevel}
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
ChangesEnvironment=yes
ChangesAssociations=yes
WizardStyle=modern
UninstallDisplayIcon={app}\bin\neuron.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "addtopath"; Description: "Add neuron and ncon commands to PATH"

[Files]
Source: "{#SourceDir}\bin\*"; DestDir: "{app}\bin"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\runtime\*"; DestDir: "{app}\runtime"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\include\*"; DestDir: "{app}\include"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\src\*"; DestDir: "{app}\src"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\toolchain\*"; DestDir: "{app}\toolchain"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\Neuron CLI"; Filename: "{app}\bin\neuron.exe"
Name: "{group}\NCON CLI"; Filename: "{app}\bin\ncon.exe"
Name: "{group}\Uninstall Neuron"; Filename: "{uninstallexe}"

[Registry]
Root: HKCR; Subkey: ".ncon"; ValueType: string; ValueName: ""; ValueData: "Neuron.Ncon"; Flags: uninsdeletevalue
Root: HKCR; Subkey: "Neuron.Ncon"; ValueType: string; ValueName: ""; ValueData: "NCON Container"; Flags: uninsdeletekey
Root: HKCR; Subkey: "Neuron.Ncon\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\nucleus.exe"",0"
Root: HKCR; Subkey: "Neuron.Ncon\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\nucleus.exe"" ""%1"""

[Code]
const
  SystemEnvironmentKey = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';

function NormalizePath(const Value: string): string;
var
  S: string;
begin
  S := Trim(Value);
  while (Length(S) > 0) and (S[Length(S)] = '\') do
  begin
    Delete(S, Length(S), 1);
  end;
  Result := Lowercase(S);
end;

function ReadSystemPath(var Value: string): Boolean;
begin
  Result := RegQueryStringValue(HKLM, SystemEnvironmentKey, 'Path', Value);
  if not Result then
    Value := '';
end;

procedure SplitPathEntries(const Value: string; var Parts: TArrayOfString);
var
  Work: string;
  Token: string;
  DelimPos: Integer;
  Count: Integer;
begin
  Work := Value;
  SetArrayLength(Parts, 0);

  while Work <> '' do
  begin
    DelimPos := Pos(';', Work);
    if DelimPos = 0 then
    begin
      Token := Work;
      Work := '';
    end
    else
    begin
      Token := Copy(Work, 1, DelimPos - 1);
      Delete(Work, 1, DelimPos);
    end;

    Count := GetArrayLength(Parts);
    SetArrayLength(Parts, Count + 1);
    Parts[Count] := Token;
  end;
end;

function WriteSystemPath(const Value: string): Boolean;
begin
  Result := RegWriteExpandStringValue(HKLM, SystemEnvironmentKey, 'Path', Value);
end;

function PathContainsEntry(const ExistingPath, CandidateEntry: string): Boolean;
var
  Parts: TArrayOfString;
  I: Integer;
begin
  Result := False;
  SplitPathEntries(ExistingPath, Parts);
  for I := 0 to GetArrayLength(Parts) - 1 do
  begin
    if NormalizePath(Parts[I]) = NormalizePath(CandidateEntry) then
    begin
      Result := True;
      Exit;
    end;
  end;
end;

function RemovePathEntry(const ExistingPath, EntryToRemove: string): string;
var
  Parts: TArrayOfString;
  Filtered: TArrayOfString;
  I: Integer;
  Count: Integer;
begin
  SplitPathEntries(ExistingPath, Parts);
  SetArrayLength(Filtered, GetArrayLength(Parts));
  Count := 0;

  for I := 0 to GetArrayLength(Parts) - 1 do
  begin
    if Trim(Parts[I]) = '' then
      Continue;
    if NormalizePath(Parts[I]) = NormalizePath(EntryToRemove) then
      Continue;

    Filtered[Count] := Parts[I];
    Count := Count + 1;
  end;

  Result := '';
  for I := 0 to Count - 1 do
  begin
    if Result <> '' then
      Result := Result + ';';
    Result := Result + Filtered[I];
  end;
end;

procedure AddBinToSystemPath();
var
  ExistingPath: string;
  NewPath: string;
  BinDir: string;
begin
  BinDir := ExpandConstant('{app}\bin');
  if not ReadSystemPath(ExistingPath) then
    ExistingPath := '';

  if PathContainsEntry(ExistingPath, BinDir) then
    Exit;

  if ExistingPath = '' then
    NewPath := BinDir
  else if ExistingPath[Length(ExistingPath)] = ';' then
    NewPath := ExistingPath + BinDir
  else
    NewPath := ExistingPath + ';' + BinDir;

  if not WriteSystemPath(NewPath) then
    MsgBox('Failed to update the system PATH. Add "' + BinDir + '" manually.',
      mbError, MB_OK);
end;

procedure RemoveBinFromSystemPath();
var
  ExistingPath: string;
  NewPath: string;
  BinDir: string;
begin
  BinDir := ExpandConstant('{app}\bin');
  if not ReadSystemPath(ExistingPath) then
    Exit;

  NewPath := RemovePathEntry(ExistingPath, BinDir);
  if NewPath <> ExistingPath then
    WriteSystemPath(NewPath);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('addtopath') then
    AddBinToSystemPath();
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    RemoveBinFromSystemPath();
end;
