; Inno Setup script for ZenViewer (https://jrsoftware.org/isinfo.php).
; Build it with windows\installer.bat (after windows\build.bat), which writes
; ZenViewer-Setup-<version>.exe to the project folder.

; installer.bat passes AppVersion, read from project(... VERSION ...) in CMakeLists.txt
#ifndef AppVersion
  #error Run windows\installer.bat, which sets AppVersion
#endif

[Setup]
AppId={{3DF6DA18-9CE2-458D-990D-3AED2B8F5133}
AppName=ZenViewer
AppVersion={#AppVersion}
AppVerName=ZenViewer {#AppVersion}
AppPublisher=ZenViewer
DefaultDirName={autopf}\ZenViewer
DefaultGroupName=ZenViewer
DisableProgramGroupPage=yes
; Installs for the current user without admin rights, or for everyone if the
; user picks that in the first dialog
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Qt 6.12 needs Windows 10 1809 or newer
MinVersion=10.0.17763
SetupIconFile=..\src\icons\zenviewer.ico
UninstallDisplayIcon={app}\zenviewer.exe
ChangesAssociations=yes
; Tells running programs (Explorer, new terminals) that PATH changed
ChangesEnvironment=yes
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
OutputDir=..
OutputBaseFilename=ZenViewer-Setup-{#AppVersion}

[Tasks]
Name: desktopicon; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
; For the user alone or for everyone, the same as the install (see [Code])
Name: addtopath; Description: "Add ZenViewer to the PATH, to run zenviewer from a command prompt"; GroupDescription: "Command line:"

[InstallDelete]
; Versions before 0.4.1 kept the DLLs, plugins and qt.conf beside the exe; they now live in dll\
Type: files; Name: "{app}\*.dll"
Type: files; Name: "{app}\qt.conf"
Type: filesandordirs; Name: "{app}\platforms"
Type: filesandordirs; Name: "{app}\styles"
Type: filesandordirs; Name: "{app}\imageformats"
Type: filesandordirs; Name: "{app}\iconengines"

[Files]
; The app as windows\build.bat leaves it, without CMake's scripts folder
Source: "build\*"; Excludes: "\scripts"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\ZenViewer"; Filename: "{app}\zenviewer.exe"
Name: "{autodesktop}\ZenViewer"; Filename: "{app}\zenviewer.exe"; Tasks: desktopicon

[Registry]
; "Open with > ZenViewer" for images. It doesn't take over the default viewer;
; the user can pick ZenViewer as default in Windows' Settings.
Root: HKA; Subkey: "Software\Classes\ZenViewer.Image"; ValueType: string; ValueData: "Image (ZenViewer)"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\ZenViewer.Image\DefaultIcon"; ValueType: string; ValueData: "{app}\zenviewer.exe,0"
Root: HKA; Subkey: "Software\Classes\ZenViewer.Image\shell\open\command"; ValueType: string; ValueData: """{app}\zenviewer.exe"" ""%1"""
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "ZenViewer"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\shell\open\command"; ValueType: string; ValueData: """{app}\zenviewer.exe"" ""%1"""
Root: HKA; Subkey: "Software\Classes\.jpg\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".jpg"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.jpeg\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".jpeg"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.jpe\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".jpe"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.png\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".png"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.gif\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".gif"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.bmp\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".bmp"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.dib\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".dib"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.webp\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".webp"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.tif\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".tif"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.tiff\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".tiff"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.svg\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".svg"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.svgz\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".svgz"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.ico\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".ico"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.cur\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".cur"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.tga\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".tga"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.wbmp\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".wbmp"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.icns\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".icns"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.pbm\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".pbm"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.pgm\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".pgm"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.ppm\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".ppm"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.pnm\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".pnm"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.xbm\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".xbm"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.xpm\OpenWithProgids"; ValueType: string; ValueName: "ZenViewer.Image"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\zenviewer.exe\SupportedTypes"; ValueType: string; ValueName: ".xpm"; ValueData: ""

[Run]
Filename: "{app}\zenviewer.exe"; Description: "{cm:LaunchProgram,ZenViewer}"; Flags: nowait postinstall skipifsilent

[Code]
// The PATH the install changes: the user's own for a user install, the
// system's for an install for everyone. The folder holds only zenviewer.exe
// and the licences; the DLLs are in dll\, so they don't end up on the PATH.
function EnvRoot: Integer;
begin
  if IsAdminInstallMode then
    Result := HKEY_LOCAL_MACHINE
  else
    Result := HKEY_CURRENT_USER;
end;

function EnvKey: String;
begin
  if IsAdminInstallMode then
    Result := 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment'
  else
    Result := 'Environment';
end;

function SameDir(A, B: String): Boolean;
begin
  Result := CompareText(RemoveBackslashUnlessRoot(Trim(A)), RemoveBackslashUnlessRoot(Trim(B))) = 0;
end;

// Path without Dir (and without empty entries); Found tells whether Dir was in it
function WithoutDir(Path, Dir: String; var Found: Boolean): String;
var
  Rest, Entry: String;
  P: Integer;
begin
  Result := '';
  Found := False;
  Rest := Path;
  while Rest <> '' do
  begin
    P := Pos(';', Rest);
    if P = 0 then
    begin
      Entry := Rest;
      Rest := '';
    end else
    begin
      Entry := Copy(Rest, 1, P - 1);
      Delete(Rest, 1, P);
    end;
    if SameDir(Entry, Dir) then
      Found := True
    else if Trim(Entry) <> '' then
    begin
      if Result <> '' then
        Result := Result + ';';
      Result := Result + Entry;
    end;
  end;
end;

procedure AddToPath(Dir: String);
var
  Path, Rest: String;
  Found: Boolean;
begin
  // RegQueryStringValue leaves %VARIABLES% unexpanded, so they are written back as they were
  if not RegQueryStringValue(EnvRoot, EnvKey, 'Path', Path) then
    Path := '';
  Rest := WithoutDir(Path, Dir, Found);
  if Found then
    Exit;
  if (Path <> '') and (Path[Length(Path)] <> ';') then
    Path := Path + ';';
  RegWriteExpandStringValue(EnvRoot, EnvKey, 'Path', Path + Dir);
end;

procedure RemoveFromPath(Dir: String);
var
  Path, Rest: String;
  Found: Boolean;
begin
  if not RegQueryStringValue(EnvRoot, EnvKey, 'Path', Path) then
    Exit;
  Rest := WithoutDir(Path, Dir, Found);
  if Found then
    RegWriteExpandStringValue(EnvRoot, EnvKey, 'Path', Rest);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    if WizardIsTaskSelected('addtopath') then
      AddToPath(ExpandConstant('{app}'))
    else
      RemoveFromPath(ExpandConstant('{app}')); // unticked when updating an install that had it
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    RemoveFromPath(ExpandConstant('{app}'));
end;
