#ifndef AppVersion
  #error AppVersion must be supplied by the build
#endif
#ifndef StageDir
  #error StageDir must be an absolute deployment directory
#endif
#ifndef OutputDir
  #error OutputDir must be supplied
#endif

[Setup]
AppId={{658AA198-41B7-4C4C-8CFD-24CDDCC72411}
AppName=Umbrella Shell
AppVersion={#AppVersion}
AppPublisher=sykia
AppPublisherURL=https://github.com/sykia
AppSupportURL=https://github.com/sykia/umbrella-shell/issues
AppUpdatesURL=https://github.com/sykia/umbrella-shell/releases
DefaultDirName={autopf}\Umbrella Shell
DefaultGroupName=Umbrella Shell
PrivilegesRequired=admin
UsePreviousAppDir=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.22000
OutputDir={#OutputDir}
OutputBaseFilename=UmbrellaShell-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
UninstallDisplayIcon={app}\UmbrellaShell.exe
#ifdef SignedRelease
SignTool=release
SignedUninstaller=yes
#endif

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Umbrella Shell"; Filename: "{app}\UmbrellaShell.exe"
Name: "{group}\Restore taskbar appearance"; Filename: "{app}\UmbrellaShell.exe"; Parameters: "--restore-taskbar-style"
Name: "{group}\Uninstall Umbrella Shell"; Filename: "{uninstallexe}"

; Per-user taskbar menu helper; user settings remain outside the installation.
; In-place installation is NOT an atomic updater or a crash rollback mechanism.

[Run]
Filename: "{app}\UmbrellaMenuHost.exe"; Parameters: "--start"; Flags: nowait runhidden runasoriginaluser
Filename: "{app}\UmbrellaShell.exe"; Description: "Launch Umbrella Shell"; Flags: nowait postinstall skipifsilent unchecked runasoriginaluser


[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "UmbrellaMenu"; ValueData: """{app}\UmbrellaMenuHost.exe"" --start"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "UmbrellaShell"; Flags: uninsdeletevalue

[UninstallRun]
Filename: "{app}\UmbrellaShell.exe"; Parameters: "--restore-taskbar-style"; Flags: runhidden waituntilterminated; RunOnceId: "RestoreTaskbarAppearance"

[Code]
function StopUmbrellaAt(const Directory: String): Boolean;
var
  Helper: String;
  Shell: String;
  ExitCode: Integer;
begin
  Helper := AddBackslash(Directory) + 'UmbrellaMenuHost.exe';
  Shell := AddBackslash(Directory) + 'UmbrellaShell.exe';
  Result := True;
  if FileExists(Helper) then
  begin
    { Exit code 2 is the older helper's unsupported-argument result. }
    Result := Exec(Helper, '--quit-settings', '', SW_HIDE, ewWaitUntilTerminated, ExitCode) and ((ExitCode = 0) or (ExitCode = 2));
    if Result then
      Result := Exec(Helper, '--stop', '', SW_HIDE, ewWaitUntilTerminated, ExitCode) and (ExitCode = 0);
  end;
  if Result and FileExists(Shell) then
    Result := Exec(Shell, '--restore-taskbar-style', '', SW_HIDE, ewWaitUntilTerminated, ExitCode) and (ExitCode = 0);
end;

function StopUmbrellaCopies(): Boolean;
var
  LegacyDirectory: String;
begin
  LegacyDirectory := ExpandConstant('{localappdata}\Programs\Umbrella Shell');
  Result := StopUmbrellaAt(LegacyDirectory);
  if Result and (CompareText(LegacyDirectory, ExpandConstant('{app}')) <> 0) then
    Result := StopUmbrellaAt(ExpandConstant('{app}'));
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if not StopUmbrellaCopies() then
    Result := 'Umbrella could not restore the taskbar and stop its running copies. Sign out of Windows, sign in, and retry the installation.';
end;

function InitializeUninstall(): Boolean;
begin
  Result := StopUmbrellaCopies();
  if not Result then
    MsgBox('Umbrella could not restore the taskbar and stop its running copies. Sign out of Windows, sign in, and retry uninstalling.', mbError, MB_OK);
end;
