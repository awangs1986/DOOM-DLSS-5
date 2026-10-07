/* Copyright (C) 2026 Nikolai Zhivotenko. GPLv2; see LICENSE.TXT. */
#include "gbuffer.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "r_local.h"
#include "tables.h"
#include "v_video.h"

#define GB_PIX   (GB_WIDTH * GB_HEIGHT)
#define GB_FAR_Z 8192.0f

static unsigned char gb_color[GB_PIX * 4];
static unsigned char gb_scene8[GB_PIX];
static unsigned char gb_overlay_color[GB_PIX * 4];
static unsigned char gb_scene_mask[GB_PIX];
static unsigned char gb_surface_kind[GB_PIX];
static unsigned char gb_overlay_mask[GB_PIX];
static int gb_scene_captured;
static int gb_overlay_drawing;
static int gb_last_scene;
static float         gb_depth[GB_PIX];
static float         gb_temporal_depth[GB_PIX];
static unsigned char gb_normal[GB_PIX * 4];
static float         gb_velocity[GB_PIX * 2];
static float         gb_obj_du[GB_PIX];
static float         gb_obj_dv[GB_PIX];
static unsigned char gb_palette[256 * 3];
static unsigned char gb_base_palette[768], gb_raw_palette[768], gb_gamma[256];
static GB_MaterialSample gb_material[GB_PIX];
static int gb_material_kind = GB_KIND_SKY;
static unsigned int gb_material_id;

static float gb_col_z;
static float gb_col_nx, gb_col_ny, gb_col_nz;
static float gb_col_du, gb_col_dv;
static int   gb_col_x = -1;
static int   gb_debug_view = GB_VIEW_COLOR;
static int   gb_hud_visible = 1;
static int   gb_reset;

static int gb_have_prev, gb_menu_open, gb_paused, gb_have_state;
static int gb_col_kind;
static GB_FrameInputs gb_frame, gb_previous;
static unsigned int gb_pending_reset;

static void gb_clear_aux(void)
{
    int i;

    memset(gb_scene_mask, 0, sizeof(gb_scene_mask));
    memset(gb_material, 0, sizeof(gb_material));
    memset(gb_surface_kind, GB_KIND_SKY, sizeof(gb_surface_kind));
    memset(gb_depth, 0, sizeof(gb_depth));
    memset(gb_normal, 0, sizeof(gb_normal));
    memset(gb_velocity, 0, sizeof(gb_velocity));
    memset(gb_obj_du, 0, sizeof(gb_obj_du));
    memset(gb_obj_dv, 0, sizeof(gb_obj_dv));
    for (i = 0; i < GB_PIX; i++) {
	gb_normal[i * 4 + 2] = 128;
        gb_temporal_depth[i] = 1.0f;
    }
}

void GB_Init(void)
{
    memset(gb_color, 0, sizeof(gb_color));
    gb_clear_aux();
    gb_scene_captured = gb_overlay_drawing = gb_last_scene = 0;
    memset(gb_overlay_mask, 0, sizeof(gb_overlay_mask));
    gb_have_prev = 0;
    gb_reset = 1;
    gb_pending_reset = GB_RESET_INITIAL;
    memset(&gb_frame, 0, sizeof(gb_frame));
    memset(&gb_previous, 0, sizeof(gb_previous));
    gb_have_state = 0;
    gb_debug_view = GB_VIEW_COLOR;
    gb_hud_visible = 1;
    gb_col_du = 0.0f;
    gb_col_dv = 0.0f;
}

void GB_Shutdown(void)
{
}

void GB_BeginFrame(void)
{
    gb_clear_aux();
    gb_col_x = -1;
    gb_col_du = 0.0f;
    gb_col_dv = 0.0f;
    GB_BeginDisplay();
}

void GB_BeginDisplay(void)
{
    gb_scene_captured = 0;
    gb_overlay_drawing = 0;
    gb_frame.scene_valid = 0;
    memset(gb_overlay_mask, 0, sizeof(gb_overlay_mask));
}

void GB_InvalidateScene(void)
{
    gb_scene_captured = 0;
    gb_overlay_drawing = 0;
    gb_frame.scene_valid = 0;
    memset(gb_overlay_mask, 0, sizeof(gb_overlay_mask));
    gb_clear_aux();
}

