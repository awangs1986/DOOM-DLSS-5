#ifndef WINDOOM_MATERIAL_SAMPLE_HLSLI
#define WINDOOM_MATERIAL_SAMPLE_HLSLI
#include "rt-material-layout.hlsli"
bool MaterialSample(uint surface,float2 uv,RtTraceBounds bounds,out uint packed,out uint descriptorIndex) {
 packed=0;descriptorIndex=0xffffffff;
 if(surface>=bounds.surfaceCount||!all(isfinite(uv))||any(abs(uv)>1e8))return false;
 RtMaterialSurface s=materialSurfaces[surface];descriptorIndex=s.descriptorIndex;
 if(descriptorIndex>=bounds.descriptorCount)return false;
 RtMaterialDescriptor d=materialDescriptors[descriptorIndex];
 if(!d.width||!d.height||d.width>4096||d.height>4096||d.widthMask>=d.width||
    d.width*d.height!=d.pixelCount||d.pixelOffset>bounds.pixelCount||d.pixelCount>bounds.pixelCount-d.pixelOffset)return false;
 int iu=int(floor(uv.x)),iv=int(floor(uv.y));
 uint x=uint(iu)&d.widthMask;
 // Masked posts are finite vertically. Transparent extent is not a data error.
 if(s.masked&&(iv<0||iv>=int(d.height)))return true;
 uint y=uint((iv%int(d.height)+int(d.height))%int(d.height));
 packed=materialPixels[d.pixelOffset+y*d.width+x];return true;
}
bool TriangleUV(uint triangleIndex,float2 bary,RtTraceBounds bounds,out float2 uv) {
 uv=0;
 if(triangleIndex>=bounds.triangleCount||triangleIndex>0xffffffff/3||triangleIndex*3>bounds.indexCount||3>bounds.indexCount-triangleIndex*3)return false;
 uint a=indices[triangleIndex*3],b=indices[triangleIndex*3+1],c=indices[triangleIndex*3+2];
 if(a>=bounds.vertexCount||b>=bounds.vertexCount||c>=bounds.vertexCount||!all(isfinite(bary)))return false;
 uv=vertices[a].uv*(1-bary.x-bary.y)+vertices[b].uv*bary.x+vertices[c].uv*bary.y;
 return all(isfinite(uv));
}
#endif
