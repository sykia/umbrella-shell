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
DefaultDirName={localappdata}\Programs\Umbrella Shell
DefaultGroupName=Umbrella Shell
PrivilegesRequired=lowest
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
Filename: "{app}\UmbrellaMenuHost.exe"; Parameters: "--start"; Flags: nowait runhidden
Filename: "{app}\UmbrellaShell.exe"; Description: "Launch Umbrella Shell"; Flags: nowait postinstall skipifsilent unchecked


[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "UmbrellaMenu"; ValueData: """{app}\UmbrellaMenuHost.exe"" --start"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "UmbrellaShell"; Flags: uninsdeletevalue

[UninstallRun]
Filename: "{app}\UmbrellaShell.exe"; Parameters: "--restore-taskbar-style"; Flags: runhidden waituntilterminated; RunOnceId: "RestoreTaskbarAppearance"

[Code]
function StopMenuHelper(): Boolean;
var
  Filename: String;
  ExitCode: Integer;
begin
  Filename := ExpandConstant('{app}\UmbrellaMenuHost.exe');
  Result := True;
  if FileExists(Filename) then
  begin
    { Exit code 2 is the older helper's unsupported-argument result. }
    Result := Exec(Filename, '--quit-settings', '', SW_HIDE, ewWaitUntilTerminated, ExitCode) and ((ExitCode = 0) or (ExitCode = 2));
    if Result then
      Result := Exec(Filename, '--stop', '', SW_HIDE, ewWaitUntilTerminated, ExitCode) and (ExitCode = 0);
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if not StopMenuHelper() then
    Result := 'Umbrella taskbar menu could not be stopped. Close its menu and retry the installation.';
end;

function InitializeUninstall(): Boolean;
begin
  Result := StopMenuHelper();
  if not Result then
    MsgBox('Umbrella taskbar menu could not be stopped. Close its menu and retry uninstalling.', mbError, MB_OK);
end;