void GB_MarkOverlayColumn(int x, int y, int count)
{
    int end = y + count;
    if (!gb_overlay_drawing || (unsigned)x >= GB_WIDTH || count <= 0) return;
    if (y < 0) y = 0;
    if (end > GB_HEIGHT) end = GB_HEIGHT;
    for (; y < end; ++y) gb_overlay_mask[y * GB_WIDTH + x] = 1;
}

void GB_MarkOverlayRect(int x, int y, int width, int height)
{
    int end = x + width;
    if (x < 0) x = 0;
    if (end > GB_WIDTH) end = GB_WIDTH;
    for (; x < end; ++x) GB_MarkOverlayColumn(x, y, height);
}

void GB_SetColumn(int x, float z, float nx, float ny, float nz, int kind)
{
    gb_col_kind = kind;
    gb_col_x = x;
    gb_col_z = z;
    gb_col_nx = nx;
    gb_col_ny = ny;
    gb_col_nz = nz;
    gb_col_du = 0.0f;
    gb_col_dv = 0.0f;
}

void GB_SetObjectMotion(float du, float dv)
{
    gb_col_du = du;
    gb_col_dv = dv;
}

void GB_RequestReset(void)
{
    GB_RequestResetReason(GB_RESET_EXPLICIT);
}

void GB_RequestResetReason(unsigned int reasons)
{
    gb_reset = 1;
    gb_pending_reset |= reasons;
    gb_have_prev = 0;
}

int GB_ConsumeReset(void)
{
    int r = gb_reset;
    gb_reset = 0;
    return r;
}

/* Screen pixel of view-space (vx, vy). Same address as screens[0]. */
static int gb_screen_x(int vx)
{
    if (vx >= 0 && columnofs[vx] >= 0 && columnofs[vx] < GB_WIDTH)
	return columnofs[vx];
    return vx + viewwindowx;
}

static int gb_screen_y(int vy)
{
    if (vy >= 0 && ylookup[vy] && screens[0])
	return (int)(ylookup[vy] - screens[0]) / SCREENWIDTH;
    return vy + viewwindowy;
}

static void gb_write_pixel(int x, int y, float z, float nx, float ny, float nz)
{
    int i;
    int r, g, b;

    if ((unsigned)x >= (unsigned)GB_WIDTH || (unsigned)y >= (unsigned)GB_HEIGHT)
	return;

    i = y * GB_WIDTH + x;
    gb_scene_mask[i] = z > 0.0f && z < GB_FAR_Z;
    gb_depth[i] = z;
    gb_surface_kind[i] = (unsigned char)gb_col_kind;
    gb_obj_du[i] = gb_col_du;
    gb_obj_dv[i] = gb_col_dv;

    r = (int)((nx * 0.5f + 0.5f) * 255.0f + 0.5f);
    g = (int)((ny * 0.5f + 0.5f) * 255.0f + 0.5f);
    b = (int)((nz * 0.5f + 0.5f) * 255.0f + 0.5f);
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    gb_normal[i * 4 + 0] = (unsigned char)r;
    gb_normal[i * 4 + 1] = (unsigned char)g;
    gb_normal[i * 4 + 2] = (unsigned char)b;
    gb_normal[i * 4 + 3] = 255;
}

void GB_WriteColumn(int x, int yl, int yh)
{
    int y;
    int sx;

    if (gb_overlay_drawing) {
        GB_MarkOverlayColumn(gb_screen_x(x), gb_screen_y(yl), yh - yl + 1);
        return;
    }
    if (x != gb_col_x)
        return;
    if (yl < 0)
	yl = 0;
    if (viewheight > 0 && yh >= viewheight)
	yh = viewheight - 1;
    sx = gb_screen_x(x);
    for (y = yl; y <= yh; y++)
	gb_write_pixel(sx, gb_screen_y(y), gb_col_z,
		       gb_col_nx, gb_col_ny, gb_col_nz);
}

void GB_WriteSpan(int y, int x1, int x2, float z, float nx, float ny, float nz)
{
    int x;
    int sy;

    if (y < 0)
	return;
    if (viewheight > 0 && y >= viewheight)
	return;
    if (x1 < 0)
	x1 = 0;
    if (viewwidth > 0 && x2 >= viewwidth)
	x2 = viewwidth - 1;
    if (gb_overlay_drawing) {
        GB_MarkOverlayRect(gb_screen_x(x1), gb_screen_y(y), x2 - x1 + 1, 1);
        return;
    }
    gb_col_du = 0.0f;
    gb_col_dv = 0.0f;
    gb_col_kind = ny > 0 ? GB_KIND_FLOOR : GB_KIND_CEILING;
    sy = gb_screen_y(y);
    for (x = x1; x <= x2; x++)
	gb_write_pixel(gb_screen_x(x), sy, z, nx, ny, nz);
}

