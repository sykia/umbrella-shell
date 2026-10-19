param([Parameter(Mandatory)][string]$HostPath, [Parameter(Mandatory)][string]$FixturePath)
$ErrorActionPreference = 'Stop'
$testDirectory = Join-Path ([IO.Path]::GetTempPath()) ('umbrella-menu-launch-' + [guid]::NewGuid().ToString('N') + ' space')
New-Item -ItemType Directory -Path $testDirectory | Out-Null
$windowFixture = $null
function Wait-Marker([string]$Name) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $path = Join-Path $testDirectory $Name
    while (!(Test-Path -LiteralPath $path) -and $timer.ElapsedMilliseconds -lt 3000) { Start-Sleep -Milliseconds 25 }
    if (!(Test-Path -LiteralPath $path)) { throw "Fixture marker not received: $Name" }
}
try {
    $hostCopy = Join-Path $testDirectory 'UmbrellaMenuHost.exe'
    Copy-Item -LiteralPath $HostPath -Destination $hostCopy
    $missing = Start-Process -FilePath $hostCopy -ArgumentList '--open-settings' -WindowStyle Hidden -Wait -PassThru
    if ($missing.ExitCode -ne 1) { throw 'A missing settings executable must report failure.' }
    Copy-Item -LiteralPath $FixturePath -Destination (Join-Path $testDirectory 'UmbrellaShell.exe')
    $launched = Start-Process -FilePath $hostCopy -ArgumentList '--open-settings' -WindowStyle Hidden -Wait -PassThru
    if ($launched.ExitCode -ne 0) { throw 'Settings command was not launched.' }
    $marker = Join-Path $testDirectory 'settings-invoked.txt'
    $timeout = [Diagnostics.Stopwatch]::StartNew()
    while (!(Test-Path -LiteralPath $marker) -and $timeout.ElapsedMilliseconds -lt 3000) { Start-Sleep -Milliseconds 25 }
    if (!(Test-Path -LiteralPath $marker) -or [IO.File]::ReadAllText($marker) -ne '--settings') { throw 'Settings argument was not delivered.' }
    $windowFixture = Start-Process -FilePath (Join-Path $testDirectory 'UmbrellaShell.exe') -ArgumentList '--window-fixture' -WindowStyle Hidden -PassThru
    Wait-Marker 'settings-ready.txt'
    $reopen = Start-Process -FilePath $hostCopy -ArgumentList '--open-settings' -WindowStyle Hidden -Wait -PassThru
    if ($reopen.ExitCode -ne 0) { throw 'Existing-window reopen failed.' }
    Wait-Marker 'settings-reopened.txt'
    if ($windowFixture.HasExited) { throw 'Reopen unexpectedly terminated the settings owner.' }
    $quit = Start-Process -FilePath $hostCopy -ArgumentList '--quit-settings' -WindowStyle Hidden -Wait -PassThru
    if ($quit.ExitCode -ne 0 -or !$windowFixture.WaitForExit(3000)) { throw 'Explicit settings shutdown failed.' }
    Wait-Marker 'settings-quit.txt'
    $quitAgain = Start-Process -FilePath $hostCopy -ArgumentList '--quit-settings' -WindowStyle Hidden -Wait -PassThru
    if ($quitAgain.ExitCode -ne 0) { throw 'Quitting absent settings must be idempotent.' }
    'Passed: sibling path/arguments, missing-exe error, reopening existing hidden owner, explicit/idempotent quit; no GUI or Explorer changes.'
} finally {
    if ($windowFixture -and !$windowFixture.HasExited) { Stop-Process -Id $windowFixture.Id; $windowFixture.WaitForExit() }
    # Delete only the explicitly created fixture files; no recursive deletion.
    foreach ($name in @('settings-invoked.txt','settings-ready.txt','settings-reopened.txt','settings-quit.txt','UmbrellaShell.exe','UmbrellaMenuHost.exe')) {
        $created = Join-Path $testDirectory $name
        if (Test-Path -LiteralPath $created) { Remove-Item -LiteralPath $created }
    }
    Remove-Item -LiteralPath $testDirectory
}
