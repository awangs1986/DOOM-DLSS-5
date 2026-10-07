#ifndef WINDOOM_ALPHA_TRACE_HLSLI
#define WINDOOM_ALPHA_TRACE_HLSLI
#include "material-sample.hlsli"
static const uint RT_TRACE_MISS=0,RT_TRACE_HIT=1,RT_TRACE_ERROR=2,RT_TRACE_EXHAUSTED=3;
struct RtTraceResult {
 uint status,triangle,surface,steps;
 float t;float2 uv;uint alphaChecks;
 uint rejected,lastSurface,lastAlpha,reserved;
 float2 lastUV;uint instance,geometry;
};
RtTraceResult TraceAccepted(RayDesc ray,RtTraceBounds bounds,bool shadow,bool opaqueDiagnostic) {
 RtTraceResult r=(RtTraceResult)0;r.triangle=r.surface=r.lastSurface=0xffffffff;
 if(!bounds.rangeCount||bounds.rangeCount>4096||bounds.descriptorCount>4096||bounds.pixelCount>16777216||
    !all(isfinite(ray.Origin))||!all(isfinite(ray.Direction))||!isfinite(ray.TMin)||!isfinite(ray.TMax)||ray.TMax<ray.TMin) {
  r.status=RT_TRACE_ERROR;return r;
 }
 RayQuery<RAY_FLAG_NONE> query;
 query.TraceRayInline(scene,shadow?RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH:RAY_FLAG_NONE,0xff,ray);
 bool complete=false,invalid=false;uint budget=clamp(bounds.budget,1,64);
 [loop]for(uint i=0;i<64;i++) {
  if(i>=budget)break;r.steps=i+1;
  if(!query.Proceed()){complete=true;break;}
  if(query.CandidateType()!=CANDIDATE_NON_OPAQUE_TRIANGLE){invalid=true;break;}
  uint tri=sceneTriangleIndex(query.CandidateInstanceID(),query.CandidateGeometryIndex(),query.CandidatePrimitiveIndex(),geometryRanges,bounds.rangeCount);
  if(tri>=bounds.triangleCount){invalid=true;break;}
  uint surface=primitives[tri].surface;
  if(surface>=bounds.surfaceCount){invalid=true;break;}
  RtMaterialSurface s=materialSurfaces[surface];
  if(!s.masked){invalid=true;break;}
  // Both sidedefs are separate coplanar triangles. Legacy winding points away
  // from the owning front sector, so back-facing candidates select that side.
  if(query.CandidateTriangleFrontFace())continue;
  float2 uv;uint packed,descriptor;
  if(!TriangleUV(tri,query.CandidateTriangleBarycentrics(),bounds,uv)||!MaterialSample(surface,uv,bounds,packed,descriptor)){invalid=true;break;}
  r.alphaChecks++;r.lastSurface=surface;r.lastUV=uv;r.lastAlpha=(packed>>8)&255;
  if(opaqueDiagnostic||r.lastAlpha) {
   query.CommitNonOpaqueTriangleHit();
   if(shadow){query.Abort();complete=true;break;}
  } else r.rejected++;
 }
 if(invalid){query.Abort();r.status=RT_TRACE_ERROR;return r;}
 if(!complete){query.Abort();r.status=RT_TRACE_EXHAUSTED;return r;}
 if(query.CommittedStatus()==COMMITTED_TRIANGLE_HIT) {
  r.instance=query.CommittedInstanceID();r.geometry=query.CommittedGeometryIndex();
  r.triangle=sceneTriangleIndex(r.instance,r.geometry,query.CommittedPrimitiveIndex(),geometryRanges,bounds.rangeCount);
  if(r.triangle>=bounds.triangleCount||!TriangleUV(r.triangle,query.CommittedTriangleBarycentrics(),bounds,r.uv)){r.status=RT_TRACE_ERROR;return r;}
  r.surface=primitives[r.triangle].surface;r.t=query.CommittedRayT();r.status=RT_TRACE_HIT;
 } else r.status=RT_TRACE_MISS;
 return r;
}
#endif
