/* Bitmap glyph overlay marking test. */
#include "v_video.h"
#include "m_bbox.h"
#include <assert.h>
#include <string.h>

static byte screen[SCREENWIDTH * SCREENHEIGHT];
static unsigned char palette[256 * 3];
static int provide_palette = 1;
static int marks, mark_x[8], mark_y[8], mark_h[8];

const unsigned char *GB_BasePaletteRGB(void)
{
    return provide_palette ? palette : NULL;
}

void M_AddToBox(int box[4], int x, int y)
{
    if (x < box[BOXLEFT]) box[BOXLEFT] = x;
    if (x > box[BOXRIGHT]) box[BOXRIGHT] = x;
    if (y < box[BOXTOP]) box[BOXTOP] = y;
    if (y > box[BOXBOTTOM]) box[BOXBOTTOM] = y;
}

void GB_MarkOverlayColumn(int x, int y, int count)
{
    assert(marks < 8);
    mark_x[marks] = x; mark_y[marks] = y; mark_h[marks] = count; ++marks;
}

static void setbit(unsigned char *mask, int row, int col)
{
    unsigned bit = (unsigned)(row * 12 + col);
    mask[bit / 8] |= (unsigned char)(0x80 >> (bit & 7));
}

int main(void)
{
    struct { patch_t patch; unsigned char data[7]; } reference;
    unsigned char mask[18] = {0};
    memset(&reference, 0, sizeof(reference));
    memset(screen, 0, sizeof(screen));
    memset(dirtybox, 0, sizeof(dirtybox));
    reference.patch.width = 1;
    reference.patch.height = 2;
    reference.patch.columnofs[0] = sizeof(patch_t);
    reference.data[0] = 0; reference.data[1] = 2;
    reference.data[3] = 5; reference.data[4] = 6; reference.data[6] = 0xff;
    palette[5 * 3] = 92; palette[5 * 3 + 1] = 1; palette[5 * 3 + 2] = 1;
    palette[6 * 3] = 255; palette[6 * 3 + 1] = 1; palette[6 * 3 + 2] = 1;
    screens[0] = screen;
    setbit(mask, 1, 2); setbit(mask, 3, 2); setbit(mask, 4, 2); setbit(mask, 7, 5);
    V_DrawCjkGlyph(10, 20, mask, &reference.patch);
    assert(screen[21 * SCREENWIDTH + 12] == 6);
    assert(screen[23 * SCREENWIDTH + 12] == 6);
    assert(screen[24 * SCREENWIDTH + 12] == 6);
    assert(screen[27 * SCREENWIDTH + 15] == 6);
    assert(screen[20 * SCREENWIDTH + 10] == 0);
    assert(marks == 3);
    assert(mark_x[0] == 12 && mark_y[0] == 21 && mark_h[0] == 1);
    assert(mark_x[1] == 12 && mark_y[1] == 23 && mark_h[1] == 2);
    assert(mark_x[2] == 15 && mark_y[2] == 27 && mark_h[2] == 1);
    memset(screen, 0, sizeof(screen));
    marks = 0;
    provide_palette = 0; /* Without PLAYPAL, preserve the legacy first ink. */
    V_DrawCjkGlyph(10, 20, mask, &reference.patch);
    assert(screen[21 * SCREENWIDTH + 12] == 5);
    assert(marks == 3);
    assert(mark_x[0] == 12 && mark_y[0] == 21 && mark_h[0] == 1);
    assert(mark_x[1] == 12 && mark_y[1] == 23 && mark_h[1] == 2);
    assert(mark_x[2] == 15 && mark_y[2] == 27 && mark_h[2] == 1);
    provide_palette = 1;
    marks = 0;
    V_DrawCjkGlyph(-1, -1, mask, &reference.patch); /* clipped at screen edges */
    V_DrawCjkGlyph(SCREENWIDTH - 1, SCREENHEIGHT - 1, mask, &reference.patch);
    return 0;
}
