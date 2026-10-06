# NGX / DLSS

WinDoom is GPLv2. NVIDIA DLSS is **not** in this repo. `.\build.cmd
--ngx-dlss4` (or 3.5 / 4.5 / 5 / `--all`) downloads the SDK into
gitignored `third_party/ngx/` and copies `nvngx_dlss.dll` next to
the exe. A plain `.\build.cmd` is nearest only, even if that folder
already exists.

One NGX binary, four folders. A one-line `ngx.mode` file picks the path:

| Folder | `ngx.mode` | Path |
|---|---|---|
| `windoom-ngx-dlss3.5` | `rr` | Ray Reconstruction |
| `windoom-ngx-dlss4` | `k` | Super Resolution, preset K |
| `windoom-ngx-dlss4.5` | `l` | Super Resolution, preset L |
| `windoom-ngx-dlss5` | `dlss5` | Same SR as 4.5; then DLSS 5 NR if RenoDX is present |

Overrides (optional): `-ngx-rr`, `-ngx-k`, `-ngx-l`. Skip NGX:
`.\play-windoom.cmd --ngx-dlss4 -nodlss`.

Input is 320x200, output 1280x800, no jitter. The status bar is
copied on after DLSS so it stays sharp.

## 4 / 4.5 / 5

4 and 4.5 are one Super Resolution pass (320→1280). 5 does that
**same** 4.5 pass first (preset L). If `renodx-dlss5.addon64` sits
next to the exe, a second pass (DLAA) runs on that image so Swapper
NR can refine it. Color for that pass is never a nearest 4x stretch.
Without the addon, the 5 folder looks like 4.5.

## Ray Reconstruction (3.5)

DOOM has no path-traced lighting. RR still gets color, depth, motion,
and normals. Missing buffers are faked (black specular, rough=1).
If create/eval fails, the log says so and the exe falls back to
preset K.

## Swapper (Native)

1. Build with NGX (`.\build.cmd --ngx-dlss5`).
2. In DLSS5-Swapper add `build-win\Release\windoom-ngx-dlss5`.
3. DirectX 12, 64-bit, **Native**. Do not put your own `dxgi.dll`
   in that folder first.

Feeder also works. Native is the intended route.

## Manual cmake

    .\fetch-ngx.cmd
    cmake -S . -B build-win -A x64 -DWINDOOM_NGX=ON
    cmake --build build-win --config Release

Needs `include/nvsdk_ngx.h` and an `nvsdk_ngx*.lib`. `WINDOOM_NGX=ON`
without a SDK is a configure error. SDK tag: NVIDIA/DLSS v310.9.1. SR and RR DLL file versions: 310.9.1.0.

Debug views (F1–F4 / `-depth` etc.) skip DLSS and use nearest so a
demo can record an aux buffer. See the root `README.md` for keys.

## Reproducible runtime installation

`fetch-ngx.cmd` uses the fixed official release manifest in `scripts/ngx-release.json`.
The NVIDIA stable release was checked on 2026-10-06: SDK `v310.9.1`, commit
`374959484e79a640feaba44c93ac8cfb0a03f5b5`. It downloads the headers, license,
x64 link libraries and **Windows_x86_64/rel** SR/RR DLLs directly from that
immutable commit. Each file must match its pinned SHA256. ARM64, development
DLLs and Frame Generation DLLs are excluded. The RR folder receives SR as its
existing fallback plus RR; the three SR folders receive SR only. Other present
modes are unaffected. This update alone does not enable every DLSS 5 feature.

The entry point verifies complete installations before reuse, stages every
new file before publication and preflights exclusive access to existing files.
A DLL in use aborts the update; quit the game before retrying. Download and
validation failures leave the original installation intact. A publication error
rolls back all touched targets. `ngx-install.json` records the SDK release,
source, actual DLL versions, architecture, hashes and signature checks. Windows
requires valid NVIDIA Authenticode signatures; a platform without that API
records that signature verification was unavailable.

Every update prints a backup identifier. Backups in
`build-win/_ngx_fetch/backups/` contain the original SDK, libraries and deployed
DLLs as one snapshot. They are retained automatically; restoring also makes a
backup of the current state, so restoration is reversible:

```powershell
.\fetch-ngx.cmd -RestoreBackup transaction-<identifier-printed-by-install>
.\fetch-ngx.cmd -Offline
```

`-Offline` requires either a verified complete installation or the full pinned
cache. Partial downloads are never reused. Backups made before a first install
restore the absence of an SDK. Backup hashes are checked before restoration;
a damaged backup is refused. Keep the complete backup directory together.

The installer does not replace game executables. `build.cmd` still stages game
builds separately. Do not run the game or another build while installing or
restoring. The installer serializes its own operations and does not alter system
DLL directories, drivers, or Swapper configuration.

Installer regression checks exercise the actual entry point in an isolated
folder. Seed it from a verified cache after a successful fetch:

```powershell
powershell -NoProfile -File scripts/test-fetch-ngx.ps1 -SeedCache build-win/_ngx_fetch/374959484e79a640feaba44c93ac8cfb0a03f5b5
```

The tests preserve their temporary directory and transcript for review. They
check first install, complete cache reuse, legacy upgrade, SR/RR deployment,
rollback/restore, corrupt cache and backup, download failure, and an in-use DLL.
