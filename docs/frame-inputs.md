# Frozen frame inputs

`GB_GetFrameInputs()` describes the current software scene, independently of
camera globals restored for the weapon and UI. Returned pointers describe the
current frame only; consumers that retain history must copy the descriptor
before the next display begins. `R_RenderPlayerView` captures the
base camera before raster jitter and the actual sampled camera/rays immediately
after jitter, before any world drawing. Scene color, depth and normals are
captured before player sprites. The raster camera is restored at that boundary
so weapon and UI positions do not inherit FSR2 scene jitter. The renderer finalizes time/history/motion once
per presented frame, through the existing `I_FinishUpdate` entry point.

## Coordinates and sampling

- World position is `(DOOM_X, height, DOOM_Y)` in map units. Normal RGB encodes
  the same Y-up axes, using the existing wall/sprite normal conventions.
- Full input buffer is320x200; output1280x800. The viewport has an explicit
  full-buffer origin and dimensions. Padding/sky/2D pixels are not scene rays.
  The Windows renderer already forces full detail; `detail_shift` is recorded.
- `sampled.position` and `sampled.yaw` are the actual raster pose. `ray_x` and
  `ray_z` retain `finecosine/finesine[(yaw+xtoviewangle[x])>>ANGLETOFINESHIFT]`
  multiplied by the actual `distscale[x]`. This preserves software quantization.
- `ray_up_column` uses the fractional raster projection center. Plane rows
  instead use the signed inverse of the actual `yslope[y]`, including their
  original half-pixel sampling. FSR2 updates and restores that table along with
  its camera. Columns use the same fractional center for texture sampling.
- `GB_SurfaceKind()` selects column versus plane sampling. `GB_SampleRay`
  returns a Y-up ray at a full-buffer pixel; `GB_SampleWorldPosition` reconstructs
  `sampled.position + ray * GB_Depth`. Both reject invalid scene pixels.
- GB depth is the software view-axis depth convention, not normalized ray
  distance. Fine-table quantization means `dot(ray,forward)` can differ slightly
  from1. A GPU tracing module should normalize the actual supplied ray if needed
  and convert its result consistently rather than assuming those are identical.

## Motion and jitter

`GB_ProjectMotion` projects a world point into both **base**, unjittered cameras
and subtracts `previousPixel-currentPixel` in full low-resolution buffer pixels.
Viewport origins participate in both projections. Reconstructing a sampled world
point before projecting it into both base cameras avoids the static bias caused
by comparing a quantized ray with an ideal integer screen coordinate. Static
geometry has exactly zero camera motion even when raster jitter changes.
NGX and FSR2 consume scale1 vectors; motion does not include jitter. A point
behind the previous camera (view depth below1) has no usable reprojection and
receives zero motion rather than unbounded or nonfinite values.

The official NGX path currently samples with zero jitter and records zero. FSR2
retains its existing horizontal **yaw approximation** to subpixel jitter; the
sampled yaw/fine rays are authoritative. It does not claim an exact off-axis
projection translation. Its vertical offset uses the fractional projection
center and the updated plane table. The requested FSR2 jitter is recorded and
passed to FSR2; changing that approximation to a true off-axis raster projection
is a separate quality improvement.

Sprite momentum remains the existing per-tic approximation, now multiplied by
the elapsed simulation tics instead of being added during identical repeated
frames. This is not exact entity history or disocclusion. Moving-sector history
is addressed by ticket #13.

## Time and history

Normal play passes actual monotonic QPC milliseconds between presented frame
preparations to both temporal APIs. The first frame uses1000/35ms. Fixed
`singletics` demo/export uses exactly1000/35ms regardless of PNG encoding time;
CSV game_tic retains the simulation timeline. Wipe frames can repeat a game tic
and remain explicitly non-scene frames.

Reset reasons are an observable bitmask:

| Bit | Meaning |
|---:|---|
|1|Initial frame|
|2|Scene/non-scene transition|
|4|Viewport or debug rendering configuration changed|
|8|Successful player teleport|
|16|Successful savegame restore|
|32|Level load, including same-map reload|
|64|Menu open/close transition|
|128|Pause/resume transition|
|256|Camera cut, backwards simulation time, or normal frame gap over250ms|
|512|Other explicit invalidation, including HUD changes|

A cut is translation over128map units between sampled frames or yaw change
over90degrees. Continuous fast turns below that threshold reproject normally.
On reset, motion is cleared and no previous frame is used. Non-scene frames do
not seed history. After a reset scene frame, that frame seeds the next scene.
NGX's existing reset flag receives the same frame decision.

## Observations and acceptance

`-frame-inputs <new.csv>` records the frozen pose, viewport, time, reset reasons,
actual SR choice and map identity, center scene probe, world point, depth, normal and motion, plus
maximum/nonfinite motion counts. It refuses to replace an existing trace.
Observations use the ordinary game/demo/export path; no alternate renderer is
introduced. Center probes can be invalid, and are labelled accordingly.

`tools/check-frame-contract.c` checks the public projection helper analytically:
static samples, jitter removal, lateral/vertical/forward translation,45-degree
turn and nonzero viewport origins. A C compiler can build it with
`win32/frame_inputs.c` and the `win32` include path.

`tools/capture-frame-inputs.ps1` captures ordinary SR/off/depth/normal/velocity
and a reduced viewport. Its fixture demo must actually cross a player teleport;
it checks the observed event instead of injecting a synthetic reset. Optional
`-UnsupportedAdapter` selects a real D3D12 render device for NGX-unavailable
acceptance. Run through the existing interactive launcher with a new output
folder. Keep source/executable/IWAD/demo hashes and match screenshots by game tic,
not export index. Live menu/pause/load/map transitions and QPC timing require a
separate normal-play window walk; a fixed demo cannot establish those claims.
