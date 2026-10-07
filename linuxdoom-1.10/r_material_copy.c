/* Bounded classic DOOM patch-post decoder. GPLv2. */
#include "r_material.h"
#include <limits.h>
static unsigned read16(const unsigned char *p) { return p[0] | ((unsigned)p[1] << 8); }
static uint32_t read32(const unsigned char *p) {
    return p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
int R_ComposeMaterialPatch(const unsigned char *patch, size_t length,
                          int ox, int oy, unsigned width, unsigned height,
                          unsigned char *indices, unsigned char *alpha, size_t capacity)
{
    unsigned columns, patch_height, pass, x;
    size_t pixels, directory;
    if (!patch || !indices || !alpha || length < 8 || length > 64u*1024u*1024u ||
        !width || !height || width > R_MATERIAL_MAX_DIMENSION || height > R_MATERIAL_MAX_DIMENSION)
        return 0;
    pixels=(size_t)width*height;
    if (pixels > R_MATERIAL_MAX_PIXELS || capacity < pixels) return 0;
    columns=read16(patch); patch_height=read16(patch+2);
    if (!columns || !patch_height || columns > R_MATERIAL_MAX_DIMENSION ||
        patch_height > R_MATERIAL_MAX_DIMENSION) return 0;
    directory=8+(size_t)columns*4;
    if (directory > length) return 0;
    /* First pass proves every post and terminator fits; second pass copies. */
    for (pass=0;pass<2;pass++) for (x=0;x<columns;x++) {
        size_t at=read32(patch+8+(size_t)x*4), posts=0;
        if (at < directory || at >= length) return 0;
        for (;;) {
            unsigned top,count,i;
            if (at >= length || ++posts > 4096) return 0;
            top=patch[at];
            if (top==255) break;
            if (length-at < 4) return 0;
            count=patch[at+1];
            if ((size_t)count+4 > length-at || top+count > patch_height) return 0;
            if (pass) for(i=0;i<count;i++) {
                int64_t dx=(int64_t)ox+x, dy=(int64_t)oy+top+i;
                if(dx>=0 && dy>=0 && dx<width && dy<height) {
                    size_t dst=(size_t)dy*width+(size_t)dx;
                    indices[dst]=patch[at+3+i]; alpha[dst]=255;
                }
            }
            at+=(size_t)count+4;
        }
    }
    return 1;
}
