#ifndef WINDOOM_MAP_SOURCE_H
#define WINDOOM_MAP_SOURCE_H
#include "map_mesh.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint64_t geometry, material, lighting; unsigned sectors, sides; } MapWorldRevision;
/* Bounded final-world survey: no retained engine pointers or geometry extraction. */
int MapSource_Survey(MapWorldRevision *revision);
void MapSource_UpdateLightLevels(MapMesh *mesh);
int MapSource_GetSectorHeights(unsigned sector, double *floor, double *ceiling);
/* Snapshot current, fully loaded/restored world. Does not retain PU_LEVEL data. */
int MapSource_Capture(uint64_t generation, MapMesh *mesh, char *diagnostic, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
