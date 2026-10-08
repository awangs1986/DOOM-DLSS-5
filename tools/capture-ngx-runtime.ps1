# Player-visible NGX acceptance through the ordinary demo/export entry point.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$SdkDirectory,
    [Parameter(Mandatory=$true)][string]$Iwad,
    [Parameter(Mandatory=$true)][string]$Demo,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [string]$RestoredExecutable,
    [switch]$IncludeFaultCases,
    [ValidateRange(1,600)][int]$TimeoutSeconds = 60,
    [int[]]$KeyGameTics = @(35,70,175)
)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
foreach($path in @($Executable,$Iwad,$Demo)) { if(-not (Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing input: $path"} }
if((Get-Process -Id $PID).SessionId -eq 0){throw 'Use a confirmed interactive desktop; SSH Session 0 is not a rendering surface.'}
if(Test-Path -LiteralPath $OutputRoot){throw 'OutputRoot must be new; existing evidence is retained.'}
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
$sdk = Get-Content -LiteralPath (Join-Path $SdkDirectory 'ngx-install.json') -Raw | ConvertFrom-Json
$runtime = $sdk.runtimes | Where-Object {$_.name -eq 'nvngx_dlss.dll'}
$dll = Join-Path $SdkDirectory 'lib/Windows_x86_64/rel/nvngx_dlss.dll'
if((Get-FileHash -LiteralPath $dll).Hash.ToLowerInvariant() -ne $runtime.sha256){throw 'Runtime seed differs from verified receipt.'}
$cases = @(
    @{name='sr-on'; arguments=@(); expect='SR'},
    @{name='sr-off'; arguments=@('-nosr'); expect='nearest'},
    @{name='legacy-off'; arguments=@('-nodlss'); expect='nearest'},
    @{name='missing-runtime'; arguments=@(); expect='nearest'},
    @{name='incompatible-runtime'; arguments=@(); expect='nearest'},
    @{name='addon-file-only'; arguments=@(); expect='SR'},
    @{name='missing-receipt'; arguments=@(); expect='SR'},
    @{name='mismatched-receipt'; arguments=@(); expect='SR'}
)
if($IncludeFaultCases){
    foreach($stage in @('init','capability','create','evaluate','evaluate-late')){
        $cases += @{name=('fail-'+$stage); arguments=@('-ngx-fail',$stage); expect='nearest'; injected=$stage}
    }
}
if($RestoredExecutable){$cases += @{name='restored-runtime';arguments=@();expect='SR';restored=$true}}
$manifest = [ordered]@{schemaVersion=1;sourceExecutable=$Executable;exeSha256=(Get-FileHash -LiteralPath $Executable).Hash;sdkTag=$sdk.sdkTag;commit=$sdk.commit;iwadSha256=(Get-FileHash -LiteralPath $Iwad).Hash;demoSha256=(Get-FileHash -LiteralPath $Demo).Hash;session=(Get-Process -Id $PID).SessionId;runs=@();claims=@();complete=$false}
function Save-Manifest {$manifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $OutputRoot 'manifest.json') -Encoding UTF8}
function Claim([bool]$Pass,[string]$Name,[string]$Evidence){
    $manifest.claims += @{name=$Name;pass=$Pass;evidence=$Evidence}
    Save-Manifest
    if(-not $Pass){throw "Acceptance failed: $Name. Evidence $Evidence"}
}
try {
    foreach($case in $cases){
        $dir=Join-Path $OutputRoot $case.name
        New-Item -ItemType Directory -Path $dir | Out-Null
        $exe=Join-Path $dir 'windoom.exe'
        if($case.restored){$exe=$RestoredExecutable}
        else {
            Copy-Item -LiteralPath $Executable -Destination $exe
            $receipt = [ordered]@{schemaVersion=1;sdkTag=$sdk.sdkTag;commit=$sdk.commit;source=$sdk.source;architecture='Windows x64';configuration='Release';runtimes=@($runtime)}
            if($case.name -ne 'missing-runtime'){
                if($case.name -eq 'incompatible-runtime'){[IO.File]::WriteAllText((Join-Path $dir 'nvngx_dlss.dll'),'incompatible test runtime')}
                else {Copy-Item -LiteralPath $dll -Destination (Join-Path $dir 'nvngx_dlss.dll')}
            }
            if($case.name -eq 'mismatched-receipt'){
                $bad = $runtime | ConvertTo-Json -Depth 8 | ConvertFrom-Json
                $bad.sha256='0'*64
                $receipt.runtimes=@($bad)
            }
            if($case.name -ne 'missing-receipt'){$receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $dir 'ngx-install.json') -Encoding UTF8}
            if($case.name -eq 'addon-file-only'){Set-Content -LiteralPath (Join-Path $dir 'renodx-dlss5.addon64') 'not a loaded addon'}
            Set-Content -LiteralPath (Join-Path $dir 'ngx.mode') 'dlss5'
        }
        Copy-Item -LiteralPath $Demo -Destination (Join-Path $dir 'baseline.lmp')
        $args=@('-iwad',$Iwad,'-config',(Join-Path $dir 'default.cfg'),'-playdemo','baseline','-color','-export',(Join-Path $dir 'frames'),'-gpu-timing',(Join-Path $dir 'gpu-timing.csv')) + $case.arguments
        $quoted=($args | ForEach-Object {'"'+$_+'"'}) -join ' '
        $entry=[ordered]@{name=$case.name;arguments=$args;executable=$exe;exeSha256=(Get-FileHash -LiteralPath $exe).Hash;frames=@();exitCode=$null;expected=$case.expect}
        $manifest.runs += $entry
        $p=$null
        try {
            $p=Start-Process -FilePath $exe -ArgumentList $quoted -WorkingDirectory $dir -RedirectStandardOutput (Join-Path $dir 'stdout.log') -RedirectStandardError (Join-Path $dir 'stderr.log') -PassThru
            $handle=$p.Handle
            $entry.session=$p.SessionId
            if(-not $p.WaitForExit($TimeoutSeconds*1000)){throw "Case $($case.name) timed out"}
            $entry.exitCode=$p.ExitCode
        } finally {if($p -and -not $p.HasExited){$p.Kill();$p.WaitForExit()}}
        $logPath=Join-Path $dir 'stderr.log'
        $log=Get-Content -LiteralPath $logPath -Raw
        $rows=@(Import-Csv -LiteralPath (Join-Path $dir 'gpu-timing.csv'))
        $entry.pngCount=@(Get-ChildItem -LiteralPath (Join-Path $dir 'frames') -Filter 'f*.png').Count
        foreach($tic in $KeyGameTics){
            $row=$rows | Where-Object {[int]$_.game_tic -eq $tic} | Select-Object -First 1
            if($row){
                $png=Join-Path $dir ('frames/f{0:D6}.png' -f [int]$row.frame)
                if(Test-Path -LiteralPath $png){$entry.frames += @{gameTic=$tic;frame=[int]$row.frame;path=$png;sha256=(Get-FileHash -LiteralPath $png).Hash}}
            }
        }
        Claim ($entry.exitCode -eq 0 -and $entry.session -gt 0 -and $entry.pngCount -eq $rows.Count -and $entry.frames.Count -eq $KeyGameTics.Count) "$($case.name): game exits normally and exports matched demo timeline" $logPath
        if($case.expect -eq 'SR'){
            Claim ($log -match 'NGX presented: feature=SR .*reason=evaluation-succeeded' -and $log -match 'sr_evaluate_successes=[1-9][0-9]* sr_evaluate_failures=0') "$($case.name): actual SR evaluates 320x200 to 1280x800" $logPath
            if($case.name -in @('sr-on','addon-file-only')){
                Claim ($log -match ('dll_file_version='+[regex]::Escape($runtime.fileVersion)+' sha256='+$runtime.sha256+' receipt_match=yes')) "$($case.name): loaded module version and SHA256 match deployment" $logPath
                Claim ($log -match 'addon_module=not-loaded' -and $log -notmatch 'SR\+DLAA|SR preset L then DLAA') "$($case.name): no extension execution inferred from file presence" $logPath
            }
            if($case.name -eq 'missing-receipt'){Claim ($log -match 'receipt=missing-or-invalid' -and $log -match 'receipt_match=unrecorded') 'Missing receipt is accurately unrecorded, with usable SR' $logPath}
            if($case.name -eq 'mismatched-receipt'){Claim ($log -match 'NGX loaded: .*receipt_match=no') 'Loaded-runtime receipt mismatch is observable' $logPath}
            if($case.restored){Claim ($log -match 'NGX identity: deployment_sdk=v310.7.0' -and $log -match 'NGX loaded: .*receipt_match=yes') 'Real installer restore yields old runtime identity and successful SR' $logPath}
        } else {
            Claim ($log -match 'present mode: nearest' -and $log -notmatch 'NGX presented: feature=SR .*reason=evaluation-succeeded' -or $case.name -eq 'fail-evaluate-late') "$($case.name): usable nearest display fallback" $logPath
            if($case.name -eq 'missing-runtime'){Claim ($log -match 'reason=runtime-missing') 'Missing DLL diagnostic' $logPath}
            if($case.name -eq 'incompatible-runtime'){Claim ($log -match 'reason=runtime-incompatible win32_error=193') 'Incompatible DLL diagnostic includes native loader error' $logPath}
            if($case.injected){Claim ($log -match 'NGX diagnostic: injected_failure=' -and $log -match 'synthetic API result') "$($case.name): injection is explicitly labelled" $logPath}
            if($case.name -eq 'fail-evaluate-late'){Claim ($log -match 'sr_evaluate_successes=3 sr_evaluate_failures=1' -and $log -match 'feature=none .*reason=evaluate-failed') 'Evaluation failure after success latches fallback without stale SR output' $logPath}
        }
        Save-Manifest
    }
    $reference=$manifest.runs | Where-Object {$_.name -eq 'sr-off'}
    foreach($run in $manifest.runs | Where-Object {$_.expected -eq 'nearest'}){
        foreach($frame in $run.frames){
            $expected=$reference.frames | Where-Object {$_.gameTic -eq $frame.gameTic}
            Claim ($frame.sha256 -eq $expected.sha256) "$($run.name): fallback pixels equal SR-off at game tic $($frame.gameTic)" $frame.path
        }
    }
    $manifest.complete=$true
} finally {Save-Manifest; Write-Host "NGX runtime evidence: $OutputRoot"}
