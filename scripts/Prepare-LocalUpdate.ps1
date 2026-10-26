[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$RuntimeBase,
    [Parameter(Mandatory)][string]$Output
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$project = Split-Path $PSScriptRoot -Parent
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$base = (Resolve-Path -LiteralPath $RuntimeBase).Path
$destination = [IO.Path]::GetFullPath($Output)
if (!$destination.StartsWith($project + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Prepare inside this workspace.' }
if (Test-Path -LiteralPath $destination) { throw 'Output must be absent.' }
$products = @('UmbrellaShell.exe','UmbrellaTaskbarHost.exe','UmbrellaTaskbarModule.dll','UmbrellaMenuHost.exe','UmbrellaMenuModule.dll')
foreach ($product in $products) { if (!(Test-Path -LiteralPath (Join-Path $build $product))) { throw "Missing $product" } }
$runtime = Join-Path $destination 'runtime'
$null = New-Item -ItemType Directory -Path $runtime
foreach ($file in Get-ChildItem -LiteralPath $base -Recurse -File) {
    $relative = [IO.Path]::GetRelativePath($base, $file.FullName)
    if ($relative -match '(^|[\\/])AGENTS(?:\.local)?\.md$|(^|[\\/])(?:\.codex|\.agents)[\\/]') { throw 'Local instructions in runtime base.' }
    if ($relative -in $products) { continue }
    $target = Join-Path $runtime $relative
    $null = New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force
    Copy-Item -LiteralPath $file.FullName -Destination $target
}
foreach ($product in $products) { Copy-Item -LiteralPath (Join-Path $build $product) -Destination (Join-Path $runtime $product) }
foreach ($file in @('README.md','CHANGELOG.md','docs/ARCHITECTURE.md','docs/SETTINGS_LIFETIME.md','docs/STARTUP_RECOVERY.md','docs/VERIFICATION.md')) {
    $source = Join-Path $project $file
    if (!(Test-Path -LiteralPath $source)) { continue }
    $target = Join-Path $runtime $file
    $null = New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force
    Copy-Item -LiteralPath $source -Destination $target
}
$inventory = @(Get-ChildItem -LiteralPath $runtime -Recurse -File | Where-Object { $_.Extension -in @('.exe','.dll') } | Sort-Object FullName | ForEach-Object {
    @{ path = [IO.Path]::GetRelativePath($runtime,$_.FullName).Replace('\','/'); sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(); product = $_.VersionInfo.ProductName; fileVersion = $_.VersionInfo.FileVersion }
})
$versionSource = Get-Content -LiteralPath (Join-Path $project 'CMakeLists.txt') -Raw
if ($versionSource -notmatch 'project\(UmbrellaShell VERSION (\d+\.\d+\.\d+)') { throw 'Missing project version.' }
@{ version = $Matches[1]; signed = $false; buildKind = 'local-msvc-qt'; files = $inventory } | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath (Join-Path $runtime 'docs/runtime-inventory.json') -Encoding utf8
$files = @(Get-ChildItem -LiteralPath $runtime -Recurse -File | Sort-Object FullName | ForEach-Object {
    @{ path = [IO.Path]::GetRelativePath($runtime,$_.FullName); sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
})
@{ format = 1; signed = $false; files = $files } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $destination 'manifest.json') -Encoding utf8
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Install-PreparedUpdate.ps1') -Destination $destination
Write-Output "Prepared $destination. The installed application and user settings were not changed."
