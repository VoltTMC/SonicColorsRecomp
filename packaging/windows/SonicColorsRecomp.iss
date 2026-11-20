; Inno Setup 6+ script — per-user Windows installer for Sonic Colors Recomp.
; Built by tools/package_windows_installer.sh from a staged install tree (no portable.txt).
;
; AppId is stable across versions so Windows upgrades the same Apps & Features entry.
; Fresh install is the default; portable migration is optional and never required.

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
AllowNoIcons=yes

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
  MigrateChoicePage: TInputOptionWizardPage;
  MigrateDirPage: TInputDirWizardPage;

function IsUpgrade(): Boolean;
begin
  Result := RegKeyExists(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppId}_is1')
         or RegKeyExists(HKLM, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppId}_is1');
end;

function CmdMigrateDir(): string;
begin
  { Optional silent/CLI override. Avoid param names containing "MIGRATE" (Setup aborts).
    Example: Setup.exe /VERYSILENT /PORTABLEDIR="D:\old\portable" }
  Result := Trim(ExpandConstant('{param:PORTABLEDIR}'));
end;

function WantMigrate(): Boolean;
begin
  if CmdMigrateDir() <> '' then
  begin
    Result := True;
    Exit;
  end;
  { Index 0 = fresh install (default); index 1 = migrate from portable. }
  Result := Assigned(MigrateChoicePage) and (MigrateChoicePage.SelectedValueIndex = 1);
end;

function LooksLikePortableRoot(const Dir: string): Boolean;
begin
  Result := DirExists(Dir) and FileExists(AddBackslash(Dir) + '{#MyAppExeName}');
end;

function SelectedPortableRoot(): string;
begin
  Result := CmdMigrateDir();
  if Result <> '' then Exit;
  if Assigned(MigrateDirPage) then
    Result := Trim(MigrateDirPage.Values[0]);
end;

procedure InitializeWizard();
begin
  MigrateChoicePage := CreateInputOptionPage(wpSelectDir,
    'Existing portable data',
    'Most people can skip this step.',
    'If you previously ran Sonic Colors Recomp from a portable ZIP folder, you can copy that data into the normal user-data location. New users should leave the default selected.',
    True, False);
  MigrateChoicePage.Add('Fresh install (recommended) — no portable folder needed');
  MigrateChoicePage.Add('Migrate data from an existing portable installation');
  MigrateChoicePage.SelectedValueIndex := 0;

  MigrateDirPage := CreateInputDirPage(MigrateChoicePage.ID,
    'Portable folder',
    'Select the folder that contains your old portable copy.',
    'Choose the folder that contains SonicColorsRecomp.exe (often also portable.txt).' + #13#10 +
    'Data is copied into %APPDATA%\SonicColorsRecomp. The original folder is not deleted.',
    False, '');
  MigrateDirPage.Add('Portable folder');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  { Upgrades already have AppData — never force migration. }
  if IsUpgrade() then
  begin
    if (PageID = MigrateChoicePage.ID) or (PageID = MigrateDirPage.ID) then
      Result := True;
    Exit;
  end;
  { Silent /PORTABLEDIR= already has a path — skip both migrate wizard pages. }
  if CmdMigrateDir() <> '' then
  begin
    if (PageID = MigrateChoicePage.ID) or (PageID = MigrateDirPage.ID) then
      Result := True;
    Exit;
  end;
  { Directory browse page only when the user opted into migration in the GUI. }
  if PageID = MigrateDirPage.ID then
    Result := not WantMigrate();
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  PortableRoot: string;
begin
  Result := True;
  if CurPageID <> MigrateDirPage.ID then
    Exit;
  if not WantMigrate() then
    Exit;

  PortableRoot := SelectedPortableRoot();
  if PortableRoot = '' then
  begin
    MsgBox('Choose your old portable folder, or go Back and select Fresh install.',
      mbError, MB_OK);
    Result := False;
    Exit;
  end;
  if not DirExists(PortableRoot) then
  begin
    MsgBox('That folder does not exist:' + #13#10 + PortableRoot + #13#10 + #13#10 +
      'Pick another folder, or go Back and choose Fresh install.',
      mbError, MB_OK);
    Result := False;
    Exit;
  end;
  if not LooksLikePortableRoot(PortableRoot) then
  begin
    MsgBox('That folder does not look like a Sonic Colors Recomp install.' + #13#10 + #13#10 +
      'Expected file: ' + AddBackslash(PortableRoot) + '{#MyAppExeName}' + #13#10 + #13#10 +
      'Pick the folder that contains the game executable, or go Back and choose Fresh install.',
      mbError, MB_OK);
    Result := False;
    Exit;
  end;
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
  { Fresh install (default) and silent installs without /PORTABLEDIR=: no portable folder. }
  if not WantMigrate() then Exit;

  PortableRoot := SelectedPortableRoot();
  if (PortableRoot = '') or (not LooksLikePortableRoot(PortableRoot)) then
  begin
    { Invalid /PORTABLEDIR= must not abort the install — skip copy and continue clean. }
    Log('Migrate skipped: portable root missing or invalid: ' + PortableRoot);
    Exit;
  end;

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
