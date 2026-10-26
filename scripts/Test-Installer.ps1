[CmdletBinding()]
param([string]$Installer = 'dist/UmbrellaShell-1.0.1-windows-x64-setup.exe')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
$setup = (Resolve-Path -LiteralPath (Join-Path $root $Installer)).Path
$installDir = Join-Path $env:ProgramFiles 'Umbrella Shell'
$application = Join-Path $installDir 'UmbrellaShell.exe'
$uninstaller = Join-Path $installDir 'unins000.exe'

function Invoke-InstallerProcess([string]$File, [string[]]$Arguments, [string]$Log = '') {
    $process = Start-Process -FilePath $File -ArgumentList $Arguments -PassThru -Wait
    if ($null -eq $process.ExitCode -or $process.ExitCode -ne 0) {
        if ($Log -and (Test-Path -LiteralPath $Log)) { Get-Content -LiteralPath $Log }
        throw "$File failed with exit code $($process.ExitCode)."
    }
}

$setupLog = Join-Path ([IO.Path]::GetTempPath()) 'umbrella-installer-test.log'
Invoke-InstallerProcess $setup @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', "/LOG=$setupLog") $setupLog
if (!(Test-Path -LiteralPath $application -PathType Leaf)) { throw "Installed executable missing: $application" }
if ((Get-Item -LiteralPath $application).VersionInfo.ProductVersion -notlike '1.0.1*') { throw 'Installed version mismatch.' }

$previousPath = $env:PATH
try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    & (Join-Path $root 'scripts\Test-DeployedApp.ps1') -Executable $application
} finally {
    $env:PATH = $previousPath
}

$helper = Join-Path $installDir 'UmbrellaMenuHost.exe'
if (Test-Path -LiteralPath $helper) {
    & $helper --quit-settings
    & $helper --stop
}
Invoke-InstallerProcess $uninstaller @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART')
if (Test-Path -LiteralPath $application) { throw 'Uninstall left the installed executable behind.' }
foreach ($name in @('UmbrellaMenu', 'UmbrellaShell')) {
    if ($null -ne (Get-ItemPropertyValue 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name $name -ErrorAction SilentlyContinue)) {
        throw "Uninstall left the $name autostart value behind."
    }
}
Write-Output 'Installer round trip passed in Program Files: install, clean-PATH smoke, version, uninstall and autostart cleanup.'
