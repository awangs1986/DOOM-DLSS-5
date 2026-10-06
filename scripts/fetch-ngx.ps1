# Copyright (C) 2026 Nikolai Zhivotenko. GPLv2; see LICENSE.TXT.
# Installs only the pinned official Windows x64 SDK and Release SR/RR runtimes.
[CmdletBinding()]
param(
    [string]$Root,
    [string]$CacheDirectory,
    [switch]$Offline,
    [string]$RestoreBackup
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2
if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = [IO.Path]::GetFullPath($Root)
if (-not $CacheDirectory) { $CacheDirectory = Join-Path $Root 'build-win/_ngx_fetch' }
$CacheDirectory = [IO.Path]::GetFullPath($CacheDirectory)
$Release = Get-Content (Join-Path $PSScriptRoot 'ngx-release.json') -Raw | ConvertFrom-Json
$Dest = Join-Path $Root 'third_party/ngx'
$ModeNames = @('windoom-ngx-dlss3.5', 'windoom-ngx-dlss4', 'windoom-ngx-dlss4.5', 'windoom-ngx-dlss5')
$Lock = $null
$Transaction = $null

function Write-Json($Value, [string]$Path) {
    $Value | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $Path -Encoding UTF8
}
function Get-Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Test-Files([string]$Directory) {
    foreach ($f in $Release.files) {
        $path = Join-Path $Directory $f.path
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $false }
        if ((Get-Item -LiteralPath $path).Length -ne $f.size -or (Get-Hash $path) -ne $f.sha256) { return $false }
    }
    return $true
}
function Get-RuntimeInfo([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $reader = New-Object IO.BinaryReader($stream)
    try {
        if ($reader.ReadUInt16() -ne 0x5a4d) { throw "Not a PE DLL: $Path" }
        $stream.Position = 0x3c
        $offset = $reader.ReadUInt32()
        $stream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) { throw "Not Windows x64: $Path" }
    } finally { $reader.Dispose() }
    $expected = $Release.files | Where-Object { [IO.Path]::GetFileName($_.path) -eq [IO.Path]::GetFileName($Path) }
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($Path)
    $version = $expected.fileVersion
    $versionCheck = 'pinned-sha256 (file-resource inspection unavailable)'
    if ($info.FileVersion) {
        $actual = '{0}.{1}.{2}.{3}' -f $info.FileMajorPart, $info.FileMinorPart, $info.FileBuildPart, $info.FilePrivatePart
        if ($actual -ne $version) { throw "Unexpected file version: $actual" }
        $versionCheck = 'PE file resource verified'
    }
    # The upstream pin fixes identity even when Authenticode is unavailable.
    $signature = 'unavailable-on-this-platform'
    if (Get-Command Get-AuthenticodeSignature -ErrorAction SilentlyContinue) {
        $sig = Get-AuthenticodeSignature -LiteralPath $Path
        $signature = [string]$sig.Status
        if ($sig.Status -ne 'Valid') { throw "Authenticode validation failed ($signature): $Path" }
        if ($sig.SignerCertificate.Subject -notmatch 'NVIDIA') { throw "Unexpected DLL signer: $Path" }
    }
    return [ordered]@{ name = [IO.Path]::GetFileName($Path); architecture = 'x64'; configuration = 'Release'; fileVersion = $version; versionCheck = $versionCheck; sha256 = Get-Hash $Path; checksum = 'verified'; authenticode = $signature }
}
function Get-File([string]$Path, [string]$Url) {
    $partial = "$Path.partial"
    try {
        $curl = Get-Command curl.exe -ErrorAction SilentlyContinue
        if ($curl) {
            & $curl.Source --fail --silent --show-error --location --retry 2 --connect-timeout 20 --max-time 300 --output $partial $Url
            if ($LASTEXITCODE -ne 0) { throw "Download failed: $Url" }
        } else {
            Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $partial -TimeoutSec 300
        }
        Move-Item -LiteralPath $partial -Destination $Path -Force
    } finally {
        if (Test-Path -LiteralPath $partial) { Remove-Item -LiteralPath $partial -Force }
    }
}
function Assert-Unlocked([string]$Path) {
    if (Test-Path -LiteralPath $Path -PathType Container) {
        foreach ($file in Get-ChildItem -LiteralPath $Path -File -Recurse) { Assert-Unlocked $file.FullName }
    } elseif (Test-Path -LiteralPath $Path -PathType Leaf) {
        # No forced overwrite: sharing violations abort before publication.
        $handle = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        $handle.Dispose()
    }
}
function Commit-Entries($Entries, [string]$Transaction) {
    $backup = Join-Path $CacheDirectory ('backups/' + [IO.Path]::GetFileName($Transaction))
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    $journal = @()
    foreach ($entry in $Entries) {
        $target = Join-Path $Root $entry.relative
        Assert-Unlocked $target
        $journal += [ordered]@{ relative = $entry.relative; existed = (Test-Path -LiteralPath $target); sha256 = $null; inventory = @() }
    }
    # All rollback material exists before the first target is changed.
    foreach ($item in $journal) {
        if ($item.existed) {
            $old = Join-Path $backup ('old/' + $item.relative)
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $old) | Out-Null
            Copy-Item -LiteralPath (Join-Path $Root $item.relative) -Destination $old -Recurse
            if (Test-Path -LiteralPath $old -PathType Leaf) { $item.sha256 = Get-Hash $old }
            else {
                foreach ($file in Get-ChildItem -LiteralPath $old -File -Recurse) {
                    $relative = $file.FullName.Substring($old.Length + 1).Replace('\', '/')
                    $item.inventory += [ordered]@{ path = $relative; sha256 = Get-Hash $file.FullName }
                }
            }
        }
    }
    Write-Json ([ordered]@{ schemaVersion = 1; sdkTag = $Release.sdkTag; createdUtc = [DateTime]::UtcNow.ToString('o'); entries = $journal; completed = $false }) (Join-Path $backup 'backup.json')
    $changed = @()
    try {
        for ($i = 0; $i -lt $Entries.Count; ++$i) {
            $entry = $Entries[$i]
            $target = Join-Path $Root $entry.relative
            $held = Join-Path $Transaction ('held/' + $entry.relative)
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $held) | Out-Null
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
            if (Test-Path -LiteralPath $target) { Move-Item -LiteralPath $target -Destination $held }
            $changed += [ordered]@{ target = $target; held = $held }
            if ($entry.source) { Move-Item -LiteralPath $entry.source -Destination $target }
        }
        Write-Json ([ordered]@{ schemaVersion = 1; sdkTag = $Release.sdkTag; createdUtc = [DateTime]::UtcNow.ToString('o'); entries = $journal; completed = $true }) (Join-Path $backup 'backup.json')
        Write-Host "Published; backup: $([IO.Path]::GetFileName($backup))"
    } catch {
        for ($i = $changed.Count - 1; $i -ge 0; --$i) {
            $item = $changed[$i]
            if (Test-Path -LiteralPath $item.target) { Remove-Item -LiteralPath $item.target -Recurse -Force }
            if (Test-Path -LiteralPath $item.held) { Move-Item -LiteralPath $item.held -Destination $item.target }
        }
        throw
    }
}

