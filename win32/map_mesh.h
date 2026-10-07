/* Owned current-map snapshot; map XY is converted to world X,height,Y.
   No engine/WAD pointers survive Build. GPLv2; see LICENSE.TXT. */
#ifndef WINDOOM_MAP_MESH_H
#define WINDOOM_MAP_MESH_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MAP_CHILD_LEAF 0x8000u
#define MAP_SIDE_NONE UINT32_MAX
#define MAP_SURFACE_NONE UINT32_MAX
#define MAP_FLAG_TWOSIDED 4u
#define MAP_FLAG_DONTPEGTOP 8u
#define MAP_FLAG_DONTPEGBOTTOM 16u

enum { MAP_WALL_MID, MAP_WALL_UPPER, MAP_WALL_LOWER, MAP_FLOOR, MAP_CEILING };
typedef struct { double x, y; } MapPoint;
typedef struct { double floor_height, ceiling_height; int floor_material, ceiling_material; int floor_sky, ceiling_sky; int32_t lightlevel; } MapSector;
typedef struct { uint32_t sector; double x_offset, y_offset; int material[3]; double texture_height[3]; } MapSide;
typedef struct { uint32_t vertex[2], side[2], flags; } MapLine;
typedef struct { uint32_t vertex[2], line, side; double offset; } MapSeg;
typedef struct { uint32_t first_seg, seg_count, sector; } MapLeaf;
typedef struct { double x,y,dx,dy; uint32_t child[2]; } MapNode;
typedef struct {
 const MapPoint *points; size_t point_count;
 const MapSector *sectors; size_t sector_count;
 const MapSide *sides; size_t side_count;
 const MapLine *lines; size_t line_count;
 const MapSeg *segs; size_t seg_count;
 const MapLeaf *leaves; size_t leaf_count;
 const MapNode *nodes; size_t node_count;
 uint64_t generation;
} MapInput;
typedef struct { float position[3], uv[2]; } MapMeshVertex;
/* Stable id is the slot: line*6+side*3+tier; planes after all line slots.
   Both normal conventions are fixed in map space, never camera-faceforward. */
typedef struct {
 uint32_t active, kind, line, side, sector, back_sector, flags;
 int32_t base_material;
 float legacy_normal[3], inward_normal[3];
 float x_offset, y_offset, v_anchor, texture_height;
 uint32_t triangle_count; /* may span several leaves; use triangle_surfaces */
 uint32_t masked; /* Two-sided finite midtexture; nonopaque AS geometry. */
 int32_t lightlevel; /* Current copied owner sector; never an engine pointer. */
} MapSurface;
typedef struct {
 MapMeshVertex *vertices; uint32_t *indices, *triangle_surfaces;
 MapSurface *surfaces; double *leaf_areas;
 size_t vertex_count, triangle_count, surface_count, leaf_count;
 uint64_t generation;
 uint32_t empty_leaves;
} MapMesh;
/* Fails transactionally with a bounded diagnostic, leaving output empty.
   Caller supplies an empty mesh and owns MapMesh_Free after a successful build. */
int MapMesh_Build(const MapInput *input, MapMesh *output, char *diagnostic, size_t capacity);
void MapMesh_Free(MapMesh *mesh);
uint32_t MapMesh_WallSurface(size_t line, unsigned side, unsigned tier);
uint32_t MapMesh_PlaneSurface(size_t line_count, size_t sector, int ceiling);
#ifdef __cplusplus
}
#endif
#endif
