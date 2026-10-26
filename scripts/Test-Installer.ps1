[CmdletBinding()]
param([string]$Installer = '')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
$project = Get-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Raw
if ($project -notmatch 'project\(UmbrellaShell VERSION (\d+\.\d+\.\d+)') { throw 'Project version missing.' }
$version = $Matches[1]
if (!$Installer) { $Installer = "dist/UmbrellaShell-$version-windows-x64-setup.exe" }
$setup = (Resolve-Path -LiteralPath (Join-Path $root $Installer)).Path
$installDir = Join-Path $env:ProgramFiles 'Umbrella Shell'
$application = Join-Path $installDir 'UmbrellaShell.exe'
$uninstaller = Join-Path $installDir 'unins000.exe'

function Invoke-InstallerProcess([string]$Label, [string]$File, [string[]]$Arguments, [string]$Log = '') {
    Write-Output "Installer round trip: $Label"
    $process = Start-Process -FilePath $File -ArgumentList $Arguments -PassThru
    try {
        if (!$process.WaitForExit(60000)) {
            $process.Kill($true)
            $process.WaitForExit()
            if ($Log -and (Test-Path -LiteralPath $Log)) { Get-Content -LiteralPath $Log }
            throw "$Label timed out after 60 seconds."
        }
        if ($null -eq $process.ExitCode -or $process.ExitCode -ne 0) {
            if ($Log -and (Test-Path -LiteralPath $Log)) { Get-Content -LiteralPath $Log }
            throw "$Label failed with exit code $($process.ExitCode)."
        }
    } finally {
        $process.Dispose()
    }
}

$setupLog = Join-Path ([IO.Path]::GetTempPath()) 'umbrella-installer-test.log'
Invoke-InstallerProcess 'setup' $setup @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', "/LOG=$setupLog") $setupLog
if (!(Test-Path -LiteralPath $application -PathType Leaf)) { throw "Installed executable missing: $application" }
if ((Get-Item -LiteralPath $application).VersionInfo.ProductVersion -notlike "$version*") { throw 'Installed version mismatch.' }

$previousPath = $env:PATH
try {
    Write-Output 'Installer round trip: deployed application smoke'
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    & (Join-Path $root 'scripts\Test-DeployedApp.ps1') -Executable $application
} finally {
    $env:PATH = $previousPath
}

$helper = Join-Path $installDir 'UmbrellaMenuHost.exe'
if (Test-Path -LiteralPath $helper) {
    Invoke-InstallerProcess 'helper settings shutdown' $helper @('--quit-settings')
    Invoke-InstallerProcess 'helper shutdown' $helper @('--stop')
}
Invoke-InstallerProcess 'uninstall' $uninstaller @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART')
if (Test-Path -LiteralPath $application) { throw 'Uninstall left the installed executable behind.' }
foreach ($name in @('UmbrellaMenu', 'UmbrellaShell')) {
    $runKey = Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -ErrorAction SilentlyContinue
    if ($null -ne $runKey -and $runKey.PSObject.Properties.Name -contains $name) {
        throw "Uninstall left the $name autostart value behind."
    }
}
Write-Output 'Installer round trip passed in Program Files: install, clean-PATH smoke, version, uninstall and autostart cleanup.'
