/* Real software patch + public G-buffer composition contract. GPLv2. */
#include "doomdef.h"
#include "r_local.h"
#include "v_video.h"
#include "gbuffer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int viewwidth = 320, viewheight = 168, viewwindowx, viewwindowy;
int centerx = 160, centery = 84;
fixed_t projection = 160 * FRACUNIT;
fixed_t viewx, viewy, viewz;
angle_t viewangle, xtoviewangle[SCREENWIDTH + 1];
int columnofs[SCREENWIDTH];
byte *ylookup[SCREENHEIGHT];
void V_DrawPatchFlipped(int x, int y, int scrn, patch_t *patch);
void I_Error(char *fmt, ...) { (void)fmt; abort(); }
byte *I_AllocLow(int length) { return calloc(1, (size_t)length); }

int main(void)
{
    unsigned char palette[768] = {0};
    unsigned char *present = malloc(320 * 200 * 4);
    unsigned char patch_bytes[32] = {0};
    patch_t *patch = (patch_t *)patch_bytes;
    int x, y, scene = 100 * 320 + 100, post = 100 * 320 + 101;
    assert(present);
    screens[0] = calloc(1, 320 * 200);
    screens[1] = calloc(1, 320 * 200);
    assert(screens[0] && screens[1]);
    palette[3] = 10; palette[4] = 20; palette[5] = 30;
    palette[6] = 60; palette[7] = 70; palette[8] = 80;
    memset(screens[0], 1, 320 * 200);
    for (x = 0; x < 320; ++x) columnofs[x] = x;
    for (y = 0; y < 200; ++y) ylookup[y] = screens[0] + y * 320;
    patch->width = patch->height = 1;
    patch->columnofs[0] = 12;
    patch_bytes[12] = 0; patch_bytes[13] = 1; patch_bytes[15] = 1;
    patch_bytes[17] = 255;

    GB_Init(); GB_SetPaletteRGB(palette); GB_BeginFrame();
    GB_SetColumn(100, 256, 1, 0, 0, GB_KIND_WALL);
    GB_WriteColumn(100, 100, 100);
    GB_CaptureScene(screens[0]);
    /* Simulate an exactly equal-color weapon post: still explicit coverage. */
    GB_SetColumn(100, 10, 0, 1, 0, GB_KIND_SPRITE);
    GB_WriteColumn(100, 100, 100);
    assert(GB_Depth()[scene] == 256);
    V_DrawPatch(101, 100, 0, patch);
    V_DrawPatch(102, 100, 1, patch);
    V_DrawPatchFlipped(103, 100, 0, patch);
    V_CopyRect(0, 0, 1, 2, 1, 104, 100, 0);
    {
        byte block[2] = {1, 1};
        V_DrawBlock(106, 100, 0, 2, 1, block);
    }
    assert(GB_OverlayMask()[100 * 320 + 103]);
    assert(GB_OverlayMask()[100 * 320 + 104] && GB_OverlayMask()[100 * 320 + 105]);
    assert(GB_OverlayMask()[100 * 320 + 106] && GB_OverlayMask()[100 * 320 + 107]);
    assert(!GB_OverlayMask()[101 * 320 + 101]); /* patch transparent gap */
    assert(GB_OverlayMask()[scene] && GB_OverlayMask()[post]);
    assert(!GB_OverlayMask()[100 * 320 + 102]);
    screens[0][scene] = 2; /* weapon differs, pre-weapon input must stay index 1 */
    GB_ConvertColor(screens[0]); GB_EndFrame();
    assert(GB_ColorRGBA()[scene * 4] == 10);
    assert(GB_OverlayRGBA()[scene * 4] == 60);
    assert(GB_SceneMask()[scene]);
    assert(!GB_SceneMask()[190 * 320 + 100]);
    assert(GB_Depth()[190 * 320 + 100] == 8192);
    assert(GB_VelocityRG()[(190 * 320 + 100) * 2] == 0);
    assert(GB_HasScenePixels());
    memset(present, 200, 320 * 200 * 4);
    GB_OverlayHud(present, 320, 200);
    assert(present[post * 4] == 30 && present[post * 4 + 2] == 10);
    assert(present[scene * 4] == 80 && present[scene * 4 + 2] == 60);
    assert(present[(100 * 320 + 102) * 4] == 200);
    GB_ComposePresent(present, 320, 200);
    assert(present[scene * 4] == 80); /* complete nearest composition */
    GB_SetDebugView(GB_VIEW_DEPTH); GB_ComposePresent(present, 320, 200);
    assert(present[scene * 4] == 128); /* true scene remains behind weapon */
    assert(present[(190 * 320 + 100) * 4] == 0);
    GB_SetDebugView(GB_VIEW_OVERLAY_MASK); GB_ComposePresent(present, 320, 200);
    assert(present[scene * 4] == 255 && present[(100 * 320 + 102) * 4] == 0);
    GB_ToggleHud(); GB_BeginFrame();
    GB_SetColumn(100, 256, 1, 0, 0, GB_KIND_WALL); GB_WriteColumn(100, 100, 100);
    GB_CaptureScene(screens[0]);
    assert(!GB_OverlayMask()[190 * 320 + 100]); /* Insert hides status bar/padding */
    V_DrawPatch(101, 100, 0, patch);
    assert(GB_OverlayMask()[post]); /* menu/weapon coverage still visible */
    GB_InvalidateScene(); GB_ConvertColor(screens[0]); GB_EndFrame();
    assert(!GB_HasScenePixels() && !GB_SceneMask()[scene] && GB_Depth()[scene] == 0);
    GB_SetDebugView(GB_VIEW_COLOR); GB_ComposePresent(present, 320, 200);
    assert(present[scene * 4] == 80); /* non-scene final frame is exact */
    free(present); free(screens[0]); free(screens[1]);
    puts("scene/explicit-overlay/real-patch composition tests passed");
    return 0;
}
