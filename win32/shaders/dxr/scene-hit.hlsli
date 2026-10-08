// Shared hit identity seam: AS primitive indices are local to each geometry.
#ifndef WINDOOM_SCENE_HIT_HLSLI
#define WINDOOM_SCENE_HIT_HLSLI
struct SceneGeometryRange {uint instanceID;uint geometryIndex;uint triangleBase;uint triangleCount;};
struct ScenePrimitive {float3 legacyNormal;uint surface;float3 inwardNormal;uint reserved;};
struct SceneVertex {float3 position;float2 uv;};
uint sceneTriangleIndex(uint instanceID,uint geometryIndex,uint primitiveIndex,
                       StructuredBuffer<SceneGeometryRange> ranges,uint rangeCount) {
 if(rangeCount>4096)return 0xffffffff;
 for(uint i=0;i<rangeCount;i++) {
  SceneGeometryRange range=ranges[i];
  if(range.instanceID==instanceID && range.geometryIndex==geometryIndex && primitiveIndex<range.triangleCount)
   return range.triangleBase<=0xffffffff-primitiveIndex?range.triangleBase+primitiveIndex:0xffffffff;
 }
 return 0xffffffff;
}
#endif
