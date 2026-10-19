# Explicit interactive desktop test; never part of unattended CI/CTest.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$HostPath,
    [switch]$OwnerDeath
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$exe = (Resolve-Path -LiteralPath $HostPath).Path
if ((Split-Path $exe -Leaf) -ne 'UmbrellaTaskbarHost.exe' -or
    !(Test-Path -LiteralPath (Join-Path (Split-Path $exe -Parent) 'UmbrellaTaskbarModule.dll'))) {
    throw 'Provide an UmbrellaTaskbarHost.exe with its matching module beside it.'
}
if (!$OwnerDeath) {
    & $exe --verify-taskview
    if ($LASTEXITCODE) { throw 'Interactive Task View verification failed; inspect the stage output.' }
    return
}
# This mode deliberately terminates only the newly created prototype child.
$baseline = @(& $exe --inspect-taskview)
if ($LASTEXITCODE) { throw 'Cannot inspect the initial taskbar.' }
$baselineText = $baseline -join "`n"
$background = [regex]::Match($baselineText, 'input hit=1 rootBackground=(0|1)')
if (!$background.Success) { throw 'Initial native hit testing was not confirmed.' }
$info = [System.Diagnostics.ProcessStartInfo]::new($exe, '--verify-taskview-owner-death')
$info.UseShellExecute = $false
$info.CreateNoWindow = $true
$info.RedirectStandardOutput = $true
$info.RedirectStandardError = $true
$child = [System.Diagnostics.Process]::Start($info)
$lines = [System.Collections.Generic.List[string]]::new()
$deadline = [DateTime]::UtcNow.AddSeconds(55)
$ready = $false
try {
    while ([DateTime]::UtcNow -lt $deadline) {
        $read = $child.StandardOutput.ReadLineAsync()
        while (!$read.Wait(500) -and [DateTime]::UtcNow -lt $deadline) {}
        if (!$read.IsCompleted) { break }
        $line = $read.GetAwaiter().GetResult()
        if ($null -eq $line) { break }
        $lines.Add($line)
        Write-Output $line
        if ($line.StartsWith('verify: ready-for-owner-death')) { $ready = $true; break }
    }
} finally {
    if (!$child.HasExited) { $child.Kill(); $null = $child.WaitForExit(3000) }
    $child.Dispose()
}
if (!$ready) { throw 'The child did not reach a verified moved state; owner-death cleanup requested.' }
Start-Sleep -Milliseconds 1500
$inspection = @(& $exe --inspect-taskview)
$inspectionExit = $LASTEXITCODE
$inspection | Write-Output
if ($inspectionExit) { throw 'Post-crash inspection failed.' }
$moved = [regex]::Match(($lines -join "`n"), 'verify: moved=1 beforeX=(-?\d+)')
$restored = [regex]::Match(($inspection -join "`n"), 'offset=0\.0 button=\((-?\d+)\.0,')
if (!$moved.Success -or !$restored.Success -or $moved.Groups[1].Value -ne $restored.Groups[1].Value -or
    ($inspection -join "`n") -notmatch ('input hit=1 rootBackground=' + $background.Groups[1].Value)) {
    throw 'Position, native hit test or root background did not return to the observed baseline.'
}
Write-Output 'Owner-death recovery passed: position, native hit test and root background restored.'
