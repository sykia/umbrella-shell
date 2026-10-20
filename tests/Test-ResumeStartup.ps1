# Explicit desktop integration check; runs the real GUI hidden, never restarts Explorer.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$RuntimeDirectory, [Parameter(Mandatory)][string]$ReportDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$runtime = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$exe = Join-Path $runtime 'UmbrellaShell.exe'
$helper = Join-Path $runtime 'UmbrellaMenuHost.exe'
$reportDirectoryPath = [IO.Path]::GetFullPath($ReportDirectory)
if (Test-Path -LiteralPath $reportDirectoryPath) { throw 'Use an absent report directory.' }
$null = New-Item -ItemType Directory -Path $reportDirectoryPath
$runPath = 'HKCU:/Software/Microsoft/Windows/CurrentVersion/Run'
$run = Get-ItemProperty -LiteralPath $runPath
$hadRun = $run.PSObject.Properties.Name -contains 'UmbrellaShell'
$runValue = if ($hadRun) { $run.UmbrellaShell } else { $null }
$ini = Join-Path $env:APPDATA 'UmbrellaShell/Taskbar.ini'
$iniHash = if (Test-Path -LiteralPath $ini) { (Get-FileHash -LiteralPath $ini -Algorithm SHA256).Hash } else { '' }
$receipt = Join-Path $env:LOCALAPPDATA 'UmbrellaShell/UmbrellaShell/startup-status.json'
$hadReceipt = Test-Path -LiteralPath $receipt
if ($hadReceipt) { Copy-Item -LiteralPath $receipt -Destination (Join-Path $reportDirectoryPath 'previous-status.json') }
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class UmbrellaStartupTest {
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string name);
 public static IntPtr SettingsWindow() { return FindWindow(null,"Umbrella — Настройки"); }
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string name);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window,uint message,IntPtr wp,IntPtr lp);
}
'@
function Wait-Receipt([int]$OwnerPid, [string]$PreviousTimestamp = '') {
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        if (Test-Path -LiteralPath $receipt) {
            try {
                $status = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json
                if ($status.ownerPid -eq $OwnerPid -and $status.succeeded -and $status.timestampUtc -ne $PreviousTimestamp) { return $status }
            } catch { }
        }
        Start-Sleep -Milliseconds 80
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'No successful startup receipt from this owner.'
}
$owner = $null
try {
    $owner = Start-Process -FilePath $exe -ArgumentList '--resume-settings' -WindowStyle Hidden -PassThru
    $first = Wait-Receipt $owner.Id
    $first | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $reportDirectoryPath 'first-start.json') -Encoding utf8
    $window = [UmbrellaStartupTest]::SettingsWindow()
    if ($window -eq [IntPtr]::Zero -or [UmbrellaStartupTest]::IsWindowVisible($window)) { throw "Startup was not hidden: window=$window visible=$([UmbrellaStartupTest]::IsWindowVisible($window))." }
    if (@($first.panels).Count -ne 2) { throw 'Expected two real taskbars.' }
    $first | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $reportDirectoryPath 'first-start.json') -Encoding utf8
    $duplicate = Start-Process -FilePath $exe -ArgumentList '--resume-settings' -WindowStyle Hidden -PassThru
    if (!$duplicate.WaitForExit(5000) -or $duplicate.ExitCode -ne 0) { throw 'Duplicate startup did not exit normally.' }
    if ([UmbrellaStartupTest]::IsWindowVisible($window)) { throw 'Duplicate resume opened settings.' }
    $owners = @(Get-CimInstance Win32_Process | Where-Object { $_.ExecutablePath -eq $exe -and $_.CommandLine -notmatch '--taskbar-watch' })
    if ($owners.Count -ne 1) { throw 'Multiple GUI owners.' }
    # Exercise the same event received after Explorer recreates Taskbar; Explorer stays running.
    $timestamp = $first.timestampUtc
    Start-Sleep -Milliseconds 1100
    if (![UmbrellaStartupTest]::PostMessage($window,[UmbrellaStartupTest]::RegisterWindowMessage('TaskbarCreated'),[IntPtr]::Zero,[IntPtr]::Zero)) { throw 'Cannot deliver lifecycle event.' }
    $recreated = Wait-Receipt $owner.Id $timestamp
    $recreated | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $reportDirectoryPath 'taskbar-created-event.json') -Encoding utf8
    if (($first.panels | Select-Object device,width,height,alpha | ConvertTo-Json -Compress) -ne ($recreated.panels | Select-Object device,width,height,alpha | ConvertTo-Json -Compress)) { throw 'Lifecycle replay changed panel configuration.' }
    $owner.Refresh(); $cpuBefore = $owner.TotalProcessorTime.TotalMilliseconds; $handlesBefore = $owner.HandleCount
    Start-Sleep -Milliseconds 1500
    $owner.Refresh()
    @{ idleSampleMs = 1500; cpuMs = $owner.TotalProcessorTime.TotalMilliseconds - $cpuBefore; workingSetBytes = $owner.WorkingSet64; handlesBefore = $handlesBefore; handlesAfter = $owner.HandleCount } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $reportDirectoryPath 'idle-sample.json') -Encoding utf8
    $quit = Start-Process -FilePath $helper -ArgumentList '--quit-settings' -WindowStyle Hidden -PassThru -Wait
    if ($quit.ExitCode -ne 0 -or !$owner.WaitForExit(7000)) { throw 'Owner did not quit normally.' }
    $owner = Start-Process -FilePath $exe -ArgumentList '--resume-settings' -WindowStyle Hidden -PassThru
    $second = Wait-Receipt $owner.Id
    $second | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $reportDirectoryPath 'second-start.json') -Encoding utf8
    if (($first.panels | Select-Object device,width,height,alpha | ConvertTo-Json -Compress) -ne ($second.panels | Select-Object device,width,height,alpha | ConvertTo-Json -Compress)) { throw 'Process restart changed panel configuration.' }
    Write-Output 'Passed: hidden real startup, two panels, duplicate rejection, TaskbarCreated replay, full process restart, INI preservation. This is NOT a Windows sign-in test.'
} finally {
    if (Test-Path -LiteralPath $receipt) { Copy-Item -LiteralPath $receipt -Destination (Join-Path $reportDirectoryPath 'last-run-status.json') -Force }
    if ($owner -and !$owner.HasExited) {
        $quit = Start-Process -FilePath $helper -ArgumentList '--quit-settings' -WindowStyle Hidden -PassThru -Wait
        if ($quit.ExitCode -ne 0 -or !$owner.WaitForExit(7000)) { Write-Warning 'Test owner remains running; no forced termination used.' }
    }
    if ($hadRun) { Set-ItemProperty -LiteralPath $runPath -Name UmbrellaShell -Value $runValue }
    else { Remove-ItemProperty -LiteralPath $runPath -Name UmbrellaShell -ErrorAction SilentlyContinue }
    if ($hadReceipt) { Copy-Item -LiteralPath (Join-Path $reportDirectoryPath 'previous-status.json') -Destination $receipt -Force }
    if ((Test-Path -LiteralPath $ini) -and (Get-FileHash -LiteralPath $ini -Algorithm SHA256).Hash -ne $iniHash) { throw 'The user INI changed during testing.' }
}
