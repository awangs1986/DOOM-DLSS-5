# Ordinary demo/export observations of the public frozen-frame contract.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$Iwad,
 [Parameter(Mandatory=$true)][string]$Demo,
 [Parameter(Mandatory=$true)][string]$SrExecutable,
 [string]$SourceRevision,
 [Parameter(Mandatory=$true)][string]$OutputRoot,
 [switch]$InteractiveDesktopConfirmed,
 [ValidateRange(1,600)][int]$TimeoutSeconds=60,
 [string]$SdkDirectory=(Join-Path (Split-Path -Parent $PSScriptRoot) 'third_party/ngx'),
 [int]$UnsupportedAdapter=-1
)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
if(-not $InteractiveDesktopConfirmed -or (Get-Process -Id $PID).SessionId -eq 0){throw 'Confirmed interactive desktop required'}
if(Test-Path $OutputRoot){throw 'OutputRoot must be new'}
New-Item -ItemType Directory $OutputRoot | Out-Null
$sdk=Get-Content (Join-Path $SdkDirectory 'ngx-install.json') -Raw | ConvertFrom-Json
$runtime=$sdk.runtimes | Where-Object {$_.name -eq 'nvngx_dlss.dll'}
$dll=Join-Path $SdkDirectory 'lib/Windows_x86_64/rel/nvngx_dlss.dll'
if((Get-FileHash $dll).Hash.ToLowerInvariant() -ne $runtime.sha256){throw 'Unverified runtime seed'}
$cases=@(@{name='sr';args=@()},@{name='off';args=@('-nosr')},@{name='depth';args=@('-depth')},@{name='normal';args=@('-normal')},@{name='velocity';args=@('-velocity')},@{name='viewport';args=@();small=$true})
if($UnsupportedAdapter -ge 0){$cases+=@{name='unsupported';args=@('-adapter',[string]$UnsupportedAdapter)}}
$manifest=[ordered]@{revision=$SourceRevision;session=(Get-Process -Id $PID).SessionId;exeSha256=(Get-FileHash $SrExecutable).Hash;iwadSha256=(Get-FileHash $Iwad).Hash;demoSha256=(Get-FileHash $Demo).Hash;runs=@();complete=$false}
try {
 foreach($case in $cases){
  $dir=Join-Path $OutputRoot $case.name
  New-Item -ItemType Directory $dir | Out-Null
  Copy-Item $SrExecutable (Join-Path $dir 'windoom.exe')
  Copy-Item $dll (Join-Path $dir 'nvngx_dlss.dll')
  @{schemaVersion=1;sdkTag=$sdk.sdkTag;commit=$sdk.commit;runtimes=@($runtime)} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $dir 'ngx-install.json') -Encoding UTF8
  Set-Content (Join-Path $dir 'ngx.mode') 'l'
  if($case.small){Set-Content (Join-Path $dir 'default.cfg') 'screenblocks 7'}
  Copy-Item $Demo (Join-Path $dir 'frame.lmp')
  $args=@('-iwad',$Iwad,'-config',(Join-Path $dir 'default.cfg'),'-playdemo','frame','-export',(Join-Path $dir 'frames'),'-gpu-timing',(Join-Path $dir 'gpu.csv'),'-frame-inputs',(Join-Path $dir 'inputs.csv'))+$case.args
  $quoted=($args | ForEach-Object {'"'+$_+'"'}) -join ' '
  $p=$null
  try {
   $p=Start-Process (Join-Path $dir 'windoom.exe') -ArgumentList $quoted -WorkingDirectory $dir -RedirectStandardOutput (Join-Path $dir 'stdout.log') -RedirectStandardError (Join-Path $dir 'stderr.log') -PassThru
   $handle=$p.Handle;$session=$p.SessionId
   if(-not $p.WaitForExit($TimeoutSeconds*1000)){throw 'Demo timeout'}
   $exit=$p.ExitCode
  } finally {if($p -and -not $p.HasExited){$p.Kill();$p.WaitForExit()}}
  $rows=@(Import-Csv (Join-Path $dir 'inputs.csv'))
  $entry=[ordered]@{name=$case.name;arguments=$args;exit=$exit;session=$session;rows=$rows.Count;keys=@()}
  $manifest.runs+=$entry
  if($exit -ne 0 -or $session -eq 0 -or $rows.Count -eq 0){throw 'Exit/frame trace failed'}
  if(@($rows | Where-Object {[int]$_.velocity_nonfinite -ne 0 -or [double]$_.delta_ms -lt 28.571 -or [double]$_.delta_ms -gt 28.572}).Count){throw 'Nonfinite motion or broken35Hz timeline'}
  if(@($rows | Where-Object {([int]$_.reset -band 8) -ne 0}).Count -eq 0){throw 'Demo did not exercise real teleport'}
  $static=@($rows | Where-Object {[int]$_.scene -eq 1 -and [int]$_.game_tic -ge 40 -and [int]$_.game_tic -le 65})
  if($static.Count -eq 0 -or @($static | Where-Object {[double]$_.velocity_max -gt 0.001}).Count){throw 'Static camera motion must be zero'}
  if($case.small -and @($rows | Where-Object {[int]$_.scene -eq 1 -and [int]$_.vx -gt 0 -and [int]$_.vy -gt 0}).Count -eq 0){throw 'Small viewport not exercised'}
  if($case.name -in @('sr','viewport') -and @($rows | Where-Object {[int]$_.used_sr -eq 1}).Count -eq 0){throw 'Actual SR never presented'}
  foreach($tic in @(50,95,110,175,220,245,280)){
   $r=$rows | Where-Object {[int]$_.game_tic -eq $tic} | Select-Object -First 1
   if($r){
    $file=Join-Path $dir ('frames/f{0:D6}.png' -f [int]$r.frame)
    if(Test-Path $file){Copy-Item $file (Join-Path $dir ('tic'+$tic+'.png'));$entry.keys+=@{gameTic=$tic;sha256=(Get-FileHash $file).Hash}}
   }
  }
  if($case.name -eq 'unsupported'){
   $log=Get-Content (Join-Path $dir 'stderr.log') -Raw
   if($log -notmatch 'NGX: init failed|NGX: DLSS unavailable' -or $log -match 'injected_failure' -or @($rows | Where-Object {[int]$_.used_sr -eq 1}).Count){throw 'Genuine unavailable NGX adapter not observed'}
  }
 }
 $manifest.complete=$true
} finally {$manifest | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $OutputRoot 'manifest.json') -Encoding UTF8}
