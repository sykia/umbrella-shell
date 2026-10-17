[CmdletBinding()]
param([switch]$Signed)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = Split-Path $PSScriptRoot -Parent
function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
Push-Location $projectRoot
try {
    Invoke-Checked cmake @('--preset', 'windows-msvc')
    Invoke-Checked cmake @('--build', '--preset', 'release')
    Invoke-Checked ctest @('--preset', 'release')
    $stage = Join-Path $projectRoot 'stage'
    # Never reuse a deployment directory: stale DLLs must not enter a package.
    if (Test-Path -LiteralPath $stage) { throw 'stage already exists; use a clean checkout or move it aside.' }
    Invoke-Checked cmake @('--install', 'build/windows-msvc', '--config', 'Release', '--prefix', $stage)
    $versionFile = Get-Content 'CMakeLists.txt' -Raw
    if ($versionFile -notmatch 'project\(UmbrellaShell VERSION (\d+\.\d+\.\d+)') { throw 'Missing version' }
    $version = $Matches[1]
    $dist = Join-Path $projectRoot 'dist'
    New-Item -ItemType Directory -Path $dist -Force | Out-Null
    $compiler = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    $iscc = if ($compiler) { $compiler.Source } else { Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe' }
    $arguments = @("/DAppVersion=$version", "/DStageDir=$stage", "/DOutputDir=$dist")
    if ($Signed) {
        if (!$env:UMBRELLA_SIGN_CERT_SHA1) { throw 'Signing certificate thumbprint is required' }
        $signTool = (Get-Command signtool.exe -ErrorAction Stop).Source
        Invoke-Checked $signTool @('sign', '/sha1', $env:UMBRELLA_SIGN_CERT_SHA1, '/fd', 'SHA256', '/tr', 'http://timestamp.digicert.com', '/td', 'SHA256', (Join-Path $stage 'UmbrellaShell.exe'))
        $signCommand = '"' + $signTool + '" sign /sha1 ' + $env:UMBRELLA_SIGN_CERT_SHA1 + ' /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 $q$f$q'
        $arguments += @('/DSignedRelease', "/Srelease=$signCommand")
    }
    $arguments += 'packaging/windows/UmbrellaShell.iss'
    Invoke-Checked $iscc $arguments
    $installer = Join-Path $dist "UmbrellaShell-$version-windows-x64-setup.exe"
    if ($Signed) {
        Invoke-Checked $signTool @('verify', '/pa', '/all', '/v', $installer)
        Invoke-Checked $signTool @('verify', '/pa', '/all', '/v', (Join-Path $stage 'UmbrellaShell.exe'))
    }
    $hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $([IO.Path]::GetFileName($installer))" | Set-Content -LiteralPath "$installer.sha256" -Encoding ascii
} finally { Pop-Location }