static float gb_bam_to_rad(angle_t a)
{
    return (float)((double)a * (6.283185307179586 / 4294967296.0));
}

static void gb_view_rect(int *x0, int *y0, int *x1, int *y1)
{
    int vw = viewwidth;
    int vh = viewheight;

    if (vw < 1)
	vw = GB_WIDTH;
    if (vh < 1)
	vh = GB_HEIGHT;
    *x0 = gb_screen_x(0);
    *y0 = gb_screen_y(0);
    *x1 = *x0 + vw - 1;
    *y1 = *y0 + vh - 1;
    if (*x0 < 0)
	*x0 = 0;
    if (*y0 < 0)
	*y0 = 0;
    if (*x1 >= GB_WIDTH)
	*x1 = GB_WIDTH - 1;
    if (*y1 >= GB_HEIGHT)
	*y1 = GB_HEIGHT - 1;
}

void GB_CaptureScene(const unsigned char *src8)
{
    int x, y, x0, y0, x1, y1;
    if (!src8) return;
    memcpy(gb_scene8, src8, sizeof(gb_scene8));
    gb_view_rect(&x0, &y0, &x1, &y1);
    for (y = 0; y < GB_HEIGHT; ++y) {
        for (x = 0; x < GB_WIDTH; ++x) {
            int i = y * GB_WIDTH + x;
            if (x >= x0 && x <= x1 && y >= y0 && y <= y1) continue;
            /* Upscaler padding has a far-depth sentinel and zero velocity.
             * It is never valid ray-tracing geometry or a fake UI surface. */
            gb_depth[i] = GB_FAR_Z;
            gb_scene_mask[i] = 0;
            gb_obj_du[i] = gb_obj_dv[i] = 0.0f;
            memset(gb_normal + i * 4, 0, 4);
            gb_scene8[i] = src8[(y < y0 ? y0 : y > y1 ? y1 : y) * GB_WIDTH +
                                  (x < x0 ? x0 : x > x1 ? x1 : x)];
            if (gb_hud_visible) gb_overlay_mask[i] = 1;
        }
    }
    gb_scene_captured = 1;
    gb_overlay_drawing = 1;
}

static void gb_capture_camera(GB_CameraSample *camera)
{
    camera->position[0] = (float)viewx / FRACUNIT;
    camera->position[1] = (float)viewz / FRACUNIT;
    camera->position[2] = (float)viewy / FRACUNIT;
    camera->yaw = viewangle;
    camera->forward_cos = (float)cos(gb_bam_to_rad(viewangle));
    camera->forward_sin = (float)sin(gb_bam_to_rad(viewangle));
    camera->center_x = (float)centerx;
    camera->center_y = (float)centeryfrac / FRACUNIT;
    camera->projection = (float)projection / FRACUNIT;
}

void GB_BeginScene(void)
{
    gb_capture_camera(&gb_frame.base);
}

void GB_CaptureSampling(float jitter_x, float jitter_y)
{
    int x, y;
    gb_capture_camera(&gb_frame.sampled);
    gb_frame.viewport_x = gb_screen_x(0);
    gb_frame.viewport_y = gb_screen_y(0);
    gb_frame.viewport_width = viewwidth;
    gb_frame.viewport_height = viewheight;
    gb_frame.detail_shift = detailshift;
    gb_frame.render_width = GB_WIDTH; gb_frame.render_height = GB_HEIGHT;
    gb_frame.output_width = GB_WIDTH * 4; gb_frame.output_height = GB_HEIGHT * 4;
    gb_frame.jitter_x = jitter_x; gb_frame.jitter_y = jitter_y;
    for (x = 0; x < viewwidth && x < GB_WIDTH; x++) {
        unsigned angle = (viewangle + xtoviewangle[x]) >> ANGLETOFINESHIFT;
        float scale = (float)distscale[x] / FRACUNIT;
        gb_frame.ray_x[x] = (float)finecosine[angle] / FRACUNIT * scale;
        gb_frame.ray_z[x] = (float)finesine[angle] / FRACUNIT * scale;
    }
    for (y = 0; y < viewheight && y < GB_HEIGHT; y++) {
        float up = gb_frame.sampled.center_y - (float)y;
        gb_frame.ray_up_column[y] = up / gb_frame.sampled.projection;
        gb_frame.ray_up_plane[y] = (up > 0.5f ? 1.0f : -1.0f) /
                                   ((float)yslope[y] / FRACUNIT);
    }
}

