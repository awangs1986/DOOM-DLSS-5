# Behavioral tests through the same fetch entry point as fetch-ngx.cmd/build.cmd.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$SeedCache)
$ErrorActionPreference = 'Stop'
$release = Get-Content (Join-Path $PSScriptRoot 'ngx-release.json') -Raw | ConvertFrom-Json
$executable = (Get-Process -Id $PID).Path
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('windoom-ngx-test-' + [Guid]::NewGuid().ToString('N'))
$cache = Join-Path $fixture 'build-win/_ngx_fetch'
New-Item -ItemType Directory -Path $cache -Force | Out-Null
Copy-Item -LiteralPath $SeedCache -Destination (Join-Path $cache $release.commit) -Recurse
function Assert($condition, $message) { if (-not $condition) { throw $message } }
function Run-Fetch([string[]]$Arguments, [bool]$Success = $true) {
    $savedAction = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $executable -NoProfile -File (Join-Path $PSScriptRoot 'fetch-ngx.ps1') -Root $fixture @Arguments 2>&1
        $status = $LASTEXITCODE
    } finally { $ErrorActionPreference = $savedAction }
    $output | Out-File -FilePath (Join-Path $fixture 'transcript.txt') -Append
    Assert (($status -eq 0) -eq $Success) "Unexpected fetch exit code $status. $output"
    return ($output -join "`n")
}
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }
function Snapshot {
    $items = @()
    foreach ($sub in @('third_party/ngx','build-win/Release')) {
        $path = Join-Path $fixture $sub
        if (Test-Path -LiteralPath $path) {
            foreach ($file in Get-ChildItem -LiteralPath $path -File -Recurse | Sort-Object FullName) {
                $items += $file.FullName.Substring($fixture.Length) + ':' + (Hash $file.FullName)
            }
        }
    }
    return ($items -join "`n")
}
try {
    $null = Run-Fetch @('-Offline')
    $sdk = Join-Path $fixture 'third_party/ngx'
    $meta = Get-Content (Join-Path $sdk 'ngx-install.json') -Raw | ConvertFrom-Json
    Assert ($meta.commit -eq $release.commit) 'First install did not pin the commit.'
    Assert ($meta.runtimes[0].fileVersion -eq '310.9.1.0') 'SDK and DLL versions were confused.'
    $backups = @(Get-ChildItem (Join-Path $cache 'backups') -Directory)
    $null = Run-Fetch @('-Offline')
    Assert (@(Get-ChildItem (Join-Path $cache 'backups') -Directory).Count -eq $backups.Count) 'Complete installed cache was not reused.'
    Write-Host 'PASS first install and same-version verified reuse'

    # Restore the first-install snapshot: no previously installed SDK means uninstall.
    $null = Run-Fetch @('-RestoreBackup', $backups[0].Name)
    Assert (-not (Test-Path -LiteralPath $sdk)) 'Restore did not recover pre-install state.'
    New-Item -ItemType Directory -Path (Join-Path $sdk 'include') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $sdk 'lib') -Force | Out-Null
    Set-Content (Join-Path $sdk 'include/nvsdk_ngx.h') 'old-header'
    Set-Content (Join-Path $sdk 'lib/old.lib') 'old-library'
    foreach ($name in @('windoom-ngx-dlss3.5','windoom-ngx-dlss4','windoom-ngx-dlss4.5','windoom-ngx-dlss5','windoom-original','windoom-anime4k','windoom-fsr2')) {
        $directory = Join-Path $fixture ('build-win/Release/' + $name)
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
        Set-Content (Join-Path $directory 'windoom.exe') 'unchanged executable'
        if ($name -like 'windoom-ngx-*') { Set-Content (Join-Path $directory 'nvngx_dlss.dll') 'old-SR' }
        # The original installer copied both SR and RR into every NGX folder.
        if ($name -like 'windoom-ngx-*') { Set-Content (Join-Path $directory 'nvngx_dlssd.dll') 'old-RR' }
        if ($name -eq 'windoom-ngx-dlss5') {
            Set-Content (Join-Path $directory 'nvngx_dlssg.dll') 'external-frame-generation'
            Set-Content (Join-Path $directory 'dxgi.dll') 'external-injector'
            Set-Content (Join-Path $directory 'renodx-dlss5.addon64') 'external-addon'
        }
    }
    $oldSnapshot = Snapshot
    $null = Run-Fetch @('-Offline')
    foreach ($name in @('windoom-ngx-dlss3.5','windoom-ngx-dlss4','windoom-ngx-dlss4.5','windoom-ngx-dlss5')) {
        $directory = Join-Path $fixture ('build-win/Release/' + $name)
        $dll = Join-Path $directory 'nvngx_dlss.dll'
        Assert ((Hash $dll).ToLowerInvariant() -eq $meta.runtimes[0].sha256) "Wrong SR deployed: $name"
        Assert ((Get-Content (Join-Path $directory 'windoom.exe') -Raw).Trim() -eq 'unchanged executable') 'Executable modified.'
        if ($name -ne 'windoom-ngx-dlss3.5') { Assert (-not (Test-Path (Join-Path $directory 'nvngx_dlssd.dll'))) 'RR leaked into SR-only mode.' }
        if ($name -eq 'windoom-ngx-dlss5') {
            Assert ((Get-Content (Join-Path $directory 'nvngx_dlssg.dll') -Raw).Trim() -eq 'external-frame-generation') 'External frame generation DLL changed.'
            Assert ((Get-Content (Join-Path $directory 'dxgi.dll') -Raw).Trim() -eq 'external-injector') 'External injector changed.'
            Assert ((Get-Content (Join-Path $directory 'renodx-dlss5.addon64') -Raw).Trim() -eq 'external-addon') 'External addon changed.'
        } else { Assert (-not (Test-Path (Join-Path $directory 'nvngx_dlssg.dll'))) 'Frame generation DLL deployed.' }
    }
    foreach ($name in @('windoom-original','windoom-anime4k','windoom-fsr2')) {
        Assert (@(Get-ChildItem (Join-Path $fixture ('build-win/Release/' + $name)) -File).Count -eq 1) 'Other mode modified.'
    }
    $backup = Get-ChildItem (Join-Path $cache 'backups') -Directory | Where-Object { (Get-Content (Join-Path $_.FullName 'backup.json') -Raw | ConvertFrom-Json).entries.Count -gt 1 } | Select-Object -First 1
    $activeSnapshot = Snapshot
    $backupCount = @(Get-ChildItem (Join-Path $cache 'backups') -Directory).Count
    $null = Run-Fetch @('-Offline')
    Assert ((Snapshot) -eq $activeSnapshot) 'Complete deployment changed on reuse.'
    Assert (@(Get-ChildItem (Join-Path $cache 'backups') -Directory).Count -eq $backupCount) 'Idempotent deployment created a backup.'
    # A complete latest SDK/deployment must still detect a stale RR copied in
    # later, rather than returning early because only the SR hash matches.
    $cachedStaleRR = Join-Path $fixture 'build-win/Release/windoom-ngx-dlss5/nvngx_dlssd.dll'
    Set-Content $cachedStaleRR 'late-stale-RR'
    $null = Run-Fetch @('-Offline')
    Assert (-not (Test-Path -LiteralPath $cachedStaleRR)) 'Stale RR survived verified installed-SDK reuse.'
    Assert ((Snapshot) -eq $activeSnapshot) 'Stale RR cleanup changed the active SDK or third-party files.'
    $null = Run-Fetch @('-RestoreBackup', $backup.Name)
    Assert ((Snapshot) -eq $oldSnapshot) 'SDK, libraries and runtimes not restored consistently.'
    Write-Host 'PASS legacy all-folder RR cleanup, feature-specific deployment, third-party preservation and consistent restore'

    # A broken cache must never remove the usable legacy installation.
    $cacheHeader = Join-Path $cache ($release.commit + '/include/nvsdk_ngx.h')
    Set-Content $cacheHeader 'corrupt-cache'
    $null = Run-Fetch @('-Offline') $false
    Assert ((Snapshot) -eq $oldSnapshot) 'Corrupted cache destroyed old installation.'
    Write-Host 'PASS corrupt-cache refusal preserves legacy SDK, libraries and DLLs'

    # Network interruption through a dead HTTPS proxy exercises actual downloader failure.
    $savedProxy = $env:HTTPS_PROXY
    $savedNoProxy = $env:NO_PROXY
    try {
        $env:HTTPS_PROXY = 'http://127.0.0.1:1'
        $env:NO_PROXY = ''
        $null = Run-Fetch @() $false
    } finally { $env:HTTPS_PROXY = $savedProxy; $env:NO_PROXY = $savedNoProxy }
    Assert ((Snapshot) -eq $oldSnapshot) 'Download failure destroyed old installation.'
    Assert (@(Get-ChildItem $cache -Filter '*.partial' -Recurse).Count -eq 0) 'Interrupted download left a reusable partial file.'
    Write-Host 'PASS failed download leaves old installation intact and discards partial data'

    Copy-Item (Join-Path $SeedCache 'include/nvsdk_ngx.h') $cacheHeader -Force
    foreach ($runtimeName in @('nvngx_dlss.dll', 'nvngx_dlssd.dll')) {
        $locked = Join-Path $fixture ('build-win/Release/windoom-ngx-dlss4/' + $runtimeName)
        $handle = [IO.File]::Open($locked, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::None)
        try { $null = Run-Fetch @('-Offline') $false }
        finally { $handle.Dispose() }
        Assert ((Snapshot) -eq $oldSnapshot) "Locked $runtimeName changed the installation."
    }
    Write-Host 'PASS in-use SR and obsolete RR refuse publication without modifying old installation'

    if ($env:OS -ne 'Windows_NT') {
        # A parent-directory permission error occurs after SDK publication, so
        # this observes real rollback rather than only validation/preflight.
        $blockedDirectory = Join-Path $fixture 'build-win/Release/windoom-ngx-dlss4'
        & chmod a-w $blockedDirectory
        try { $null = Run-Fetch @('-Offline') $false }
        finally { & chmod u+w $blockedDirectory }
        Assert ((Snapshot) -eq $oldSnapshot) 'Publication error did not roll back SDK and deployed runtimes.'
        Write-Host 'PASS mid-publication failure rolls back every touched target'
    }

    $oldBackupHeader = Join-Path $backup.FullName 'old/third_party/ngx/include/nvsdk_ngx.h'
    Set-Content $oldBackupHeader 'corrupt-backup'
    $null = Run-Fetch @('-RestoreBackup', $backup.Name) $false
    Assert ((Snapshot) -eq $oldSnapshot) 'Corrupt backup changed the installation.'
    Write-Host 'PASS corrupted SDK backup cannot be restored'
    Write-Host "Evidence: $fixture/transcript.txt"
} catch {
    Write-Host "Failure evidence: $fixture/transcript.txt"
    throw
}
