// One deterministic point light, at most one opaque visibility ray per receiver.
#include "scene-hit.hlsli"
RaytracingAccelerationStructure scene : register(t0);
struct Sample {
 float3 position; uint flags;
 float3 normal; uint sourceAmbient;
 uint original; uint material; uint kind; uint reserved;
};
StructuredBuffer<Sample> samples : register(t1);
// Active raw PLAYPAL RGB in low bytes, classic gamma channel LUT in high byte.
StructuredBuffer<uint> palette : register(t2);
RWStructuredBuffer<uint> pixels : register(u0);
struct Result {
 float3 position; uint flags;
 float distance; float cosine; uint visible; uint original;
 uint finalColor; uint sourceAmbient; uint material; uint kind;
};
RWStructuredBuffer<Result> results : register(u1);
cbuffer Light : register(b0) {
 float3 lightPosition; uint shadowEnabled;
 float3 lightColor; float intensity;
 float radius; uint width; uint height; uint rowWords;
};
float3 decode(float3 v) {
 return float3(v.x<=0.04045?v.x/12.92:pow((v.x+0.055)/1.055,2.4),
               v.y<=0.04045?v.y/12.92:pow((v.y+0.055)/1.055,2.4),
               v.z<=0.04045?v.z/12.92:pow((v.z+0.055)/1.055,2.4));
}
float3 rawRGB(uint p) { return float3(p&255,(p>>8)&255,(p>>16)&255)/255; }
uint encodeChannel(float v) {
 v=saturate(v); float s=v<=0.0031308?12.92*v:1.055*pow(v,1.0/2.4)-0.055;
 uint index=min(255,uint(s*255+0.5)); return palette[index]>>24;
}
uint encode(float3 v) {
 return encodeChannel(v.r)|(encodeChannel(v.g)<<8)|(encodeChannel(v.b)<<16)|0xff000000;
}
bool visible(float3 p,float3 n,float3 direction,float distance) {
 RayDesc ray;ray.Origin=p+n*0.03125;ray.Direction=direction;
 ray.TMin=0.03125;ray.TMax=max(0.03125,distance-0.0625);
 RayQuery<RAY_FLAG_FORCE_OPAQUE|RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
 query.TraceRayInline(scene,RAY_FLAG_NONE,0xff,ray);
 // Force-opaque static triangles produce no material candidates. Bound future
 // candidate traversal too; exhaustion is conservative blockage, never leakage.
 bool exhausted=true;
 for(uint i=0;i<64;i++) if(!query.Proceed()) {exhausted=false;break;}
 return !exhausted && query.CommittedStatus()==COMMITTED_NOTHING;
}
[numthreads(8,8,1)]
void main(uint3 thread:SV_DispatchThreadID) {
 if(thread.x>=320||thread.y>=200)return;
 uint i=thread.y*320+thread.x;Sample s=samples[i];
 Result result=(Result)0;result.position=s.position;result.flags=s.flags;
 result.visible=1;result.original=s.original;result.sourceAmbient=s.sourceAmbient;
 result.material=s.material;result.kind=s.kind;
 uint color=s.original;
 if(s.flags&1) {
  float3 delta=lightPosition-s.position;float distance=length(delta);result.distance=distance;
  float3 normal=normalize(s.normal);float cosine=distance>0?max(0,dot(normal,delta/distance)):0;
  result.cosine=cosine;
  if(distance<radius && distance>0.0625 && cosine>0 && intensity>0) {
   if(shadowEnabled) {result.flags|=2;result.visible=visible(s.position,normal,delta/distance,distance);}
   if(!result.visible)result.flags|=4;
   float falloff=pow(saturate(1-distance/radius),2)/max(distance*distance,256);
   float3 direct=decode(rawRGB(palette[s.sourceAmbient&255]))*lightColor*intensity*falloff*cosine;
   if(result.visible && any(direct>0)) {
    // Ambient already includes exact original COLORMAP once. Only the new
    // direct term is shadowed; selected palette effects tint both contributions.
    float3 ambient=decode(rawRGB(palette[(s.sourceAmbient>>8)&255]));
    color=encode(ambient+direct);result.flags|=8;
   }
  }
 }
 result.finalColor=color;results[i]=result;pixels[i]=color;
 uint bgra=(color&0xff00ff00)|((color&255)<<16)|((color>>16)&255);
 uint sx=width/320,sy=height/200;
 for(uint y=0;y<sy;y++)for(uint x=0;x<sx;x++)
  pixels[64000+(thread.y*sy+y)*rowWords+thread.x*sx+x]=bgra;
}