void GB_SetFrameTiming(int game_tic, float delta_ms, int fixed_timeline,
                       int menu_open, int paused_now, int map_episode, int map_number)
{
    gb_frame.game_tic = game_tic;
    gb_frame.map_episode = map_episode; gb_frame.map_number = map_number;
    gb_frame.frame_delta_ms = delta_ms;
    gb_frame.fixed_timeline = fixed_timeline;
    if (gb_have_state && menu_open != gb_menu_open) GB_RequestResetReason(GB_RESET_MENU);
    if (gb_have_state && paused_now != gb_paused) GB_RequestResetReason(GB_RESET_PAUSE);
    gb_menu_open = menu_open; gb_paused = paused_now; gb_have_state = 1;
}

const GB_FrameInputs *GB_GetFrameInputs(void) { return &gb_frame; }
const unsigned char *GB_SurfaceKind(void) { return gb_surface_kind; }

int GB_SampleRay(int screen_x, int screen_y, float ray_world[3])
{
    int i, x = screen_x - gb_frame.viewport_x, y = screen_y - gb_frame.viewport_y;
    if (!ray_world || !gb_frame.scene_valid || (unsigned)screen_x >= GB_WIDTH ||
        (unsigned)screen_y >= GB_HEIGHT || x < 0 || y < 0 ||
        x >= gb_frame.viewport_width || y >= gb_frame.viewport_height) return 0;
    i = screen_y * GB_WIDTH + screen_x;
    if (!gb_scene_mask[i]) return 0;
    ray_world[0] = gb_frame.ray_x[x]; ray_world[2] = gb_frame.ray_z[x];
    ray_world[1] = (gb_surface_kind[i] == GB_KIND_FLOOR || gb_surface_kind[i] == GB_KIND_CEILING) ?
                   gb_frame.ray_up_plane[y] : gb_frame.ray_up_column[y];
    return 1;
}

int GB_SampleWorldPosition(int x, int y, float world[3])
{
    float ray[3]; int c;
    if (!world || !GB_SampleRay(x, y, ray)) return 0;
    for (c = 0; c < 3; c++) world[c] = gb_frame.sampled.position[c] + ray[c] * gb_depth[y * GB_WIDTH + x];
    return 1;
}

void GB_EndFrame(void)
{
    int x, y;
    gb_frame.frame_id++;
    gb_frame.scene_valid = gb_scene_captured;
    for (x = 0; x < GB_PIX; x++)
        gb_temporal_depth[x] = GB_DeviceDepthFromViewDepth(gb_depth[x],
                              gb_scene_captured && gb_scene_mask[x]);
    if (!gb_scene_captured) gb_have_prev = 0;
    if (gb_have_prev) {
        float dx = gb_frame.base.position[0] - gb_previous.base.position[0];
        float dz = gb_frame.base.position[1] - gb_previous.base.position[1];
        float dy = gb_frame.base.position[2] - gb_previous.base.position[2];
        int32_t yaw_delta = (int32_t)(gb_frame.base.yaw - gb_previous.base.yaw);
        if (dx * dx + dy * dy + dz * dz > 128.0f * 128.0f ||
            fabs((double)yaw_delta) > 1073741824.0 || gb_frame.game_tic < gb_previous.game_tic ||
            gb_frame.frame_delta_ms > 250.0f) GB_RequestResetReason(GB_RESET_CAMERA_CUT);
        if (gb_frame.viewport_x != gb_previous.viewport_x || gb_frame.viewport_y != gb_previous.viewport_y ||
            gb_frame.viewport_width != gb_previous.viewport_width || gb_frame.viewport_height != gb_previous.viewport_height)
            GB_RequestResetReason(GB_RESET_VIEW);
    }
    gb_frame.history_valid = gb_have_prev;
    gb_frame.reset_reasons = gb_pending_reset;
    gb_pending_reset = 0;
    memset(gb_velocity, 0, sizeof(gb_velocity));
    if (gb_have_prev) for (y = 0; y < GB_HEIGHT; y++) for (x = 0; x < GB_WIDTH; x++) {
        int i = y * GB_WIDTH + x;
        float world[3], motion[2];
        if (GB_SampleWorldPosition(x, y, world) && GB_ProjectMotion(&gb_frame, &gb_previous, world, world, motion)) {
            int tics = gb_frame.game_tic - gb_previous.game_tic;
            gb_velocity[i * 2] = motion[0] + gb_obj_du[i] * tics;
            gb_velocity[i * 2 + 1] = motion[1] + gb_obj_dv[i] * tics;
        }
    }
    if (gb_scene_captured) { gb_previous = gb_frame; gb_have_prev = 1; }
}

