# Run an isolated capture on the signed-in Windows user's interactive desktop.
[CmdletBinding()]
param(
    [string]$CaptureScript = (Join-Path $PSScriptRoot 'capture-baseline.ps1'),
    [Parameter(Mandatory = $true)][string]$Iwad,
    [Parameter(Mandatory = $true)][string]$Demo,
    [string]$SrExecutable,
    [string]$SourceRevision,
    [string]$OutputRoot = (Join-Path $env:TEMP ('windoom-interactive-' + [guid]::NewGuid().ToString('N'))),
    [ValidateRange(1, 86400)][int]$TimeoutSeconds = 600
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$CaptureScript = (Resolve-Path -LiteralPath $CaptureScript).Path
$Iwad = (Resolve-Path -LiteralPath $Iwad).Path
$Demo = (Resolve-Path -LiteralPath $Demo).Path
if ($SrExecutable) { $SrExecutable = (Resolve-Path -LiteralPath $SrExecutable).Path }
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
if (Test-Path -LiteralPath $OutputRoot) { throw 'OutputRoot must not already exist.' }
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
$taskName = 'WinDoomBaseline-' + [guid]::NewGuid().ToString('N')
$params = @{ Iwad = $Iwad; Demo = $Demo; OutputRoot = (Join-Path $OutputRoot 'capture'); InteractiveDesktopConfirmed = $true; TimeoutSeconds = $TimeoutSeconds }
if ($SrExecutable) { $params.SrExecutable = $SrExecutable }
if ($SourceRevision) { $params.SourceRevision = $SourceRevision }
$params | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputRoot 'parameters.json') -Encoding UTF8
# Trusted script path is data in JSON, never interpolated into executable code.
@{ capture = $CaptureScript; output = $OutputRoot } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputRoot 'worker-input.json') -Encoding UTF8
@'
$ErrorActionPreference = 'Stop'
$inputData = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'worker-input.json') -Raw | ConvertFrom-Json
$inputParams = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'parameters.json') -Raw | ConvertFrom-Json
$arguments = @{}
foreach ($property in $inputParams.PSObject.Properties) { $arguments[$property.Name] = $property.Value }
$result = @{ success = $false; processSession = (Get-Process -Id $PID).SessionId; startUtc = [DateTime]::UtcNow.ToString('o') }
try {
    if ($result.processSession -eq 0) { throw 'Scheduled task ran in Session 0; active interactive token is required.' }
    & $inputData.capture @arguments *> (Join-Path $PSScriptRoot 'capture-transcript.log')
    $result.success = $true
} catch {
    $result.error = $_.Exception.Message
} finally {
    $result.endUtc = [DateTime]::UtcNow.ToString('o')
    # Publish only a complete JSON result; the launcher polls for its existence.
    $resultPath = Join-Path $PSScriptRoot 'worker-result.json'
    $result | ConvertTo-Json | Set-Content -LiteralPath ($resultPath + '.tmp') -Encoding UTF8
    Move-Item -LiteralPath ($resultPath + '.tmp') -Destination $resultPath
}
if (!$result.success) { exit 1 }
'@ | Set-Content -LiteralPath (Join-Path $OutputRoot 'worker.ps1') -Encoding UTF8
$action = New-ScheduledTaskAction -Execute (Join-Path $PSHOME 'powershell.exe') -Argument ('-NoLogo -NoProfile -File "' + (Join-Path $OutputRoot 'worker.ps1') + '"')
# No stored password, no elevated token and no recurring trigger.
$principal = New-ScheduledTaskPrincipal -UserId ([Security.Principal.WindowsIdentity]::GetCurrent().Name) -LogonType Interactive -RunLevel Limited
$settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Seconds ($TimeoutSeconds * 5 + 180)) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
$registered = $false
try {
    Register-ScheduledTask -TaskName $taskName -Action $action -Principal $principal -Settings $settings | Out-Null
    $registered = $true
    Start-ScheduledTask -TaskName $taskName
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds * 5 + 180)
    $resultPath = Join-Path $OutputRoot 'worker-result.json'
    while (!(Test-Path -LiteralPath $resultPath)) {
        if ([DateTime]::UtcNow -gt $deadline) { throw 'Interactive capture task timed out; inspect task status and transcript.' }
        $info = Get-ScheduledTaskInfo -TaskName $taskName
        $task = Get-ScheduledTask -TaskName $taskName
        # State and Info are separate snapshots. 0x41301 means still running,
        # even if the adjacent state snapshot briefly says Ready; 0x41303 means
        # not yet run. Neither is a failed completion.
        if ($task.State -eq 'Ready' -and $info.LastRunTime.Year -gt 2000 -and
            $info.LastTaskResult -notin @(0, 0x41301, 0x41303)) {
            if (Test-Path -LiteralPath $resultPath) { break }
            Start-Sleep -Milliseconds 200
            $confirmedInfo = Get-ScheduledTaskInfo -TaskName $taskName
            $confirmedTask = Get-ScheduledTask -TaskName $taskName
            if (Test-Path -LiteralPath $resultPath) { break }
            if ($confirmedTask.State -eq 'Ready' -and
                $confirmedInfo.LastRunTime -eq $info.LastRunTime -and
                $confirmedInfo.LastTaskResult -eq $info.LastTaskResult) {
                throw "Interactive task exited without a result (task result $($info.LastTaskResult)); inspect execution policy and logged-in session."
            }
        }
        Start-Sleep -Seconds 2
    }
    $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
    $result | ConvertTo-Json | Write-Output
    if (!$result.success) { throw "Interactive worker failed: $($result.error)" }
} finally {
    if ($registered) {
        # Stop/unregister only the unique task created by this invocation.
        if ((Get-ScheduledTask -TaskName $taskName).State -eq 'Running') { Stop-ScheduledTask -TaskName $taskName }
        Unregister-ScheduledTask -TaskName $taskName -Confirm:$false
    }
    Write-Host "Interactive evidence: $OutputRoot"
}
