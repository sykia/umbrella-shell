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
AppPublisher=UmbrellaShell
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
Name: "{group}\Umbrella Shell diagnostics"; Filename: "{app}\UmbrellaShell.exe"; Parameters: "--diagnostics"
Name: "{group}\Open Windows Explorer"; Filename: "{app}\UmbrellaShell.exe"; Parameters: "--restore-explorer"
Name: "{group}\Uninstall Umbrella Shell"; Filename: "{uninstallexe}"

; No Winlogon, Explorer termination, startup task or user-data deletion.
; In-place installation is NOT an atomic updater or a crash rollback mechanism.