void GB_SetPaletteRGB(const unsigned char *rgb768)
{
    memcpy(gb_palette, rgb768, 256 * 3);
}
void GB_SetBasePaletteRGB(const unsigned char *palette) { memcpy(gb_base_palette,palette,768); }
void GB_SetRawPaletteRGB(const unsigned char *palette, const unsigned char *gamma)
{
    memcpy(gb_raw_palette,palette,768); memcpy(gb_gamma,gamma,256);
}
const unsigned char *GB_BasePaletteRGB(void) { return gb_base_palette; }
const unsigned char *GB_RawPaletteRGB(void) { return gb_raw_palette; }
const unsigned char *GB_GammaLUT(void) { return gb_gamma; }
void GB_SetMaterialContext(int kind, unsigned int id)
{
    gb_material_kind=kind; gb_material_id=id;
}
void GB_RecordMaterialSample(int offset, unsigned char source, unsigned char ambient)
{
    GB_MaterialSample *sample;
    if(gb_overlay_drawing || (unsigned)offset>=GB_PIX) return;
    sample=&gb_material[offset];
    sample->source_index=source; sample->ambient_index=ambient;
    sample->kind=(unsigned char)gb_material_kind; sample->material_id=gb_material_id;
    sample->valid=gb_material_kind>=GB_KIND_WALL && gb_material_kind<=GB_KIND_CEILING && !fixedcolormap;
}
const GB_MaterialSample *GB_MaterialSamples(void) { return gb_material; }

void GB_ConvertColor(const unsigned char *src8)
{
    int i;
    const unsigned char *scene = gb_scene_captured ? gb_scene8 : src8;
    if (gb_last_scene != gb_scene_captured) GB_RequestResetReason(GB_RESET_SCENE);
    gb_last_scene = gb_scene_captured;
    if (!gb_scene_captured) gb_clear_aux();
    for (i = 0; i < GB_PIX; i++) {
        const unsigned char *p = gb_palette + scene[i] * 3;
        const unsigned char *overlay = gb_palette + src8[i] * 3;
        gb_color[i * 4 + 0] = p[0];
        gb_color[i * 4 + 1] = p[1];
        gb_color[i * 4 + 2] = p[2];
        gb_color[i * 4 + 3] = 255;
        memcpy(gb_overlay_color + i * 4, overlay, 3);
        gb_overlay_color[i * 4 + 3] = 255;
    }
}

void GB_SetDebugView(int view)
{
    if (view < GB_VIEW_COLOR)
	view = GB_VIEW_COLOR;
    if (view > GB_VIEW_MATERIAL_MASK)
        view = GB_VIEW_MATERIAL_MASK;
    if (gb_debug_view != view) GB_RequestResetReason(GB_RESET_VIEW);
    gb_debug_view = view;
}

int GB_GetDebugView(void)
{
    return gb_debug_view;
}

void GB_ToggleHud(void)
{
    gb_hud_visible = !gb_hud_visible;
    GB_RequestReset();
}

int GB_HudVisible(void)
{
    return gb_hud_visible;
}

const unsigned char *GB_ColorRGBA(void)
{
    return gb_color;
}

const unsigned char *GB_OverlayRGBA(void) { return gb_overlay_color; }
const unsigned char *GB_SceneMask(void) { return gb_scene_mask; }
const unsigned char *GB_OverlayMask(void) { return gb_overlay_mask; }

const float *GB_Depth(void)
{
    return gb_depth;
}

const float *GB_TemporalDepth(void)
{
    return gb_temporal_depth;
}

const unsigned char *GB_NormalRGBA(void)
{
    return gb_normal;
}

