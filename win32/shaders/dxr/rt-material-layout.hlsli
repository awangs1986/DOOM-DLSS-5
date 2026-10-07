#ifndef WINDOOM_RT_MATERIAL_LAYOUT_HLSLI
#define WINDOOM_RT_MATERIAL_LAYOUT_HLSLI
struct RtMaterialDescriptor {
 uint baseID,resolvedID,flatNamespace,width,height,widthMask,pixelOffset,pixelCount;
};
struct RtMaterialSurface {uint descriptorIndex,masked,kind,lightlevel;};
// Explicit counts are required: root SRVs have no descriptor numElements.
struct RtTraceBounds {
 uint rangeCount,triangleCount,vertexCount,indexCount;
 uint surfaceCount,descriptorCount,pixelCount,budget;
};
#endif
