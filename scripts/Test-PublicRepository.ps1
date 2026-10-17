[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    # Read the Git index, not working-tree files: check exactly what is staged.
    $files = @(git -c core.quotePath=false ls-files)
    if ($LASTEXITCODE -ne 0 -or !$files.Count) { throw 'Run inside an initialized repository with staged files.' }
    $forbidden = '(?i)(^|/)(AGENTS(?:\.local)?\.md)$|(^|/)(\.agents|\.codex|\.git|\.vs|\.vscode|\.idea|\.cache|node_modules|__pycache__|build(?:-[^/]*)?|stage|dist|tmp|temp)/|\.(pfx|p12|pem|key|jks|keystore|exe|dll|obj|pdb|log|tmp)$|(^|/)(\.env(?:\..*)?|credentials\.json|secrets\.json|CMakeUserPresets\.json)$'
    $patterns = @{
        'private-key material' = '-----BEGIN (?:RSA |EC |OPENSSH |ENCRYPTED )?PRIVATE KEY-----'
        'GitHub token' = '(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,})'
        'AWS access key' = '(?:AKIA|ASIA)[A-Z0-9]{16}'
        'personal Windows home path' = '[A-Za-z]:[\\/](?:Users|Documents and Settings)[\\/]'
        'personal Unix home path' = '(?<![A-Za-z0-9])/(?:home|Users)/[A-Za-z0-9_.-]+/'
    }
    foreach ($file in $files) {
        if ($file -match $forbidden -and $file -notmatch '(^|/)\.env\.example$') { throw "Forbidden public path: $file" }
        $content = git show ":$file"
        if ($LASTEXITCODE -ne 0) { throw "Cannot inspect index entry: $file" }
        $text = $content -join "`n"
        foreach ($label in $patterns.Keys) {
            if ($text -match $patterns[$label]) { throw "Potential $label in $file (content withheld)." }
        }
    }
    git check-ignore --no-index --quiet AGENTS.md
    if ($LASTEXITCODE -ne 0) { throw 'AGENTS.md must remain ignored.' }
    foreach ($required in @('CHANGELOG.md', 'README.md', '.gitignore', '.github/workflows/ci.yml', '.github/workflows/release.yml')) {
        if ($required -notin $files) { throw "Missing public file: $required" }
    }
    Write-Output "Public index audit passed for $($files.Count) files. AGENTS.md is ignored and absent from the index. Pattern scans do not replace manual review or GitHub secret scanning."
} finally { Pop-Location }
