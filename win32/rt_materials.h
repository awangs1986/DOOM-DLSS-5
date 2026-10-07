/* One passive copied-material catalog for alpha and reflection consumers. */
#ifndef WINDOOM_RT_MATERIALS_H
#define WINDOOM_RT_MATERIALS_H
#include "dxr_map.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t base_id, resolved_id, flat_namespace, width, height, width_mask;
    uint32_t pixel_offset, pixel_count;
} RtMaterialDescriptor;
typedef struct {
    const RtMaterialDescriptor *descriptors;
    unsigned descriptor_count;
    const uint32_t *indexed_alpha; /* index low byte, binary alpha next byte */
    unsigned pixel_count;
    void *descriptor_resource, *pixel_resource;
    uint64_t generation;
} RtMaterialView;
/* Renderer has fenced before prepare/release. Views expire at next prepare. */
void RtMaterials_Init(void *device);
void RtMaterials_Prepare(const DxrMapSceneView *scene);
const RtMaterialView *RtMaterials_GetView(void);
void RtMaterials_Shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
