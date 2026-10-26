$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = Split-Path $PSScriptRoot -Parent
$required = @('CMakeLists.txt', 'CMakePresets.json', 'src/app/main.cpp',
    'src/core/updates/UpdatePolicy.h', 'src/presentation/Settings.qml',
    'packaging/windows/UmbrellaShell.iss', 'scripts/Test-Installer.ps1',
    '.github/workflows/ci.yml', '.github/workflows/release.yml')
foreach ($relative in $required) {
    if (!(Test-Path -LiteralPath (Join-Path $projectRoot $relative))) { throw "Missing $relative" }
}
$null = Get-Content (Join-Path $projectRoot 'CMakePresets.json') -Raw | ConvertFrom-Json
Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.ps1' | ForEach-Object {
    $parseErrors = $null
    $tokens = $null
    $null = [System.Management.Automation.Language.Parser]::ParseFile($_.FullName, [ref]$tokens, [ref]$parseErrors)
    if ($parseErrors.Count) { throw ($parseErrors | Out-String) }
}
$installer = Get-Content (Join-Path $projectRoot 'packaging/windows/UmbrellaShell.iss') -Raw
foreach ($directive in @('DefaultDirName={autopf}\Umbrella Shell', 'PrivilegesRequired=admin', 'UsePreviousAppDir=no',
        'CloseApplications=yes', 'RestartApplications=no')) {
    if (!$installer.Contains($directive)) { throw "Missing installer constraint: $directive" }
}
Write-Output 'Foundation file, JSON, PowerShell syntax and installer constraint checks passed. This does not compile C++ or validate installer behavior.'
