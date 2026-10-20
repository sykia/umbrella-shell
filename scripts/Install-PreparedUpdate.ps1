# Run only after the owner approves this specific prepared build.
[CmdletBinding()]
param([string]$InstallDirectory = (Join-Path $env:LOCALAPPDATA 'Programs/Umbrella Shell'), [switch]$ValidateOnly)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$target = (Resolve-Path -LiteralPath $InstallDirectory).Path.TrimEnd('\','/')
$expected = [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'Programs/Umbrella Shell')).TrimEnd('\','/')
if (![string]::Equals($target,$expected,[StringComparison]::OrdinalIgnoreCase)) { throw 'This update is prepared for the current per-user installation only.' }
$runtime = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'runtime')).Path
$manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw | ConvertFrom-Json
if ($manifest.format -ne 1) { throw 'Unsupported manifest.' }
foreach ($file in $manifest.files) {
    $path = [IO.Path]::GetFullPath((Join-Path $runtime $file.path))
    if (!$path.StartsWith($runtime + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $file.path -match '(^|[\\/])AGENTS(?:\.local)?\.md$') { throw 'Invalid manifest path.' }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.sha256) { throw "Hash mismatch: $($file.path)" }
}
if ($ValidateOnly) {
    Write-Output "Validated $(@($manifest.files).Count) files for $target. No processes, registry or installed files changed."
    return
}
$backup = Join-Path $PSScriptRoot ('backup-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
if (Test-Path -LiteralPath $backup) { throw 'Backup path already exists.' }
$null = New-Item -ItemType Directory -Path $backup
$runPath = 'HKCU:/Software/Microsoft/Windows/CurrentVersion/Run'
$run = Get-ItemProperty -LiteralPath $runPath
$savedRun = @{}
foreach ($name in @('UmbrellaShell','UmbrellaMenu')) {
    if ($run.PSObject.Properties.Name -contains $name) { $savedRun[$name] = $run.$name }
}
$savedRun | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $backup 'login.json') -Encoding utf8
$settings = Join-Path $env:APPDATA 'UmbrellaShell/Taskbar.ini'
if (Test-Path -LiteralPath $settings) { Copy-Item -LiteralPath $settings -Destination (Join-Path $backup 'Taskbar.ini') }
# Back up binaries before stopping anything. Nothing in the settings directories is removed.
Copy-Item -LiteralPath $target -Destination (Join-Path $backup 'installation') -Recurse
$helper = Join-Path $target 'UmbrellaMenuHost.exe'
function Invoke-Helper([string]$Argument) {
    $process = Start-Process -FilePath $helper -ArgumentList $Argument -WindowStyle Hidden -PassThru -Wait
    if ($process.ExitCode) { throw "Menu helper $Argument failed: $($process.ExitCode)" }
}
Invoke-Helper '--quit-settings'
Invoke-Helper '--stop'
$remaining = @(Get-CimInstance Win32_Process | Where-Object {
    $_.ExecutablePath -and $_.ExecutablePath.StartsWith($target + '\',[StringComparison]::OrdinalIgnoreCase) -and $_.Name -match '^Umbrella.*\.exe$'
})
if ($remaining.Count) { throw 'Umbrella processes still own installed files. No files replaced.' }
$copied = $false
try {
    foreach ($file in $manifest.files) {
        $destination = Join-Path $target $file.path
        $null = New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force
        Copy-Item -LiteralPath (Join-Path $runtime $file.path) -Destination $destination -Force
    }
    $copied = $true
} finally {
    if (!$copied) {
        foreach ($file in Get-ChildItem -LiteralPath (Join-Path $backup 'installation') -Recurse -File) {
            $relative = [IO.Path]::GetRelativePath((Join-Path $backup 'installation'),$file.FullName)
            Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $target $relative) -Force
        }
        Start-Process -FilePath $helper -ArgumentList '--start' -WindowStyle Hidden
        throw "Copy failed; original files copied back from $backup. Inspect before retrying."
    }
}
Start-Process -FilePath $helper -ArgumentList '--start' -WindowStyle Hidden
$resume = $true
if (Test-Path -LiteralPath $settings) {
    $resumeLine = Get-Content -LiteralPath $settings | Where-Object { $_ -match '^resumeAppearance=' } | Select-Object -Last 1
    if ($resumeLine -and $resumeLine -match '=(false|0)$') { $resume = $false }
}
if ($resume) {
    Set-ItemProperty -LiteralPath $runPath -Name UmbrellaShell -Value ('"' + (Join-Path $target 'UmbrellaShell.exe') + '" --resume-settings')
} else { Remove-ItemProperty -LiteralPath $runPath -Name UmbrellaShell -ErrorAction SilentlyContinue }
Start-Process -FilePath (Join-Path $target 'UmbrellaShell.exe') -ArgumentList '--resume-settings' -WindowStyle Hidden
Write-Output "Updated. Backup: $backup. User INI was preserved. Verify both panels, then sign out/in manually."
