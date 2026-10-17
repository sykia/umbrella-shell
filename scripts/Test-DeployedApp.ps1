[CmdletBinding()]
param([string]$Executable = 'stage/UmbrellaShell.exe')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$application = (Resolve-Path -LiteralPath $Executable).Path
foreach ($argument in @('--diagnostics', '--ui-smoke')) {
    $token = [guid]::NewGuid().ToString('N')
    $output = Join-Path ([IO.Path]::GetTempPath()) "umbrella-$token.stdout"
    $errors = Join-Path ([IO.Path]::GetTempPath()) "umbrella-$token.stderr"
    try {
        $process = Start-Process -FilePath $application -ArgumentList $argument -PassThru -WindowStyle Hidden -RedirectStandardOutput $output -RedirectStandardError $errors
        if (!$process.WaitForExit(20000)) {
            $process.Kill()
            $process.WaitForExit()
            throw "Deployed application timed out: $argument"
        }
        # Read ExitCode from the process object: GUI subsystem executables do not
        # reliably populate PowerShell LASTEXITCODE with a direct invocation.
        $process.WaitForExit()
        Get-Content -LiteralPath $output
        Get-Content -LiteralPath $errors
        if ($process.ExitCode -ne 0) { throw "Deployed application failed: $argument (exit $($process.ExitCode))" }
    } finally {
        if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
        if (Test-Path -LiteralPath $errors) { Remove-Item -LiteralPath $errors }
        if (Get-Variable process -ErrorAction SilentlyContinue) { $process.Dispose() }
    }
}
