# Windows PowerShell 5.1; GPLv2, see LICENSE.TXT.
[CmdletBinding()]
param(
    [string]$SourceRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$OutputRoot = (Join-Path $env:TEMP ('windoom-baseline-' + [guid]::NewGuid().ToString('N'))),
    [string]$CMake,
    [string]$SourceRevision,
    [string]$Iwad,
    [string]$Demo,
    [string]$SrExecutable,
    [switch]$ProbeOnly,
    [switch]$BuildOnly,
    [switch]$InteractiveDesktopConfirmed,
    [ValidateRange(1, 86400)][int]$TimeoutSeconds = 600,
    [ValidateRange(0, 1000000)][int[]]$KeyFrames = @(1, 35, 70, 175),
    [ValidateRange(0, 1000000)][int[]]$KeyGameTics = @(35, 70, 175)
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
if (Test-Path -LiteralPath $OutputRoot) { throw 'OutputRoot must be a new directory; existing evidence is never replaced.' }
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
$manifest = [ordered]@{
    schema = 1; utc = [DateTime]::UtcNow.ToString('o'); source = $SourceRoot
    ssh = 'not established by this script; retain runner CLI transcript separately'
    build = 'not attempted'; launch = 'not attempted'; visual = 'unverified'
    interactiveDesktopConfirmed = $InteractiveDesktopConfirmed.IsPresent
    gpuPhaseTiming = 'opt-in D3D12 timestamps; verify gpu-timing.csv and log availability per run; excludes CPU/PNG encoding and Present'
    telemetry = 'nvidia-smi device-level samples, if supported; includes other processes and is not per-stage timing'
    runs = @(); blockers = @()
}
function Save-Manifest { $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputRoot 'manifest.json') -Encoding UTF8 }
function Find-CMake {
    if ($CMake) { return (Get-Command $CMake -ErrorAction Stop).Source }
    $found = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($found) { return $found.Source }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        foreach ($vs in @(& $vswhere -all -products '*' -property installationPath)) {
            $candidate = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path -LiteralPath $candidate) { return $candidate }
        }
    }
    throw 'CMake not found. Install Visual Studio C++ and CMake tools or supply -CMake.'
}
function Invoke-BuildCommand([string[]]$Arguments, [string]$Log) {
    @{ executable = $script:cmakeExe; arguments = $Arguments } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath ($Log + '.command.json') -Encoding UTF8
    $commandErrorPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & $script:cmakeExe @Arguments 2>&1 | Tee-Object -FilePath $Log | Out-Host }
    finally { $ErrorActionPreference = $commandErrorPreference }
    if ($LASTEXITCODE -ne 0) { throw "CMake failed ($LASTEXITCODE). See $Log" }
}
try {
    $gpu = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion, AdapterRAM, PNPDeviceID)
    $manifest.gpus = $gpu
    $manifest.os = Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber
    $manifest.processSession = (Get-Process -Id $PID).SessionId
    (& quser.exe 2>&1 | Out-String) | Set-Content -LiteralPath (Join-Path $OutputRoot 'sessions.txt') -Encoding UTF8
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        (& $vswhere -all -products '*' -format json | Out-String) | Set-Content -LiteralPath (Join-Path $OutputRoot 'visual-studio.json') -Encoding UTF8
    }
    $smi = Get-Command nvidia-smi.exe -ErrorAction SilentlyContinue
    if ($smi) {
        (& $smi.Source 2>&1 | Out-String) | Set-Content -LiteralPath (Join-Path $OutputRoot 'nvidia-smi.txt') -Encoding UTF8
    }
    $manifest.instrumentationSources = @{}
    foreach ($relative in @('CMakeLists.txt', 'win32/i_video_win.c', 'win32/gpu_timing.c', 'win32/gpu_timing.h')) {
        $sourceFile = Join-Path $SourceRoot $relative
        if (Test-Path -LiteralPath $sourceFile) { $manifest.instrumentationSources[$relative] = (Get-FileHash -LiteralPath $sourceFile).Hash }
    }
    $manifest.revision = $SourceRevision
    if (!$SourceRevision -and (Test-Path -LiteralPath (Join-Path $SourceRoot '.git'))) {
        $manifest.revision = "$(& git -C $SourceRoot rev-parse HEAD)"
    }
    if (!$manifest.revision) { $manifest.revision = 'unknown; record upload/archive provenance separately' }
    if ($ProbeOnly) { return }
    $script:cmakeExe = Find-CMake
    $manifest.cmake = $script:cmakeExe
    $build = Join-Path $OutputRoot 'build-original'
    Invoke-BuildCommand @('-S', $SourceRoot, '-B', $build, '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DWINDOOM_NGX=OFF', '-DWINDOOM_FSR2=OFF', '-DWINDOOM_ANIME4K=OFF') (Join-Path $OutputRoot 'configure.log')
    Invoke-BuildCommand @('--build', $build, '--config', 'Release', '--parallel', '2') (Join-Path $OutputRoot 'build.log')
    $original = Join-Path $build 'Release\windoom.exe'
    if (!(Test-Path -LiteralPath $original)) { throw 'Build completed without expected windoom.exe.' }
    $manifest.build = 'passed: original Release executable exists'
    $manifest.originalSha256 = (Get-FileHash -LiteralPath $original -Algorithm SHA256).Hash
    if ($BuildOnly) { return }
    if (!$InteractiveDesktopConfirmed) {
        $manifest.blockers += 'No confirmed active interactive desktop. Build/probe evidence cannot establish correct visuals.'
        return
    }
    foreach ($inputFile in @($Iwad, $Demo)) {
        if (!$inputFile -or !(Test-Path -LiteralPath $inputFile -PathType Leaf)) { throw 'Supply existing -Iwad and -Demo files recorded for this baseline.' }
    }
    if ([IO.Path]::GetExtension($Demo) -ne '.lmp') { throw 'Demo must be an external .lmp file matching the IWAD.' }
    $Iwad = (Resolve-Path -LiteralPath $Iwad).Path
    $Demo = (Resolve-Path -LiteralPath $Demo).Path
    $manifest.iwad = @{ name = [IO.Path]::GetFileName($Iwad); sha256 = (Get-FileHash -LiteralPath $Iwad).Hash }
    $manifest.demo = @{ name = [IO.Path]::GetFileName($Demo); sha256 = (Get-FileHash -LiteralPath $Demo).Hash }
    $cases = @(
        @{ name = 'original-color'; exe = $original; view = '-color' },
        @{ name = 'original-depth'; exe = $original; view = '-depth' },
        @{ name = 'original-normal'; exe = $original; view = '-normal' },
        @{ name = 'original-velocity'; exe = $original; view = '-velocity' }
    )
    if ($SrExecutable -and (Test-Path -LiteralPath $SrExecutable -PathType Leaf)) {
        $cases += @{ name = 'sr-color'; exe = (Resolve-Path -LiteralPath $SrExecutable).Path; view = '-color' }
    } else {
        $manifest.blockers += 'SR executable not provided; current SR baseline remains unverified.'
    }
    foreach ($case in $cases) {
        $runDir = Join-Path $OutputRoot $case.name
        New-Item -ItemType Directory -Path $runDir | Out-Null
        Copy-Item -LiteralPath $Demo -Destination (Join-Path $runDir 'baseline.lmp')
        $frames = Join-Path $runDir 'frames'
        $gameArguments = @('-iwad', $Iwad, '-config', (Join-Path $runDir 'default.cfg'), '-playdemo', 'baseline', $case.view, '-export', $frames, '-gpu-timing', (Join-Path $runDir 'gpu-timing.csv'))
        # Windows file paths cannot contain a double quote; quoting every argument
        # also protects spaces without ever passing commands through a shell.
        $quoted = ($gameArguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
        $entry = [ordered]@{ name = $case.name; executable = $case.exe; exeSha256 = (Get-FileHash -LiteralPath $case.exe).Hash; arguments = $gameArguments; launch = 'not attempted'; completed = $false; visual = 'requires human inspection'; frames = @() }
        $manifest.runs += $entry
        $smiProcess = $null
        $process = $null
        try {
            if ($smi) {
                $smiProcess = Start-Process -FilePath $smi.Source -ArgumentList @('--query-gpu=timestamp,index,memory.used,utilization.gpu', '--format=csv', '-lms', '500') -PassThru -NoNewWindow -RedirectStandardOutput (Join-Path $runDir 'gpu-device.csv') -RedirectStandardError (Join-Path $runDir 'gpu-device-errors.txt')
            }
            $entry.startUtc = [DateTime]::UtcNow.ToString('o')
            $process = Start-Process -FilePath $case.exe -ArgumentList $quoted -WorkingDirectory $runDir -PassThru -RedirectStandardOutput (Join-Path $runDir 'stdout.log') -RedirectStandardError (Join-Path $runDir 'stderr.log')
            $processHandle = $process.Handle # cache OS handle before quick demo can exit
            $entry.launch = 'process created; not evidence of renderer initialization'
            if (!$process.WaitForExit($TimeoutSeconds * 1000)) { throw "Demo exceeded $TimeoutSeconds seconds." }
            $process.Refresh()
            $entry.exitCode = $process.ExitCode
            $entry.endUtc = [DateTime]::UtcNow.ToString('o')
            $pngs = @(Get-ChildItem -LiteralPath $frames -Filter 'f*.png' -ErrorAction SilentlyContinue | Sort-Object Name)
            $entry.frameCount = $pngs.Count
            foreach ($number in $KeyFrames) {
                $path = Join-Path $frames ('f{0:D6}.png' -f $number)
                if (Test-Path -LiteralPath $path) {
                    $entry.frames += @{ frame = $number; exportSequenceSeconds = ($number - 1) / 35.0; path = $path; sha256 = (Get-FileHash -LiteralPath $path).Hash }
                }
            }
            $entry.completed = ($entry.exitCode -eq 0 -and $pngs.Count -gt 0)
            $log = Get-Content -LiteralPath (Join-Path $runDir 'stderr.log') -Raw
            $entry.rendererInitialized = ($log -match 'I_InitGraphics: D3D12CreateDevice')
            $entry.presentModes = @([regex]::Matches($log, 'present mode: [^\r\n]+') | ForEach-Object { $_.Value })
            $timingPath = Join-Path $runDir 'gpu-timing.csv'
            $entry.gpuTimingAvailable = $false
            if (Test-Path -LiteralPath $timingPath) {
                $timings = @(Import-Csv -LiteralPath $timingPath)
                $entry.gpuTimingAvailable = ($timings.Count -gt 0)
                $entry.gpuTimingRows = $timings.Count
                $entry.keyGameFrames = @()
                foreach ($tic in $KeyGameTics) {
                    $row = $timings | Where-Object { [int]$_.game_tic -eq $tic } | Select-Object -First 1
                    if ($row) {
                        $png = Join-Path $frames ('f{0:D6}.png' -f [int]$row.frame)
                        if (Test-Path -LiteralPath $png) {
                            $entry.keyGameFrames += @{ gameTic = $tic; frame = [int]$row.frame; path = $png; sha256 = (Get-FileHash -LiteralPath $png).Hash }
                        }
                    }
                }
                foreach ($keyframe in $entry.frames) {
                    $timing = $timings | Where-Object { [int]$_.frame -eq $keyframe.frame } | Select-Object -First 1
                    if ($timing) { $keyframe.gameTic = [int]$timing.game_tic }
                }
            }
            if ($case.name -eq 'sr-color' -and $log -notmatch 'present mode: dlss-(upscale|stack)') {
                $manifest.blockers += 'SR run did not report a DLSS present path; fallback frames cannot count as SR evidence.'
            }
            if (!$entry.completed) { $manifest.blockers += "Run $($case.name) failed or exported no PNGs; inspect stderr." }
        } finally {
            # Only terminate the processes owned by this invocation, never by name.
            if ($process -and !$process.HasExited) { $process.Kill(); $process.WaitForExit() }
            if ($smiProcess -and !$smiProcess.HasExited) { $smiProcess.Kill(); $smiProcess.WaitForExit() }
            Save-Manifest
        }
    }
    $manifest.launch = 'see per-run process, renderer initialization, exit and frame results'
    $manifest.visual = 'unverified: inspect PNG keyframes before assigning a visual verdict'
} catch {
    $manifest.blockers += $_.Exception.Message
    throw
} finally {
    Save-Manifest
    Write-Host "Evidence: $OutputRoot"
}
