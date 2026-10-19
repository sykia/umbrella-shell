param([Parameter(Mandatory)][string]$HostPath)
$ErrorActionPreference = 'Stop'
# Opt-in desktop test. Temporarily changes Taskbar; EOF restores the session.
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class UmbrellaAlignmentProbe {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hwnd);
}
'@
$windows = @()
foreach ($class in @('Shell_TrayWnd','Shell_SecondaryTrayWnd')) {
    $after = [IntPtr]::Zero
    while (($after = [UmbrellaAlignmentProbe]::FindWindowEx([IntPtr]::Zero,$after,$class,$null)) -ne [IntPtr]::Zero) {
        $windows += $after
        if ($windows.Count -gt 16) { throw 'Too many taskbars' }
    }
}
if (-not $windows.Count) { throw 'No Windows taskbar found' }
function Read-Geometry([IntPtr]$Window) {
    $rect = [UmbrellaAlignmentProbe+Rect]::new()
    if (-not [UmbrellaAlignmentProbe]::GetWindowRect($Window,[ref]$rect)) { throw 'Taskbar disappeared' }
    return @($rect.Left,$rect.Top,$rect.Right,$rect.Bottom)
}
$before = @{}
foreach ($window in $windows) { $before[$window] = Read-Geometry $window }
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = (Resolve-Path -LiteralPath $HostPath).Path
$start.Arguments = '--serve'
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardInput = $true
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$process = [Diagnostics.Process]::new()
$process.StartInfo = $start
function Expect-Line([string]$Expected) {
    $deadline = [Diagnostics.Stopwatch]::StartNew()
    while ($deadline.ElapsedMilliseconds -lt 8000) {
        $read = $process.StandardOutput.ReadLineAsync()
        if (-not $read.Wait(8000 - [int]$deadline.ElapsedMilliseconds)) { throw 'Native host response timed out' }
        $line = $read.Result
        if ($line -eq $Expected) { return }
        if ($null -eq $line -or $line.StartsWith('error ') -or $line.StartsWith('FAILED')) { throw "Native host rejected configuration: $line" }
    }
    throw 'Native host response timed out'
}
$failure = $null
try {
    if (-not $process.Start()) { throw 'Native host did not start' }
    Expect-Line 'ready'
    foreach ($case in @(@(3,72),@(3,96),@(1,72),@(0,72),@(2,72),@(3,48))) {
        $edge,$thickness = $case
        $islands = [int]($thickness -ge 64)
        $command = "75 $edge $thickness $islands"
        $process.StandardInput.WriteLine($command)
        $process.StandardInput.Flush()
        Expect-Line "applied $command"
        foreach ($window in $windows) {
            $verified = $false
            for ($attempt = 0; $attempt -lt 15; ++$attempt) {
                $root = [System.Windows.Automation.AutomationElement]::FromHandle($window)
                $nodes = $root.FindAll([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.Condition]::TrueCondition)
                $expected = $thickness * [UmbrellaAlignmentProbe]::GetDpiForWindow($window) / 96
                $apps = 0; $aligned = $true
                foreach ($node in $nodes) {
                    $value = $node.Current
                    if ($value.IsOffscreen -or $value.ControlType -ne [System.Windows.Automation.ControlType]::Button) { continue }
                    $isApp = $value.ClassName -eq 'Taskbar.TaskListButtonAutomationPeer'
                    $isTray = $value.ClassName.StartsWith('SystemTray.')
                    if (-not ($isApp -or $isTray)) { continue }
                    if ($isApp) { ++$apps }
                    $rect = $value.BoundingRectangle
                    $geometry = Read-Geometry $window
                    $center = if ($edge -in 0,2) { $rect.X + $rect.Width / 2 } else { $rect.Y + $rect.Height / 2 }
                    $panelCenter = if ($edge -in 0,2) { ($geometry[0] + $geometry[2]) / 2 } else { ($geometry[1] + $geometry[3]) / 2 }
                    $extent = if ($edge -in 0,2) { $geometry[2] - $geometry[0] } else { $geometry[3] - $geometry[1] }
                    if ([Math]::Abs($center - $panelCenter) -gt 1 -or [Math]::Abs($extent - $expected) -gt 1) { $aligned = $false }
                }
                if ($apps -gt 0 -and $aligned) { $verified = $true; break }
                Start-Sleep -Milliseconds 80
            }
            if (-not $verified) { throw "Native content and frame disagree: edge=$edge thickness=$thickness" }
        }
        "Verified edge=$edge thickness=$thickness"
    }
    'Passed: native app/tray centers match panel geometry at 48/72/96 DIP on all four edges.'
} catch {
    $failure = $_
    Write-Host "Alignment check failed: $_"
    throw
} finally {
    if ($process.Id -and -not $process.HasExited) {
        $process.StandardInput.Close()
        if (-not $process.WaitForExit(8000)) { throw 'Native host did not confirm restoration; not terminated' }
    }
    if ($process.HasExited) {
        $diagnostics = $process.StandardError.ReadToEnd()
        if ($failure -or $process.ExitCode -ne 0) { Write-Host $diagnostics }
        if ($process.ExitCode -ne 0) { throw 'Native host failed to confirm clean restoration' }
    }
    foreach ($window in $windows) {
        $actual = Read-Geometry $window
        if (Compare-Object $before[$window] $actual -SyncWindow 0) { throw "Taskbar geometry was not restored: before=$($before[$window]) actual=$actual" }
    }
    $process.Dispose()
}
