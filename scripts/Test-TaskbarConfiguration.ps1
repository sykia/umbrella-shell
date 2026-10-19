# Explicit desktop checks; never run in unattended CI. Does not drag the taskbar.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$HostPath,
    [string]$FixturePath,
    [switch]$OwnerDeath
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$exe = (Resolve-Path -LiteralPath $HostPath).Path
if ((Split-Path $exe -Leaf) -ne 'UmbrellaTaskbarHost.exe' -or
    !(Test-Path -LiteralPath (Join-Path (Split-Path $exe -Parent) 'UmbrellaTaskbarModule.dll'))) {
    throw 'Provide the host with its matching module beside it.'
}
if (!$OwnerDeath) {
    if ($FixturePath) {
        $fixture = (Resolve-Path -LiteralPath $FixturePath).Path
        & $fixture (Join-Path (Split-Path $PSScriptRoot -Parent) 'src/presentation/TaskbarConfiguration.qml')
        if ($LASTEXITCODE) { throw 'Settings-to-Explorer integration failed.' }
    } else {
        & $exe --verify-taskview-placement
        if ($LASTEXITCODE) { throw 'Native position/click/focus/reset verification failed.' }
    }
    return
}
$baseline = @(& $exe --inspect-taskview)
if ($LASTEXITCODE) { throw 'Cannot inspect the initial taskbar.' }
$before = [regex]::Match(($baseline -join "`n"), 'offset=0\.0 button=\((-?\d+)\.0,')
if (!$before.Success) { throw 'Initial position is unknown.' }
$info = [System.Diagnostics.ProcessStartInfo]::new($exe, '--configure-taskview')
$info.UseShellExecute = $false
$info.CreateNoWindow = $true
$info.RedirectStandardInput = $true
$info.RedirectStandardOutput = $true
$child = [System.Diagnostics.Process]::Start($info)
$deadline = [DateTime]::UtcNow.AddSeconds(20)
$ready = $false
$moved = $false
try {
    while ([DateTime]::UtcNow -lt $deadline) {
        $read = $child.StandardOutput.ReadLineAsync()
        while (!$read.Wait(250) -and [DateTime]::UtcNow -lt $deadline) {}
        if (!$read.IsCompleted) { break }
        $line = $read.GetAwaiter().GetResult()
        if ($null -eq $line) { break }
        Write-Output $line
        if ($line -eq 'layout-ready') {
            $ready = $true
            $child.StandardInput.WriteLine('after-widgets 8')
            $child.StandardInput.Flush()
        } elseif ($ready -and $line -match 'status=00000000 edit=0 drag=0 moves=1 .*offset=-[1-9]') {
            $moved = $true
        } elseif ($ready -and $line -eq 'taskview-ok') { break }
        elseif ($ready -and $line -eq 'taskview-refused') { break }
    }
} finally {
    # Deliberately terminate ONLY this newly created child, never Explorer/other hosts.
    if (!$child.HasExited) { $child.Kill(); $null = $child.WaitForExit(3000) }
    $child.Dispose()
}
if (!$moved) { throw 'The child did not confirm the requested original-button placement.' }
Start-Sleep -Milliseconds 1500
$inspection = @(& $exe --inspect-taskview)
if ($LASTEXITCODE) { throw 'Post-crash inspection failed.' }
$inspection | Write-Output
$after = [regex]::Match(($inspection -join "`n"), 'offset=0\.0 button=\((-?\d+)\.0,')
if (!$after.Success -or $after.Groups[1].Value -ne $before.Groups[1].Value -or
    ($inspection -join "`n") -notmatch 'input hit=1 rootBackground=1') {
    throw 'Original position or native hit testing was not restored.'
}
Write-Output 'Configuration owner-death recovery passed: original position and native input restored.'
