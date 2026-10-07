// Deterministic single map bounce. Shared traversal supplies alpha and identity.
#include "scene-hit.hlsli"
#include "rt-material-layout.hlsli"
RaytracingAccelerationStructure scene : register(t0);
struct Sample { float3 position;uint flags;float3 normal;uint sourceAmbient;uint original,material,kind,descriptor; };
StructuredBuffer<Sample> samples : register(t1);
StructuredBuffer<uint> palette : register(t2);
StructuredBuffer<ScenePrimitive> primitives : register(t3);
StructuredBuffer<SceneGeometryRange> geometryRanges : register(t4);
StructuredBuffer<SceneVertex> vertices : register(t5);
StructuredBuffer<uint> indices : register(t6);
StructuredBuffer<RtMaterialSurface> materialSurfaces : register(t7);
StructuredBuffer<RtMaterialDescriptor> materialDescriptors : register(t8);
StructuredBuffer<uint> materialPixels : register(t9);
struct Setting {float roughness,specular;uint reflect,reserved;float3 emissive;uint padding;};
StructuredBuffer<Setting> settings : register(t10);
StructuredBuffer<uint> colorMaps : register(t11);
StructuredBuffer<float4> baseLinear : register(t12);
StructuredBuffer<uint> baseRGBA : register(t13);
#include "alpha-trace.hlsli"
RWStructuredBuffer<uint> pixels : register(u0);
struct Result {
 float3 position;uint flags;
 float3 hitPosition;uint status;
 float2 uv;uint surface,triangleIndex;
 float3 reflection;uint descriptor;
 float hitT;uint steps,shadowSteps,shadowStatus;
 uint sourceIndex,ambientIndex,original,finalColor;
 uint alphaChecks,rejected,instance,geometry;
 float3 hitNormal;uint reserved;
};
RWStructuredBuffer<Result> results : register(u1);
cbuffer Frame : register(b0) {
 float3 camera;uint useLighting;
 float3 lightPosition;uint shadows;
 float3 lightColor;float intensity;
 float radius;uint width,height,rowWords;
 float3 environment;uint lightEnabled;
 RtTraceBounds bounds;
 uint colorMapCount,padding0,padding1,padding2;
};
float3 rawRGB(uint p) {return float3(p&255,(p>>8)&255,(p>>16)&255)/255;}
float3 decode(float3 v) {
 return float3(v.x<=0.04045?v.x/12.92:pow((v.x+0.055)/1.055,2.4),
 v.y<=0.04045?v.y/12.92:pow((v.y+0.055)/1.055,2.4),
 v.z<=0.04045?v.z/12.92:pow((v.z+0.055)/1.055,2.4));
}
uint encodeChannel(float v) {v=saturate(v);float s=v<=0.0031308?12.92*v:1.055*pow(v,1.0/2.4)-0.055;return palette[min(255,uint(s*255+0.5))]>>24;}
uint encode(float3 v) {return encodeChannel(v.r)|(encodeChannel(v.g)<<8)|(encodeChannel(v.b)<<16)|0xff000000;}
[numthreads(8,8,1)]
void trace(uint3 thread:SV_DispatchThreadID) {
 if(thread.x>=320||thread.y>=200)return;
 uint i=thread.y*320+thread.x;Sample s=samples[i];
 Result r=(Result)0;r.position=s.position;r.flags=s.flags;r.original=useLighting?baseRGBA[i]:s.original;
 r.reserved=s.sourceAmbient|((s.kind&255)<<16);
 r.finalColor=r.original;r.descriptor=s.descriptor;r.surface=r.triangleIndex=0xffffffff;
 if((s.flags&1)&&s.descriptor<bounds.descriptorCount&&settings[s.descriptor].reflect&&s.kind<=1) {
  float3 n=normalize(s.normal),incident=normalize(s.position-camera);
  RayDesc ray;ray.Origin=s.position+n*0.03125;ray.Direction=normalize(reflect(incident,n));ray.TMin=0.03125;ray.TMax=8192;
  RtTraceResult hit=TraceAccepted(ray,bounds,false,false);
  r.flags|=2;r.status=hit.status;r.steps=hit.steps;r.alphaChecks=hit.alphaChecks;r.rejected=hit.rejected;
  r.hitT=hit.t;r.surface=hit.surface;r.triangleIndex=hit.triangleIndex;r.uv=hit.uv;r.instance=hit.instance;r.geometry=hit.geometry;
  float3 radiance=0;
  if(hit.status==RT_TRACE_MISS) {radiance=environment;r.flags|=8;}
  else if(hit.status==RT_TRACE_HIT) {
   uint packed,descriptor;
   if(!MaterialSample(hit.surface,hit.uv,bounds,packed,descriptor)||hit.surface>=bounds.surfaceCount||hit.triangleIndex>=bounds.triangleCount||!colorMapCount) {r.status=RT_TRACE_ERROR;r.flags|=16;}
   else {
    r.flags|=4;r.hitPosition=ray.Origin+ray.Direction*hit.t;r.hitNormal=normalize(primitives[hit.triangleIndex].inwardNormal);
    r.sourceIndex=packed&255;
    uint row=min(colorMapCount-1,(255-min(255,materialSurfaces[hit.surface].lightlevel))/8);
    r.ambientIndex=colorMaps[row*256+r.sourceIndex];
    radiance=decode(rawRGB(palette[r.ambientIndex]))+settings[descriptor].emissive;
    if(lightEnabled) {
     float3 delta=lightPosition-r.hitPosition;float distance=length(delta);
     float cosine=distance>0.0625?max(0,dot(r.hitNormal,delta/distance)):0;
     if(distance<radius&&distance>0.0625&&cosine>0&&intensity>0) {
      bool clear=true;
      if(shadows) {
       RayDesc shadow;shadow.Origin=r.hitPosition+r.hitNormal*0.03125;shadow.Direction=delta/distance;shadow.TMin=0.03125;shadow.TMax=max(0.03125,distance-0.0625);
       RtTraceResult visibility=TraceAccepted(shadow,bounds,true,false);
       r.flags|=64;r.shadowStatus=visibility.status;r.shadowSteps=visibility.steps;r.alphaChecks+=visibility.alphaChecks;r.rejected+=visibility.rejected;
       clear=visibility.status==RT_TRACE_MISS;if(!clear)r.flags|=128;
      }
      if(clear)radiance+=decode(rawRGB(palette[r.sourceIndex]))*lightColor*intensity*pow(saturate(1-distance/radius),2)/max(distance*distance,256)*cosine;
     }
    }
   }
  } else r.flags|=hit.status==RT_TRACE_EXHAUSTED?32:16;
  if(r.status==RT_TRACE_HIT||r.status==RT_TRACE_MISS) {
   float f0=settings[s.descriptor].specular;
   float fresnel=f0+(1-f0)*pow(1-saturate(dot(-incident,n)),5);
   r.reflection=radiance*fresnel;
  }
 }
 results[i]=r;
}
[numthreads(8,8,1)]
void compose(uint3 thread:SV_DispatchThreadID) {
 if(thread.x>=320||thread.y>=200)return;
 uint i=thread.y*320+thread.x;Sample s=samples[i];Result r=results[i];uint color=r.original;
 if((s.flags&1)&&s.descriptor<bounds.descriptorCount) {
  float3 reflected=r.reflection;
  float rough=settings[s.descriptor].roughness;
  if(rough>0&&(r.status==RT_TRACE_HIT||r.status==RT_TRACE_MISS)&&(r.flags&2)) {
   float3 sum=0;float weight=0;
   [unroll]for(int y=-1;y<=1;y++)[unroll]for(int x=-1;x<=1;x++) {
    int2 p=int2(thread.xy)+int2(x,y);if(any(p<0)||p.x>=320||p.y>=200)continue;
    uint j=p.y*320+p.x;Sample neighbor=samples[j];Result other=results[j];
    if(!(neighbor.flags&1)||neighbor.descriptor!=s.descriptor||neighbor.kind!=s.kind||
       other.status!=r.status||other.surface!=r.surface||dot(normalize(s.normal),normalize(neighbor.normal))<0.98||
       length(neighbor.position-s.position)>max(8,length(s.position-camera)*0.05)||abs(other.hitT-r.hitT)>max(8,r.hitT*0.1))continue;
    float w=(x==0&&y==0)?4:1;sum+=other.reflection*w;weight+=w;
   }
   if(weight>0)reflected=lerp(reflected,sum/weight,saturate(rough/0.25));
  }
  float3 added=reflected+settings[s.descriptor].emissive;
  if(any(added>0)) {color=encode(baseLinear[i].rgb+added);r.flags|=256;}
 }
 pixels[i]=color;
 uint bgra=(color&0xff00ff00)|((color&255)<<16)|((color>>16)&255);
 uint sx=width/320,sy=height/200;
 for(uint y=0;y<sy;y++)for(uint x=0;x<sx;x++)pixels[64000+(thread.y*sy+y)*rowWords+thread.x*sx+x]=bgra;
}
