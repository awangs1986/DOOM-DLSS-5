#ifndef WINDOOM_MAP_SOURCE_H
#define WINDOOM_MAP_SOURCE_H
#include "map_mesh.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Snapshot current, fully loaded/restored world. Does not retain PU_LEVEL data. */
int MapSource_Capture(uint64_t generation, MapMesh *mesh, char *diagnostic, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
