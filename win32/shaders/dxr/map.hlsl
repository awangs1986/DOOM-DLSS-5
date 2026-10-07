// Real static-map primary rays from the frozen software sampling contract.
RaytracingAccelerationStructure scene : register(t0);
#include "scene-hit.hlsli"
StructuredBuffer<ScenePrimitive> primitives : register(t1);
StructuredBuffer<float4> rays : register(t2); // xyz per GB depth, w reference depth
StructuredBuffer<uint> flags : register(t3); // valid1, overlay2
StructuredBuffer<uint> original : register(t4); // full-output BGRA
StructuredBuffer<SceneGeometryRange> geometryRanges : register(t5);
RWStructuredBuffer<uint> pixels : register(u0);
struct Hit { float depth; float3 normal; uint surface; uint primitive; uint geometry; uint instance; uint valid; uint reserved; };
RWStructuredBuffer<Hit> hits : register(u1);
cbuffer Frame : register(b0) {
 uint width, height, rowWords, mode;
 float3 origin; uint rangeCount;
 float3 offscreenDirection; float unused2;
};
Hit trace(float3 direction) {
 Hit hit=(Hit)0; hit.surface=hit.primitive=hit.geometry=0xffffffff;
 RayDesc ray;ray.Origin=origin;ray.Direction=direction;ray.TMin=0.001;ray.TMax=8192;
 RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
 query.TraceRayInline(scene,RAY_FLAG_NONE,0xff,ray);
 while(query.Proceed()) {}
 if(query.CommittedStatus()==COMMITTED_TRIANGLE_HIT) {
  hit.valid=1;hit.depth=query.CommittedRayT();hit.primitive=query.CommittedPrimitiveIndex();hit.geometry=query.CommittedGeometryIndex();
  hit.instance=query.CommittedInstanceID();
  uint triangleIndex=sceneTriangleIndex(hit.instance,hit.geometry,hit.primitive,geometryRanges,rangeCount);
  if(triangleIndex==0xffffffff){hit.valid=0;return hit;}
  ScenePrimitive p=primitives[triangleIndex];hit.surface=p.surface;hit.normal=p.legacyNormal;
 }
 return hit;
}
[numthreads(8,8,1)]
void main(uint3 thread:SV_DispatchThreadID) {
 if(thread.x>=320||thread.y>=200)return;
 uint low=thread.y*320+thread.x;
 Hit hit=(Hit)0;hit.surface=hit.primitive=hit.geometry=0xffffffff;
 if(flags[low]&1)hit=trace(rays[low].xyz);
 hits[low]=hit;
 uint3 rgb=uint3(255,0,255); // eligible miss makes missing geometry visible
 if(hit.valid) {
  if(mode==2)rgb=uint3(clamp(hit.normal*0.5+0.5,0,1)*255+0.5);
  else {uint gray=uint(255/(1+hit.depth/256)+0.5);rgb=uint3(gray,gray,gray);}
 }
 uint packed=rgb.b|(rgb.g<<8)|(rgb.r<<16)|0xff000000;
 uint scaleX=width/320,scaleY=height/200;
 for(uint sy=0;sy<scaleY;sy++)for(uint sx=0;sx<scaleX;sx++) {
  uint x=thread.x*scaleX+sx,y=thread.y*scaleY+sy;
  uint originalIndex=y*width+x;
  pixels[y*rowWords+x]=((flags[low]&3)==1)?packed:original[originalIndex];
 }
 // Independent reverse-camera ray: geometry is not collected from visible segs.
 if(low==0)hits[64000]=trace(offscreenDirection);
}