try {
    New-Item -ItemType Directory -Path $CacheDirectory -Force | Out-Null
    # Serialize fetch/build/deploy operations in this checkout.
    $lockDirectory = Join-Path $Root 'build-win/_ngx_fetch'
    New-Item -ItemType Directory -Force -Path $lockDirectory | Out-Null
    $Lock = [IO.File]::Open((Join-Path $lockDirectory 'install.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $Transaction = Join-Path $CacheDirectory ('transaction-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $Transaction | Out-Null
    $entries = @()
    if ($RestoreBackup) {
        if ($RestoreBackup -notmatch '^transaction-[a-f0-9]{32}$') { throw 'Use the backup identifier printed by a successful install.' }
        $backup = Join-Path $CacheDirectory ('backups/' + $RestoreBackup)
        $record = Get-Content -LiteralPath (Join-Path $backup 'backup.json') -Raw | ConvertFrom-Json
        if (-not $record.completed) { throw 'Backup is from an incomplete install.' }
        foreach ($item in $record.entries) {
            # Backup metadata is local data; keep its paths inside managed targets.
            if ($item.relative -notmatch '^(third_party/ngx|build-win/Release/windoom-ngx-dlss(3\.5|4|4\.5|5)/(nvngx_dlss(d)?\.dll|ngx-install\.json))$') { throw 'Invalid backup path.' }
            $source = $null
            if ($item.existed) {
                $old = Join-Path $backup ('old/' + $item.relative)
                if (-not (Test-Path -LiteralPath $old)) { throw "Backup missing: $($item.relative)" }
                if ($item.sha256 -and (Get-Hash $old) -ne $item.sha256) { throw "Backup corrupted: $($item.relative)" }
                if ($item.inventory.Count -and @(Get-ChildItem -LiteralPath $old -File -Recurse).Count -ne $item.inventory.Count) { throw 'Backup SDK inventory changed.' }
                foreach ($file in $item.inventory) {
                    if ($file.path -match '(^[/\\]|(^|[/\\])\.\.([/\\]|$)|:)') { throw 'Invalid backup inventory path.' }
                    $check = Join-Path $old $file.path
                    if (-not (Test-Path -LiteralPath $check -PathType Leaf) -or (Get-Hash $check) -ne $file.sha256) { throw "Backup SDK file corrupted: $($file.path)" }
                }
                $source = Join-Path $Transaction ('new/' + $item.relative)
                New-Item -ItemType Directory -Force -Path (Split-Path -Parent $source) | Out-Null
                Copy-Item -LiteralPath $old -Destination $source -Recurse
            }
            $entries += [ordered]@{ relative = $item.relative; source = $source }
        }
        Commit-Entries $entries $Transaction
        Write-Host 'Restored SDK, libraries and deployed runtimes as one snapshot.'
    } else {
        Write-Host "WinDoom: official NVIDIA SDK $($Release.sdkTag), commit $($Release.commit)"
        $cache = Join-Path $CacheDirectory $Release.commit
        New-Item -ItemType Directory -Force -Path $cache | Out-Null
        $reuse = $false
        if (Test-Path -LiteralPath (Join-Path $Dest 'ngx-install.json')) {
            try {
                $installed = Get-Content -LiteralPath (Join-Path $Dest 'ngx-install.json') -Raw | ConvertFrom-Json
                $reuse = ($installed.commit -eq $Release.commit) -and ($installed.sdkTag -eq $Release.sdkTag) -and
                    ($installed.source -eq $Release.source) -and ($installed.architecture -eq $Release.architecture) -and
                    ($installed.configuration -eq $Release.configuration) -and
                    (($installed.files | ConvertTo-Json -Depth 4 -Compress) -eq ($Release.files | ConvertTo-Json -Depth 4 -Compress)) -and (Test-Files $Dest)
            } catch { Write-Host 'Invalid installation metadata; staging a complete replacement.' }
        }
        if ($reuse) { $sdk = $Dest; Write-Host 'Verified installed SDK; reusing complete installation.' }
        else {
            foreach ($file in $Release.files) {
                $path = Join-Path $cache $file.path
                New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
                $valid = (Test-Path -LiteralPath $path -PathType Leaf) -and ((Get-Hash $path) -eq $file.sha256)
                if (-not $valid) {
                    if ($Offline) { throw "Offline cache missing or corrupt: $($file.path)" }
                    Get-File $path "https://raw.githubusercontent.com/NVIDIA/DLSS/$($Release.commit)/$($file.path)"
                    if ((Get-Hash $path) -ne $file.sha256) { throw "SHA256 mismatch: $($file.path)" }
                }
            }
            if (-not (Test-Files $cache)) { throw 'Incomplete or corrupted SDK cache.' }
            $sdk = Join-Path $Transaction 'sdk'
            Copy-Item -LiteralPath $cache -Destination $sdk -Recurse
            $entries += [ordered]@{ relative = 'third_party/ngx'; source = $sdk }
        }
        $sr = Get-RuntimeInfo (Join-Path $sdk 'lib/Windows_x86_64/rel/nvngx_dlss.dll')
        $rr = Get-RuntimeInfo (Join-Path $sdk 'lib/Windows_x86_64/rel/nvngx_dlssd.dll')
        $metadata = [ordered]@{ schemaVersion = 1; sdkTag = $Release.sdkTag; commit = $Release.commit; source = $Release.source; releaseUrl = $Release.releaseUrl; architecture = $Release.architecture; configuration = $Release.configuration; verifiedUtc = $Release.verifiedUtc; runtimes = @($sr, $rr); files = $Release.files }
        if (-not $reuse) { Write-Json $metadata (Join-Path $sdk 'ngx-install.json') }
        foreach ($name in $ModeNames) {
            $relative = 'build-win/Release/' + $name
            $directory = Join-Path $Root $relative
            if (-not (Test-Path -LiteralPath $directory)) { continue }
            $runtimeSet = @($sr)
            $staleRR = $null
            if ($name -eq 'windoom-ngx-dlss3.5') { $runtimeSet += $rr }
            elseif (Test-Path -LiteralPath (Join-Path $directory 'nvngx_dlssd.dll') -PathType Leaf) {
                # The legacy fetcher copied RR into all NGX modes. Remove only
                # this managed obsolete runtime, through the backup transaction.
                $staleRR = $relative + '/nvngx_dlssd.dll'
            }
            $modeMetadata = [ordered]@{ schemaVersion = 1; sdkTag = $Release.sdkTag; commit = $Release.commit; source = $Release.source; architecture = 'Windows x64'; configuration = 'Release'; runtimes = $runtimeSet }
            $changed = ($null -ne $staleRR) -or (-not $reuse) -or (-not (Test-Path -LiteralPath (Join-Path $directory 'ngx-install.json')))
            foreach ($runtime in $runtimeSet) {
                $target = Join-Path $directory $runtime.name
                if (-not (Test-Path -LiteralPath $target) -or (Get-Hash $target) -ne $runtime.sha256) { $changed = $true }
            }
            if (-not $changed) {
                try {
                    $m = Get-Content -LiteralPath (Join-Path $directory 'ngx-install.json') -Raw | ConvertFrom-Json
                    if (($m | ConvertTo-Json -Depth 12 -Compress) -ne ($modeMetadata | ConvertTo-Json -Depth 12 -Compress)) { $changed = $true }
                } catch { $changed = $true }
            }
            if (-not $changed) { Write-Host "Verified deployed runtimes: $name"; continue }
            if ($staleRR) { $entries += [ordered]@{ relative = $staleRR; source = $null } }
            foreach ($runtime in $runtimeSet) {
                $source = Join-Path $Transaction ($name + '/' + $runtime.name)
                New-Item -ItemType Directory -Force -Path (Split-Path -Parent $source) | Out-Null
                Copy-Item -LiteralPath (Join-Path $sdk ('lib/Windows_x86_64/rel/' + $runtime.name)) -Destination $source
                if ((Get-Hash $source) -ne $runtime.sha256) { throw 'Staged runtime checksum mismatch.' }
                $entries += [ordered]@{ relative = $relative + '/' + $runtime.name; source = $source }
            }
            $source = Join-Path $Transaction ($name + '/ngx-install.json')
            Write-Json $modeMetadata $source
            $entries += [ordered]@{ relative = $relative + '/ngx-install.json'; source = $source }
        }
        if ($entries.Count) { Commit-Entries $entries $Transaction }
        else { Write-Host 'Complete installation and deployed runtimes verified; no changes.' }
        foreach ($runtime in @($sr,$rr)) { Write-Host "$($runtime.name): version=$($runtime.fileVersion), x64 Release, SHA256=$($runtime.sha256), Authenticode=$($runtime.authenticode)" }
    }
} catch {
    Write-Error "NGX installation aborted; existing installation preserved. $($_.Exception.Message)"
    exit 1
} finally {
    if ($Lock) { $Lock.Dispose() }
    if ($Transaction -and (Test-Path -LiteralPath $Transaction)) { Remove-Item -LiteralPath $Transaction -Recurse -Force }
}
