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
| `windoom-ngx-dlss5` | `dlss5` | Same SR as 4.5; optional loaded-addon DLAA, external NR unverified |

Overrides (optional): `-ngx-rr`, `-ngx-k`, `-ngx-l`. Skip NGX (independent of future RT controls):
`.\play-windoom.cmd --ngx-dlss4 -nodlss`. `-nosr` is an equivalent explicit SR switch.

Input is 320x200, output 1280x800, no jitter. The status bar is
copied on after DLSS so it stays sharp.

## 4 / 4.5 / 5

4 and 4.5 are one Super Resolution pass (320→1280). 5 does that
**same** 4.5 pass first (preset L). A second pass (DLAA) is requested only
if the `renodx-dlss5.addon64` module is actually loaded by the optional external
integration. File presence alone does not enable it. Color for that pass is
never a nearest 4x stretch. Logs distinguish the addon file, loaded module,
actual SR/DLAA evaluations and external NR activation, which remains
unverified by this program. Without a loaded addon, the 5 folder uses official SR.

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
existing fallback plus RR; the three SR folders receive SR only. Old RR DLLs
left there by the legacy installer are removed within the same backup transaction
and recovered on restore. External frame generation DLLs, injectors and addons
are preserved. Other present modes are unaffected. This update alone does not enable every DLSS 5 feature.

The entry point verifies complete installations before reuse, stages every
new file before publication and preflights exclusive access to existing files.
Only manifest-listed files enter the new SDK stage; extra cache files are left
alone. Reuse also requires the installed SDK inventory to contain only those
files, their directories and `ngx-install.json`. An extra installed header or
library triggers a replacement with a backup of the complete original SDK.
Managed paths, pinned cache files and SDK/backup trees must not contain junctions
or symbolic links; those cases abort rather than following external targets.
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


## Loaded identity and failure diagnostics

Startup distinguishes the SDK release/commit used to compile, deployment receipt,
DLL file resource version, requested preset, feature, and fixed input/output sizes.
The `NGX loaded:` record enumerates a module loaded by NGX itself, reads its actual
Windows path, version and SHA256, and compares that hash with `ngx-install.json`.
The local-file preflight is explicitly labelled as preparation; it never counts
as evaluation. Missing/invalid receipts or mismatches are reported without
misidentifying the loaded DLL as the installed version.

`present mode: ngx-pending` means NGX has initialized but has not evaluated yet.
`NGX presented:` and the actual `present mode:` transition appear only after the
frame's evaluation/display choice is known. Shutdown records successful/failed SR
evaluation counts. Missing or incompatible local SR DLLs, unavailable capabilities,
old drivers and initialization/creation/evaluation errors retain the ordinary
nearest display path. A failed SR evaluation disables further SR attempts for
that process; resources remain owned until the renderer's normal GPU-safe shutdown.

The first latest-runtime RTX acceptance confirmed 320x200 to 1280x800 creation and
successful evaluation with DLL 310.9.1.0, even though the optimal-settings query
advertised 427x267. This observation is specific to that tested runtime/driver;
creation/evaluation remains authoritative and errors still fall back. Preset
requests do not imply configurable scene resolution or complete quality modes.

To capture repeatable acceptance from a confirmed interactive console, use
`tools/capture-ngx-runtime.ps1` with an executable, verified SDK, matching IWAD,
external demo and a new output directory. It compares failure/off PNGs at equal
`game_tic` values, preserves per-case module logs/CSVs/frames, and can include an
executable whose SDK/runtime were actually restored using the installer.

Failure acceptance builds opt in with `-DWINDOOM_NGX_DIAGNOSTICS=ON`; the ordinary
build defaults to OFF. Only those diagnostic builds accept `-ngx-fail init`,
`capability`, `create`, `evaluate`, or `evaluate-late` (three successful evaluations,
then failure). These are explicitly synthetic API results. They exercise the
same game's production fallback path and do not impersonate a physical GPU or
driver fault. Missing/incompatible DLL scenarios use actual loader failures.
No driver, system DLL, injection component or security setting is installed by
this capture tool.
