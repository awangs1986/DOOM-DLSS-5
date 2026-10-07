#!/usr/bin/env python3
"""Real catalog public API; fake only D3D12 allocation/map and copied WAD input."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
windows=r'''
#pragma once
#include <cstddef>
using HRESULT=int;
#define FAILED(x) ((x)<0)
#define IID_PPV_ARGS(p) (p)
'''
d3d=r'''
#pragma once
#include "windows.h"
#include <vector>
inline int allocations=0,live_resources=0,fail_create=0,fail_map=0;
struct D3D12_HEAP_PROPERTIES{int Type;};
struct D3D12_RESOURCE_DESC{int Dimension;size_t Width;int Height,DepthOrArraySize,MipLevels;struct{int Count;}SampleDesc;int Layout;};
struct D3D12_RANGE{size_t Begin,End;};
enum{D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_DIMENSION_BUFFER,D3D12_TEXTURE_LAYOUT_ROW_MAJOR,D3D12_HEAP_FLAG_NONE,D3D12_RESOURCE_STATE_GENERIC_READ};
struct ID3D12Resource{
 unsigned refs=1;std::vector<unsigned char>bytes;
 explicit ID3D12Resource(size_t n):bytes(n){++live_resources;}
 ~ID3D12Resource(){--live_resources;}
 void AddRef(){++refs;}void Release(){if(!--refs)delete this;}
 HRESULT Map(unsigned,const D3D12_RANGE*,void**p){if(fail_map){--fail_map;return -1;}*p=bytes.data();return 0;}
 void Unmap(unsigned,const D3D12_RANGE*){}
};
struct ID3D12Device{
 void AddRef(){}void Release(){}
 HRESULT CreateCommittedResource(const D3D12_HEAP_PROPERTIES*,int,const D3D12_RESOURCE_DESC*d,int,void*,ID3D12Resource**out){
  ++allocations;if(fail_create){--fail_create;return -1;}*out=new ID3D12Resource(d->Width);return 0;
 }
};
'''
wrl=r'''
#pragma once
namespace Microsoft{namespace WRL{
template<class T>class ComPtr{
 T*p=nullptr;
 public:ComPtr()=default;ComPtr(const ComPtr&o):p(o.p){if(p)p->AddRef();}~ComPtr(){if(p)p->Release();}
 ComPtr&operator=(const ComPtr&o){if(this!=__builtin_addressof(o)){T*n=o.p;if(n)n->AddRef();if(p)p->Release();p=n;}return *this;}
 ComPtr&operator=(T*n){if(n)n->AddRef();if(p)p->Release();p=n;return *this;}
 T*Get()const{return p;}T*operator->()const{return p;}explicit operator bool()const{return p!=nullptr;}
 T**operator&(){if(p){p->Release();p=nullptr;}return &p;}
};}}
'''
driver=r'''
#include "rt_materials.h"
#include "r_material.h"
#include <d3d12.h>
#include <cassert>
#include <cstdio>
extern "C" int R_ResolveTexture(unsigned id,unsigned*out){*out=id;return 1;}
extern "C" int R_ResolveFlat(unsigned id,unsigned*out){*out=id;return 1;}
extern "C" int R_DescribeTexture(unsigned id,R_MaterialDescription*out){*out={id,1,1,0,{0}};return 1;}
extern "C" int R_DescribeFlat(unsigned id,R_MaterialDescription*out){return R_DescribeTexture(id,out);}
extern "C" int R_CopyTextureIndexedAlpha(unsigned,unsigned char*i,unsigned char*a,size_t n){assert(n==1);*i=23;*a=255;return 1;}
extern "C" int R_CopyFlatIndexed(unsigned,unsigned char*i,size_t n){assert(n==1);*i=23;return 1;}
int main(){
 ID3D12Device device;MapSurface surface={};surface.active=1;surface.kind=MAP_WALL_MID;surface.base_material=1;surface.lightlevel=160;
 MapMesh mesh={};mesh.surfaces=&surface;mesh.surface_count=1;DxrMapSceneView scene={};scene.mesh=&mesh;scene.generation=1;
 for(int failure=0;failure<2;++failure){
  RtMaterials_Init(&device);if(failure==0)fail_create=1;else fail_map=1;
  RtMaterials_Prepare(&scene);assert(!RtMaterials_GetView());assert(live_resources==0);
  int attempted=allocations;RtMaterials_Prepare(&scene);RtMaterials_Prepare(&scene);assert(allocations==attempted);
  RtMaterials_RetryUnavailable();RtMaterials_Prepare(&scene);
  assert(RtMaterials_GetView());assert(allocations>attempted);assert(live_resources==3);
  const RtMaterialView*view=RtMaterials_GetView();void*descriptor=view->descriptor_resource;void*pixels=view->pixel_resource;
  attempted=allocations;RtMaterials_RetryUnavailable();
  assert(RtMaterials_GetView()==view&&view->descriptor_resource==descriptor&&view->pixel_resource==pixels);assert(live_resources==3);
  RtMaterials_Prepare(&scene);assert(allocations==attempted&&RtMaterials_GetView()==view);
  assert(view->descriptor_resource==descriptor&&view->pixel_resource==pixels&&live_resources==3);
  // A failed metadata Map also recovers on the identical new scene signature.
  surface.lightlevel=161;scene.shading_revision=1;fail_map=1;
  RtMaterials_Prepare(&scene);assert(!RtMaterials_GetView());attempted=allocations;
  RtMaterials_Prepare(&scene);assert(!RtMaterials_GetView()&&allocations==attempted);
  RtMaterials_RetryUnavailable();RtMaterials_Prepare(&scene);assert(RtMaterials_GetView());assert(live_resources==3);
  RtMaterials_Shutdown();assert(live_resources==0);surface.lightlevel=160;scene.shading_revision=0;
 }
 std::puts("PASS: create/map failure suppressed until explicit retry; healthy views retained; resources retired safely");
}
'''
with tempfile.TemporaryDirectory(prefix='rt-materials-retry-') as temp:
 p=Path(temp);(p/'wrl').mkdir();(p/'windows.h').write_text(windows);(p/'d3d12.h').write_text(d3d);(p/'wrl/client.h').write_text(wrl);(p/'driver.cpp').write_text(driver)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(p),'-I'+str(root/'win32'),'-I'+str(root/'linuxdoom-1.10'),str(root/'win32/rt_materials.cpp'),str(p/'driver.cpp'),'-o',str(p/'retry')],check=True)
 subprocess.run([str(p/'retry')],check=True)