const float *GB_VelocityRG(void)
{
    return gb_velocity;
}

static unsigned char gb_clamp_u8(int v)
{
    if (v < 0)
	return 0;
    if (v > 255)
	return 255;
    return (unsigned char)v;
}

void GB_ComposePresent(unsigned char *dst_bgra, int dst_w, int dst_h)
{
    int x, y;
    int sx, sy;
    int src;

    if (dst_w < 1 || dst_h < 1)
	return;

    for (y = 0; y < dst_h; y++)
    {
	sy = y * GB_HEIGHT / dst_h;
	for (x = 0; x < dst_w; x++)
	{
	    unsigned char *d;
	    unsigned char r = 0, g = 0, b = 0;

	    sx = x * GB_WIDTH / dst_w;
	    src = sy * GB_WIDTH + sx;
	    switch (gb_debug_view)
            {
              case GB_VIEW_ALBEDO:
                r=gb_material[src].valid && gb_scene_mask[src] ? gb_base_palette[gb_material[src].source_index*3] : 0;
                g=gb_material[src].valid && gb_scene_mask[src] ? gb_base_palette[gb_material[src].source_index*3+1] : 0;
                b=gb_material[src].valid && gb_scene_mask[src] ? gb_base_palette[gb_material[src].source_index*3+2] : 0;
                break;
              case GB_VIEW_MATERIAL_MASK:
                r=g=b=gb_material[src].valid && gb_scene_mask[src] ? 255 : 0;
                break;
              case GB_VIEW_SCENE_MASK:
                r = g = b = gb_scene_mask[src] ? 255 : 0;
                break;
              case GB_VIEW_OVERLAY_MASK:
                r = g = b = gb_overlay_mask[src] ? 255 : 0;
                break;
	      case GB_VIEW_DEPTH:
	      {
		  float z = gb_depth[src];
		  float t = (!gb_scene_mask[src]) ? 0.0f : (1.0f / (1.0f + z / 256.0f));
		  r = g = b = gb_clamp_u8((int)(t * 255.0f + 0.5f));
		  break;
	      }
	      case GB_VIEW_NORMAL:
                  r = gb_scene_mask[src] ? gb_normal[src * 4 + 0] : 0;
                  g = gb_scene_mask[src] ? gb_normal[src * 4 + 1] : 0;
                  b = gb_scene_mask[src] ? gb_normal[src * 4 + 2] : 0;
		  break;
	      case GB_VIEW_VELOCITY:
		  r = gb_clamp_u8((int)(128.0f + gb_velocity[src * 2 + 0] * 8.0f));
		  g = gb_clamp_u8((int)(128.0f + gb_velocity[src * 2 + 1] * 8.0f));
		  b = 128;
		  break;
	      default:
		  r = gb_color[src * 4 + 0];
		  g = gb_color[src * 4 + 1];
		  b = gb_color[src * 4 + 2];
		  break;
	    }

	    d = dst_bgra + (y * dst_w + x) * 4;
	    d[0] = b;
	    d[1] = g;
	    d[2] = r;
            d[3] = 255;
	}
    }
    if (gb_debug_view == GB_VIEW_COLOR) GB_OverlayHud(dst_bgra, dst_w, dst_h);
}

int GB_HasScenePixels(void)
{
    int i;

    if (!gb_scene_captured) return 0;
    for (i = 0; i < GB_PIX; i++)
    {
        if (gb_scene_mask[i])
	    return 1;
    }
    return 0;
}

void GB_OverlayHud(unsigned char *dst_bgra, int dst_w, int dst_h)
{
    int x, y;
    int sx, sy;
    int src;

    if (!dst_bgra || dst_w < 1 || dst_h < 1)
	return;

    for (y = 0; y < dst_h; y++)
    {
	sy = y * GB_HEIGHT / dst_h;
	for (x = 0; x < dst_w; x++)
	{
	    unsigned char *d;

	    sx = x * GB_WIDTH / dst_w;
	    src = sy * GB_WIDTH + sx;
	    if (!gb_overlay_mask[src])
                continue;
	    d = dst_bgra + (y * dst_w + x) * 4;
	    d[0] = gb_overlay_color[src * 4 + 2];
            d[1] = gb_overlay_color[src * 4 + 1];
            d[2] = gb_overlay_color[src * 4 + 0];
	    d[3] = 255;
	}
    }
}
