/* Copyright (C) 2026 Nikolai Zhivotenko. GPLv2; see LICENSE.TXT. */
#ifndef WIN32_GBUFFER_H
#define WIN32_GBUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    GB_KIND_WALL = 0,
    GB_KIND_FLOOR,
    GB_KIND_CEILING,
    GB_KIND_SPRITE,
    GB_KIND_SKY
};

enum
{
    GB_VIEW_COLOR = 0,
    GB_VIEW_DEPTH,
    GB_VIEW_NORMAL,
    GB_VIEW_VELOCITY,
    GB_VIEW_SCENE_MASK,
    GB_VIEW_OVERLAY_MASK,
    GB_VIEW_ALBEDO,
    GB_VIEW_MATERIAL_MASK
};

#define GB_WIDTH   320
#define GB_HEIGHT  200
#define GB_TEMPORAL_NEAR 1.0f
#define GB_TEMPORAL_FAR  8192.0f
/* Exact indexed texel before/after COLORMAP. Material namespace follows kind:
   wall textures, floor/ceiling flats; names are resolved by r_material.h. */
typedef struct {
    unsigned char source_index, ambient_index, kind, valid;
    unsigned int material_id;
} GB_MaterialSample;
void GB_SetMaterialContext(int kind, unsigned int material_id);
int GB_MaterialContextKind(void);
void GB_RecordMaterialSample(int screen_offset, unsigned char source_index,
                            unsigned char ambient_index);
const GB_MaterialSample *GB_MaterialSamples(void);
void GB_SetBasePaletteRGB(const unsigned char *palette768);
void GB_SetRawPaletteRGB(const unsigned char *palette768, const unsigned char *gamma256);
const unsigned char *GB_BasePaletteRGB(void);
const unsigned char *GB_RawPaletteRGB(void);
const unsigned char *GB_GammaLUT(void);

/* Coordinates are (DOOM X, height, DOOM Y), in map units.  Screen positions
 * and motion are full 320x200-buffer pixels, including the viewport offset.
 * Camera snapshots own every value; globals restored for UI are not frame data. */
typedef struct {
    float position[3];
    unsigned int yaw;
    float forward_cos, forward_sin;
    float center_x, center_y, projection;
} GB_CameraSample;

enum {
    GB_RESET_INITIAL = 1, GB_RESET_SCENE = 2, GB_RESET_VIEW = 4,
    GB_RESET_TELEPORT = 8, GB_RESET_LOAD = 16, GB_RESET_LEVEL = 32,
    GB_RESET_MENU = 64, GB_RESET_PAUSE = 128, GB_RESET_CAMERA_CUT = 256,
    GB_RESET_EXPLICIT = 512, GB_RESET_GEOMETRY = 1024, GB_RESET_SHADING = 2048, GB_RESET_REFLECTION = 4096
};
typedef struct {
    unsigned int frame_id;
    int game_tic, map_episode, map_number, scene_valid, history_valid, fixed_timeline;
    int viewport_x, viewport_y, viewport_width, viewport_height, detail_shift;
    int render_width, render_height, output_width, output_height;
    GB_CameraSample base, sampled;
    /* Actual fine-table rays per unit GB depth, not normalized ray distance.
     * Column and plane row sampling differ in the original software renderer. */
    float ray_x[GB_WIDTH], ray_z[GB_WIDTH];
    float ray_up_column[GB_HEIGHT], ray_up_plane[GB_HEIGHT];
    float jitter_x, jitter_y, frame_delta_ms;
    unsigned int reset_reasons;
} GB_FrameInputs;

void GB_BeginScene(void);
void GB_CaptureSampling(float jitter_x, float jitter_y);
void GB_SetFrameTiming(int game_tic, float delta_ms, int fixed_timeline,
                       int menu_open, int paused, int map_episode, int map_number);
void GB_RequestResetReason(unsigned int reasons);
const GB_FrameInputs *GB_GetFrameInputs(void);
const unsigned char *GB_SurfaceKind(void);
/* Reject padding/sky/2D pixels. Ray vector is in Y-up world coordinates. */
int GB_SampleRay(int screen_x, int screen_y, float ray_world[3]);
int GB_SampleWorldPosition(int screen_x, int screen_y, float world[3]);
/* Analytic current->previous, jitter-free low-resolution pixel displacement.
 * World positions are current and previous positions of the same surface. */
int GB_ProjectMotion(const GB_FrameInputs *current, const GB_FrameInputs *previous,
                     const float current_world[3], const float previous_world[3],
                     float motion[2]);
/* Conventional perspective device Z: near0, far1, no depth-inverted flag.
 * Invalid pixels use far1. Raw world reconstruction continues using GB_Depth. */
float GB_DeviceDepthFromViewDepth(float view_depth, int scene_valid);

void GB_Init(void);
void GB_Shutdown(void);

void GB_BeginFrame(void);
/* One software display pass. Capture after world sprites, before player weapon. */
void GB_BeginDisplay(void);
void GB_CaptureScene(const unsigned char *src8);
void GB_InvalidateScene(void);
void GB_MarkOverlayColumn(int x, int y, int count);
void GB_MarkOverlayRect(int x, int y, int width, int height);
void GB_SetColumn(int x, float z, float nx, float ny, float nz, int kind);
void GB_SetObjectMotion(float du, float dv);
void GB_WriteColumn(int x, int yl, int yh);
void GB_WriteSpan(int y, int x1, int x2, float z, float nx, float ny, float nz);
void GB_RequestReset(void);
int  GB_ConsumeReset(void);
void GB_EndFrame(void);

void GB_SetPaletteRGB(const unsigned char *rgb768);
void GB_ConvertColor(const unsigned char *src8);

void GB_SetDebugView(int view);
int  GB_GetDebugView(void);

void GB_ToggleHud(void);
int  GB_HudVisible(void);

const unsigned char *GB_ColorRGBA(void);
const unsigned char *GB_OverlayRGBA(void);
/* True world coverage only: excludes sky, padding, weapon and 2D geometry. */
const unsigned char *GB_SceneMask(void);
const unsigned char *GB_OverlayMask(void);
const float         *GB_Depth(void);
const float         *GB_TemporalDepth(void);
const unsigned char *GB_NormalRGBA(void);
/* Float3 physical inward normals from the current scene capture. Wall normals
 * come from the current directed seg; planes use their exact axis normal.
 * Valid only where material/scene capture is valid; overlays never replace it. */
const float *GB_GeometricNormalXYZ(void);
void GB_SetColumnGeometricNormal(float nx,float ny,float nz);
const float         *GB_VelocityRG(void);

void GB_ComposePresent(unsigned char *dst_bgra, int dst_w, int dst_h);
int  GB_HasScenePixels(void);
void GB_OverlayHud(unsigned char *dst_bgra, int dst_w, int dst_h);

#ifdef __cplusplus
}
#endif

#endif
