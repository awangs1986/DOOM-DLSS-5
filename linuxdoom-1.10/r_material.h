/* Copied indexed material data. No zone/WAD pointers cross this API. GPLv2. */
#ifndef DOOM_R_MATERIAL_H
#define DOOM_R_MATERIAL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define R_MATERIAL_MAX_DIMENSION 4096u
#define R_MATERIAL_MAX_PIXELS (4u * 1024u * 1024u)
typedef struct {
    unsigned id, width, height, width_mask;
    char name[9]; /* Eight WAD bytes plus an explicit terminator. */
} R_MaterialDescription;
unsigned R_ColorMapCount(void);
int R_CopyColorMap(unsigned row, unsigned char *indices, size_t capacity);
int R_DescribeTexture(unsigned id, R_MaterialDescription *out);
int R_DescribeFlat(unsigned id, R_MaterialDescription *out);
unsigned R_MaterialCount(int flat_namespace);
int R_CopyTextureIndexedAlpha(unsigned id, unsigned char *indices,
                             unsigned char *alpha, size_t capacity);
int R_CopyFlatIndexed(unsigned id, unsigned char *indices, size_t capacity);
int R_ResolveTexture(unsigned id, unsigned *resolved);
int R_ResolveFlat(unsigned id, unsigned *resolved);
/* Public bounded post copier; validates the entire patch before mutating output.
   Gaps leave earlier texture patches intact; source index zero is opaque. */
int R_ComposeMaterialPatch(const unsigned char *patch, size_t length,
                          int origin_x, int origin_y, unsigned width,
                          unsigned height, unsigned char *indices,
                          unsigned char *alpha, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
