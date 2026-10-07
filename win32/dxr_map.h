/* One scene owner. Loaned views expire at unload; renderer fences before access.
   No engine map pointers. GPLv2. */
#ifndef WINDOOM_DXR_MAP_H
#define WINDOOM_DXR_MAP_H
#include "map_mesh.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint32_t instance_id, geometry_index, triangle_base, triangle_count; } MapGeometryRange;
typedef struct {
 const MapMesh *mesh;
 void *tlas_resource;
 uint64_t tlas_gpu_address, generation;
 const MapGeometryRange *geometry_ranges;
 unsigned geometry_count;
 void *primitive_resource, *geometry_resource, *vertex_resource, *index_resource;
} DxrMapSceneView;
void DxrMap_Init(void *device, void *queue, unsigned width, unsigned height,
                 int mode, int disabled); /* mode 0=off,1=depth,2=normal */
/* Ordinary RT consumers request the same scene independently of diagnostics. */
void DxrMap_RequestScene(int requested);
int DxrMap_Available(void);
void DxrMap_Prepare(void); /* caller has waited renderer queue */
int DxrMap_Render(void *commands, void *backbuffer); /* COPY_DEST, preserves UI */
void DxrMap_Toggle(void);
void DxrMap_Unload(void); /* waits own shared-queue fence before level data free */
void DxrMap_LevelLoaded(const char *name);
void DxrMap_Shutdown(void);
const DxrMapSceneView *DxrMap_GetScene(void);
/* Geometry0 uses mesh triangle order; future descriptors need an explicit map. */
const MapSurface *DxrMap_GetSurface(unsigned instance_id, unsigned geometry_index, unsigned primitive_index);
#ifdef __cplusplus
}
#endif
#endif
