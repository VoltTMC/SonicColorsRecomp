; Inno Setup 6+ script — per-user Windows installer for Sonic Colors Recomp.
; Built by tools/package_windows_installer.sh from a staged install tree (no portable.txt).
;
; AppId is stable across versions so Windows upgrades the same Apps & Features entry.

#define MyAppName "Sonic Colors Recomp"
#define MyAppNameShort "SonicColorsRecomp"
#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#define MyAppPublisher "Sonic Colors Recomp contributors"
#define MyAppURL "https://github.com/VoltTMC/SonicColorsRecomp"
#define MyAppExeName "SonicColorsRecomp.exe"
; Stable product GUID — do not change between releases.
#define MyAppId "{{B8E4D2A1-5C3F-4E9B-8A71-A1B2C3D4E5F6}"

#ifndef StageDir
  #define StageDir "..\..\dist\SonicColorsRecomp-install"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif
#ifndef OutputBase
  #define OutputBase "SonicColorsRecomp-Setup"
#endif

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
; Per-user install: no Administrator elevation for normal use / updates.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBase}-v{#MyAppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
VersionInfoVersion={#MyAppVersion}.0
VersionInfoProductName={#MyAppName}
VersionInfoCompany={#MyAppPublisher}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
RestartApplications=no
DirExistsWarning=no
UsePreviousAppDir=yes
UsePreviousGroup=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Entire staged install tree (exe, DLLs, tools/, mods/music/catalog.json, installed.txt, …).
; Never include portable.txt in StageDir.
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
var
  MigratePage: TInputDirWizardPage;

function IsUpgrade(): Boolean;
begin
  Result := RegKeyExists(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppId}_is1')
         or RegKeyExists(HKLM, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppId}_is1');
end;

procedure InitializeWizard();
begin
  MigratePage := CreateInputDirPage(wpSelectDir,
    'Import portable data (optional)',
    'If you previously used the portable ZIP, you can copy saves and mods into the installed user-data folder.',
    'Select your old portable folder (the folder that contains SonicColorsRecomp.exe).' + #13#10 +
    'Data is copied into %APPDATA%\SonicColorsRecomp — the original folder is left intact.',
    False, '');
  MigratePage.Add('Portable folder');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if PageID = MigratePage.ID then
    Result := IsUpgrade();
end;

procedure CopyTreeIfExists(const Src, Dst: string);
var
  ResultCode: Integer;
begin
  if not DirExists(Src) then Exit;
  ForceDirectories(Dst);
  Exec(ExpandConstant('{cmd}'),
    '/C xcopy "' + Src + '" "' + Dst + '\" /E /I /Y /Q /H /R',
    '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  PortableRoot, UserData: string;
begin
  if CurStep <> ssPostInstall then Exit;
  PortableRoot := Trim(MigratePage.Values[0]);
  if (PortableRoot = '') or (not DirExists(PortableRoot)) then Exit;
  if not FileExists(AddBackslash(PortableRoot) + '{#MyAppExeName}') then Exit;

  UserData := ExpandConstant('{userappdata}\SonicColorsRecomp');
  ForceDirectories(UserData);

  { Durable user data only — skip regenerable caches (shadercache, music_preview, downloads). }
  if FileExists(AddBackslash(PortableRoot) + 'config.ini') then
    CopyFile(AddBackslash(PortableRoot) + 'config.ini', AddBackslash(UserData) + 'config.ini', False);

  CopyTreeIfExists(AddBackslash(PortableRoot) + 'nand', AddBackslash(UserData) + 'nand');
  CopyTreeIfExists(AddBackslash(PortableRoot) + 'mods', AddBackslash(UserData) + 'mods');
  CopyTreeIfExists(AddBackslash(PortableRoot) + 'music_mods', AddBackslash(UserData) + 'music_mods');
  CopyTreeIfExists(AddBackslash(PortableRoot) + 'screenshots', AddBackslash(UserData) + 'screenshots');
  CopyTreeIfExists(AddBackslash(PortableRoot) + 'input_profiles', AddBackslash(UserData) + 'input_profiles');
  CopyTreeIfExists(AddBackslash(PortableRoot) + 'mod_profiles', AddBackslash(UserData) + 'mod_profiles');
  CopyTreeIfExists(AddBackslash(PortableRoot) + 'video_profiles', AddBackslash(UserData) + 'video_profiles');
  CopyTreeIfExists(AddBackslash(PortableRoot) + 'config_backups', AddBackslash(UserData) + 'config_backups');
  CopyTreeIfExists(AddBackslash(PortableRoot) + 'game', AddBackslash(UserData) + 'game');
  CopyTreeIfExists(AddBackslash(PortableRoot) + 'tools\bin', AddBackslash(UserData) + 'tools\bin');

  SaveStringToFile(AddBackslash(UserData) + 'migrated_from_portable.txt',
    'Migrated from portable folder:' + #13#10 + PortableRoot + #13#10 +
    'Original files were not deleted.' + #13#10, False);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  UserData: string;
begin
  if CurUninstallStep <> usPostUninstall then Exit;
  UserData := ExpandConstant('{userappdata}\SonicColorsRecomp');
  if not DirExists(UserData) then Exit;
  if MsgBox('Remove user data as well?' + #13#10 + #13#10 +
            'This permanently deletes saves, NAND, config, mods, and Music Library files under:' + #13#10 +
            UserData + #13#10 + #13#10 +
            'Choose No to keep your data for a future install.',
            mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
  begin
    DelTree(UserData, True, True, True);
  end;
end;
