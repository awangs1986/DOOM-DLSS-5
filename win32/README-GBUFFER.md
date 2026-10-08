# G-buffer

The game still draws 320x200. The window is 1280x800. Do not raise
`SCREENWIDTH` / `SCREENHEIGHT`: HUD and menus are hard-wired to 320x200.

Debug views (Win32 window only, not the DOOM menu):

    F1  color
    F2  depth
    F3  normals
    F4  velocity

## CPU buffers (320x200)

Origin top-left, +X right, +Y down. Written in `win32/gbuffer.c` at
the same `columnofs` / `ylookup` address as `screens[0]` (the bezel
hole). The scene is captured after masked world sprites and before player
weapon sprites. Thus the real world color/depth behind a weapon remains
available to SR and future lighting. Weapon column writes and front-buffer
patch posts record explicit overlay coverage, including pixels whose palette
value equals the scene. Patch transparent gaps and offscreen scratch draws are
not coverage. HUD, pause, menu glyphs, rectangular blits and window borders are
composed from the final software frame after scene upscaling.

The scene input fills the region outside the view with clamped edge color,
far-depth sentinel 8192 and zero motion. `GB_SceneMask` excludes that padding
and sky; they are never ray-tracing surfaces. `GB_OverlayMask` describes opaque
2D pixels independently of depth. Insert preserves its existing meaning: hide
the status bar and outside-view border/padding, while weapon/menu/HUD text stay
visible. The actual UI coordinates remain 320x200.

Color — RGBA8, PLAYPAL after gamma, A=255.

Depth — float32 linear view-Z in map units (farther is larger, not
inverted). Walls: `projection / rw_scale`. Flats: `planeheight * yslope`.
World sprites: `projection / spryscale`. Sky/padding: 8192 (mask=0).
Weapon/UI never write scene depth. Within the scene rectangle, scene depth and
motion behind a weapon/HUD/menu remain the real world's values. Outside the
rectangle the padding sentinel is invalid geometry, not a 2D surface.
NGX / FSR2: not DepthInverted.

Normal — RGBA8, world space, Y up, stored as `n * 0.5 + 0.5`.
Walls use `rw_normalangle`. Floor `(0,1,0)`, ceiling `(0,-1,0)`.
Sprites face the camera. Sky `(0,1,0)`.

Velocity — RG32F, pixel delta at 320x200 for NGX `MVLowRes`.
`mv = prev - cur` (+X right, +Y down). Camera reprojection from
depth and the last view pose, plus per-sprite motion when the
renderer sets it. Not optical flow. Menu / TITLEPIC / sky / HUD
stay 0. Overlaid weapon/menu pixels do not overwrite the underlying scene
motion. Non-scene title/help/automap/intermission/finale/wipe frames bypass all
scene upscalers and use the complete software frame; they clear scene history.
`GB_RequestReset()` zeros motion on the next frame.

Map: +X east, +Y north, +Z up. `viewangle` is BAM; 0 looks at +X.

## GPU

Created in `I_InitGraphics`, updated every frame:

    GB_Color     R8G8B8A8_UNORM
    GB_Depth     R32_FLOAT
    GB_Normal    R8G8B8A8_UNORM
    GB_Velocity  R32G32_FLOAT

Swapchain is B8G8R8A8, 1280x800, windowed.

## Layer inspection and output

F1-F4 retain color/depth/normal/velocity controls. Depth and normal views use
only valid world coverage; 2D overlays are absent so the scene beneath the
weapon is visible for inspection. Use `-scene-mask` or `-overlay-mask` for
white/black coverage exports. The scene mask can overlap the overlay mask:
that means genuine world geometry exists behind opaque 2D pixels, not that
those 2D pixels acquired depth or motion.

Nearest presentation and successful NGX/FSR2/Anime4K presentation share the
same final opaque overlay colors and coverage. All color paths compose the
complete weapon, status, border, pause, text and menu before export readback.
A PNG is copied from the same final backbuffer subsequently shown in the
window. Debug output intentionally omits color overlays.

The portable public test `tests/layering_test.c` uses actual `V_DrawPatch`
posts and G-buffer APIs to check unchanged scene depth/color beneath a weapon,
equal-color coverage, offscreen patch exclusion, invalid padding geometry,
Insert semantics, complete nearest composition, debug views and non-scene
fallback. Real Windows builds and matched-tic demo exports provide the full
rendering acceptance; compare run-specific `game_tic`, not export frame index.
