/* Owned real-map DXR scene + diagnostics. GPLv2; see LICENSE.TXT. */
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <chrono>
#include "dxr_map.h"
#include "map_source.h"
#include "gbuffer.h"
extern "C" {
#include "m_argv.h"
}
using Microsoft::WRL::ComPtr;
namespace {
struct Primitive {float normal[3];uint32_t surface;float inward[3];uint32_t reserved;};
struct Hit {float depth,normal[3];uint32_t surface,primitive,geometry,instance,valid,reserved;};
struct FrameConstants {uint32_t width,height,row_words,mode;float origin[3];uint32_t range_count;float reverse[3],unused2;};
static_assert(sizeof(Hit)==40 && sizeof(Primitive)==32 && sizeof(FrameConstants)==48,"shader layout");
struct Group {
 ComPtr<ID3D12Resource> vertices,blas;
 std::vector<MapMeshVertex> positions;
 std::vector<uint32_t> surfaces;
 unsigned opaque_triangles=0;
};
struct State {
 ComPtr<ID3D12Device5> device;ComPtr<ID3D12CommandQueue> queue;
 ComPtr<ID3D12Resource> vertices,indices,instances,blas,tlas,scratch,metadata,geometry_ranges;
 ComPtr<ID3D12Resource> rays,flags,original,output,hits,readback;
 ComPtr<ID3D12RootSignature> root;ComPtr<ID3D12PipelineState> pipeline;
 D3D12_RESOURCE_STATES output_state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS,hit_state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
 MapMesh mesh={};DxrMapSceneView view={};
 std::vector<Group> groups;std::vector<MapGeometryRange> ranges;MapWorldRevision world={};
 uint64_t geometry_revision=0,material_revision=0,shading_revision=0;
 bool capture_pending=true,lighting_pending=false,mapping_pending=false;
 unsigned refits=0,rebuilds=0,watch_sector=UINT32_MAX;double capture_ms=0,blas_ms=0,tlas_ms=0;UINT64 resident_bytes=0;
 uint64_t generation=0;char map_name[9]={};unsigned consumers=0;bool loaded=false,supported=false,enabled=false,disabled=false,ready=false,failed_generation=false,pending=false;
 int mode=0,hit_tic=-1,last_hit_tic=-1,frame_tic=0;unsigned width=0,height=0,row_pitch=0,frame_id=0;
 FILE *stats=nullptr,*details=nullptr,*updates=nullptr;
 std::vector<float> expected;std::vector<unsigned char> reference_normal;std::vector<uint32_t> frame_flags;
 GB_FrameInputs frame={};
} state;
bool failed(HRESULT hr,const char* operation) {
 if(SUCCEEDED(hr))return false;std::fprintf(stderr,"DXR map: %s failed (0x%08lx); map feature disabled, normal present retained\n",operation,static_cast<unsigned long>(hr));return true;
}
ComPtr<ID3D12Resource> buffer(UINT64 size, D3D12_HEAP_TYPE heap_type,
            D3D12_RESOURCE_STATES initial, D3D12_RESOURCE_FLAGS flags)
{
    ComPtr<ID3D12Resource> resource;
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = heap_type;
    D3D12_RESOURCE_DESC description = {};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width = size;
    description.Height = description.DepthOrArraySize = description.MipLevels = 1;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    description.Flags = flags;
    if (failed(state.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
             &description, initial, nullptr, IID_PPV_ARGS(&resource)), "buffer allocation"))
        return {};
    return resource;
}
void uav_barrier(ID3D12GraphicsCommandList4 *commands, ID3D12Resource *resource)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = resource;
    commands->ResourceBarrier(1, &barrier);
}
bool upload(ID3D12Resource *resource, const void *data, size_t size)
{
    void *mapped;
    D3D12_RANGE no_read = { 0, 0 };
    if (failed(resource->Map(0, &no_read, &mapped), "geometry upload map")) return false;
    std::memcpy(mapped, data, size);
    D3D12_RANGE written = { 0, size };
    resource->Unmap(0, &written);
    return true;
}
bool wait_queue() {
 if(!state.queue||!state.device)return true;
 ComPtr<ID3D12Fence> fence; if(failed(state.device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"scene fence"))return false;
 HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event)return false;
 HRESULT hr=state.queue->Signal(fence.Get(),1);
 if(SUCCEEDED(hr)&&fence->GetCompletedValue()<1) {hr=fence->SetEventOnCompletion(1,event);if(SUCCEEDED(hr)&&WaitForSingleObject(event,INFINITE)!=WAIT_OBJECT_0)hr=E_FAIL;}
 CloseHandle(event);return !failed(hr,"scene queue completion")&&!failed(state.device->GetDeviceRemovedReason(),"scene device status");
}
bool load_pipeline() {
 wchar_t path[MAX_PATH];DWORD length=GetModuleFileNameW(nullptr,path,MAX_PATH);if(!length||length>=MAX_PATH)return false;
 auto slash=wcsrchr(path,L'\\');if(!slash)return false;*(slash+1)=0;
 if(wcslen(path)+wcslen(L"shaders\\dxr_map.cso")>=MAX_PATH)return false;wcscat_s(path,L"shaders\\dxr_map.cso");
 FILE* file=nullptr;_wfopen_s(&file,path,L"rb");if(!file){std::fprintf(stderr,"DXR map: shader missing beside executable\n");return false;}
 std::vector<unsigned char> bytes;
 try {if(std::fseek(file,0,SEEK_END)||std::ftell(file)<=0||std::ftell(file)>16*1024*1024){std::fclose(file);return false;}bytes.resize(static_cast<size_t>(std::ftell(file)));std::rewind(file);bool ok=std::fread(bytes.data(),1,bytes.size(),file)==bytes.size();std::fclose(file);file=nullptr;if(!ok)return false;}
 catch(...){if(file)std::fclose(file);throw;}
 D3D12_ROOT_PARAMETER params[9]={};
 for(unsigned i=0;i<6;i++){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;params[i].Descriptor.ShaderRegister=i;}
 for(unsigned i=0;i<2;i++){params[i+6].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;params[i+6].Descriptor.ShaderRegister=i;}
 params[8].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[8].Constants.ShaderRegister=0;params[8].Constants.Num32BitValues=12;
 D3D12_ROOT_SIGNATURE_DESC desc={};desc.NumParameters=9;desc.pParameters=params;
 ComPtr<ID3DBlob> blob,errors;
 if(failed(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors),"map root serialization")||
    failed(state.device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&state.root)),"map root"))return false;
 D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline={};pipeline.pRootSignature=state.root.Get();pipeline.CS={bytes.data(),bytes.size()};
 return !failed(state.device->CreateComputePipelineState(&pipeline,IID_PPV_ARGS(&state.pipeline)),"map inline pipeline");
}
bool build_scene() {
 const auto begin=std::chrono::steady_clock::now();
 MapMesh next={};char diagnostic[256];
 if(!MapSource_Capture(state.generation,&next,diagnostic,sizeof(diagnostic))){std::fprintf(stderr,"DXR map: extraction rejected: %s\n",diagnostic);return false;}
 struct MeshGuard {MapMesh* mesh;~MeshGuard(){MapMesh_Free(mesh);}} guard{&next};
 struct Planned {std::vector<MapMeshVertex> vertices[2];std::vector<uint32_t> surfaces[2];};
 std::vector<Planned> plans(state.world.sectors);
 for(size_t t=0;t<next.triangle_count;t++) {
  auto id=next.triangle_surfaces[t];const auto& surface=next.surfaces[id];
  unsigned owner=surface.sector;
  if(surface.back_sector!=MAP_SIDE_NONE)owner=std::min(owner,surface.back_sector);
  if(owner>=plans.size())throw std::runtime_error("invalid AS group owner");
  unsigned masked=surface.masked?1:0;auto& plan=plans[owner];plan.surfaces[masked].push_back(id);
  for(unsigned v=0;v<3;v++)plan.vertices[masked].push_back(next.vertices[next.indices[t*3+v]]);
 }
 state.groups.resize(plans.size());state.ranges.clear();
 std::vector<MapMeshVertex> vertices;std::vector<uint32_t> indices,surfaces;
 std::vector<Primitive> primitives;std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instances;
 struct Build {unsigned group;bool update;std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometry;D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;};
 std::vector<Build> builds;UINT64 scratch_bytes=0;state.refits=state.rebuilds=0;state.resident_bytes=0;
 for(unsigned owner=0;owner<plans.size();owner++) {
  auto& group=state.groups[owner];auto& plan=plans[owner];
  std::vector<MapMeshVertex> local=plan.vertices[0];local.insert(local.end(),plan.vertices[1].begin(),plan.vertices[1].end());
  std::vector<uint32_t> ids=plan.surfaces[0];ids.insert(ids.end(),plan.surfaces[1].begin(),plan.surfaces[1].end());
  unsigned opaque=static_cast<unsigned>(plan.surfaces[0].size());
  if(ids.empty()){if(group.blas)state.rebuilds++;group=Group();continue;}
  bool same=group.blas&&ids==group.surfaces&&opaque==group.opaque_triangles;
  bool moved=!same||local.size()!=group.positions.size();
  if(!moved)for(size_t i=0;i<local.size();i++)if(std::memcmp(local[i].position,group.positions[i].position,12)){moved=true;break;}
  unsigned geometry_index=0;
  for(unsigned kind=0;kind<2;kind++)if(!plan.surfaces[kind].empty()){
   state.ranges.push_back({owner,geometry_index++,static_cast<uint32_t>(surfaces.size()),static_cast<uint32_t>(plan.surfaces[kind].size())});
   for(size_t i=0;i<plan.surfaces[kind].size();i++){
    auto id=plan.surfaces[kind][i];surfaces.push_back(id);
    Primitive primitive={};std::memcpy(primitive.normal,next.surfaces[id].legacy_normal,12);primitive.surface=id;std::memcpy(primitive.inward,next.surfaces[id].inward_normal,12);primitives.push_back(primitive);
    for(unsigned v=0;v<3;v++){indices.push_back(static_cast<uint32_t>(vertices.size()));vertices.push_back(plan.vertices[kind][i*3+v]);}
   }
  }
  if(moved){
   if(!same)group.vertices=buffer(local.size()*sizeof(MapMeshVertex),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
   if(!group.vertices||!upload(group.vertices.Get(),local.data(),local.size()*sizeof(MapMeshVertex)))return false;
   Build build={owner,same,{}, {}};unsigned vertex_start=0;
   for(unsigned kind=0;kind<2;kind++)if(!plan.surfaces[kind].empty()){
    D3D12_RAYTRACING_GEOMETRY_DESC geometry={};geometry.Type=D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometry.Flags=kind?D3D12_RAYTRACING_GEOMETRY_FLAG_NONE:D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    geometry.Triangles.VertexFormat=DXGI_FORMAT_R32G32B32_FLOAT;geometry.Triangles.VertexCount=static_cast<UINT>(plan.vertices[kind].size());
    geometry.Triangles.VertexBuffer={group.vertices->GetGPUVirtualAddress()+vertex_start*sizeof(MapMeshVertex),sizeof(MapMeshVertex)};
    geometry.Triangles.IndexFormat=DXGI_FORMAT_UNKNOWN;build.geometry.push_back(geometry);vertex_start+=geometry.Triangles.VertexCount;
   }
   D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottom={};bottom.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
   bottom.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE|D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
   bottom.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY;bottom.NumDescs=static_cast<UINT>(build.geometry.size());bottom.pGeometryDescs=build.geometry.data();
   state.device->GetRaytracingAccelerationStructurePrebuildInfo(&bottom,&build.info);if(!build.info.ResultDataMaxSizeInBytes)return false;
   if(!same)group.blas=buffer(build.info.ResultDataMaxSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   if(!group.blas)return false;
   scratch_bytes=std::max(scratch_bytes,same?build.info.UpdateScratchDataSizeInBytes:build.info.ScratchDataSizeInBytes);
   same?state.refits++:state.rebuilds++;builds.push_back(std::move(build));
  }
  group.positions=std::move(local);group.surfaces=std::move(ids);group.opaque_triangles=opaque;
  state.resident_bytes+=group.vertices->GetDesc().Width+group.blas->GetDesc().Width;
  D3D12_RAYTRACING_INSTANCE_DESC instance={};instance.Transform[0][0]=instance.Transform[1][1]=instance.Transform[2][2]=1;
  instance.InstanceID=owner;instance.InstanceMask=255;instance.AccelerationStructure=group.blas->GetGPUVirtualAddress();instances.push_back(instance);
 }
 if(instances.empty())return false;
 /* Canonical packed shader geometry follows (instance, actual descriptor, primitive).
    BLAS local buffers remain independently stable when another group changes size. */
 auto copied_vertices=static_cast<MapMeshVertex*>(std::malloc(vertices.size()*sizeof(MapMeshVertex)));
 auto copied_indices=static_cast<uint32_t*>(std::malloc(indices.size()*sizeof(uint32_t)));
 auto copied_surfaces=static_cast<uint32_t*>(std::malloc(surfaces.size()*sizeof(uint32_t)));
 if(!copied_vertices||!copied_indices||!copied_surfaces){std::free(copied_vertices);std::free(copied_indices);std::free(copied_surfaces);return false;}
 std::memcpy(copied_vertices,vertices.data(),vertices.size()*sizeof(MapMeshVertex));std::memcpy(copied_indices,indices.data(),indices.size()*sizeof(uint32_t));std::memcpy(copied_surfaces,surfaces.data(),surfaces.size()*sizeof(uint32_t));
 std::free(next.vertices);std::free(next.indices);std::free(next.triangle_surfaces);next.vertices=copied_vertices;next.indices=copied_indices;next.triangle_surfaces=copied_surfaces;next.vertex_count=vertices.size();
 state.vertices=buffer(vertices.size()*sizeof(MapMeshVertex),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
 state.indices=buffer(indices.size()*sizeof(uint32_t),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
 state.metadata=buffer(primitives.size()*sizeof(Primitive),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
 state.geometry_ranges=buffer(state.ranges.size()*sizeof(MapGeometryRange),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
 if(!state.vertices||!state.indices||!state.metadata||!state.geometry_ranges||!upload(state.vertices.Get(),vertices.data(),vertices.size()*sizeof(MapMeshVertex))||!upload(state.indices.Get(),indices.data(),indices.size()*sizeof(uint32_t))||!upload(state.metadata.Get(),primitives.data(),primitives.size()*sizeof(Primitive))||!upload(state.geometry_ranges.Get(),state.ranges.data(),state.ranges.size()*sizeof(MapGeometryRange)))return false;
 state.capture_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();state.blas_ms=state.tlas_ms=0;
 if(!builds.empty()||!state.tlas||state.rebuilds){
  state.instances=buffer(instances.size()*sizeof(D3D12_RAYTRACING_INSTANCE_DESC),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
  if(!state.instances||!upload(state.instances.Get(),instances.data(),instances.size()*sizeof(D3D12_RAYTRACING_INSTANCE_DESC)))return false;
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS top={};top.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;top.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY;top.NumDescs=static_cast<UINT>(instances.size());top.InstanceDescs=state.instances->GetGPUVirtualAddress();
  top.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ts={};state.device->GetRaytracingAccelerationStructurePrebuildInfo(&top,&ts);if(!ts.ResultDataMaxSizeInBytes)return false;
  if(!state.tlas||state.tlas->GetDesc().Width<ts.ResultDataMaxSizeInBytes)state.tlas=buffer(ts.ResultDataMaxSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  scratch_bytes=std::max(scratch_bytes,ts.ScratchDataSizeInBytes);
  if(!state.scratch||state.scratch->GetDesc().Width<scratch_bytes)state.scratch=buffer(scratch_bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  if(!state.tlas||!state.scratch)return false;
  ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList4> list;
  if(failed(state.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"map build allocator")||failed(state.device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"map build list4"))return false;
  ComPtr<ID3D12QueryHeap> timestamps;ComPtr<ID3D12Resource> readback;D3D12_QUERY_HEAP_DESC query={};query.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;query.Count=4;
  bool timed=SUCCEEDED(state.device->CreateQueryHeap(&query,IID_PPV_ARGS(&timestamps)))&&(readback=buffer(32,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_FLAG_NONE))!=nullptr;
  if(timed)list->EndQuery(timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0);
  for(auto& build:builds){
   auto& group=state.groups[build.group];D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc={};desc.Inputs.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
   desc.Inputs.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY;desc.Inputs.NumDescs=static_cast<UINT>(build.geometry.size());desc.Inputs.pGeometryDescs=build.geometry.data();
   desc.Inputs.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE|D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
   desc.DestAccelerationStructureData=group.blas->GetGPUVirtualAddress();desc.ScratchAccelerationStructureData=state.scratch->GetGPUVirtualAddress();
   if(build.update){desc.Inputs.Flags|=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;desc.SourceAccelerationStructureData=desc.DestAccelerationStructureData;}
   list->BuildRaytracingAccelerationStructure(&desc,0,nullptr);uav_barrier(list.Get(),group.blas.Get());uav_barrier(list.Get(),state.scratch.Get());
  }
  if(timed){list->EndQuery(timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,1);list->EndQuery(timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,2);}
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc={};desc.Inputs=top;desc.DestAccelerationStructureData=state.tlas->GetGPUVirtualAddress();desc.ScratchAccelerationStructureData=state.scratch->GetGPUVirtualAddress();
  list->BuildRaytracingAccelerationStructure(&desc,0,nullptr);uav_barrier(list.Get(),state.tlas.Get());
  if(timed){list->EndQuery(timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,3);list->ResolveQueryData(timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0,4,readback.Get(),0);}
  if(failed(list->Close(),"map build close"))return false;ID3D12CommandList* lists[]={list.Get()};state.queue->ExecuteCommandLists(1,lists);if(!wait_queue())return false;
  if(timed){UINT64 frequency=0,*values=nullptr;D3D12_RANGE read={0,32};if(SUCCEEDED(state.queue->GetTimestampFrequency(&frequency))&&frequency&&SUCCEEDED(readback->Map(0,&read,reinterpret_cast<void**>(&values)))){state.blas_ms=1000.0*(values[1]-values[0])/frequency;state.tlas_ms=1000.0*(values[3]-values[2])/frequency;D3D12_RANGE no_write={0,0};readback->Unmap(0,&no_write);}}
 }
 state.resident_bytes+=state.vertices->GetDesc().Width+state.indices->GetDesc().Width+state.metadata->GetDesc().Width+state.geometry_ranges->GetDesc().Width+(state.instances?state.instances->GetDesc().Width:0)+(state.tlas?state.tlas->GetDesc().Width:0)+(state.scratch?state.scratch->GetDesc().Width:0);
 MapMesh_Free(&state.mesh);state.mesh=next;next={};
 state.view={&state.mesh,state.tlas.Get(),state.tlas->GetGPUVirtualAddress(),state.generation,state.ranges.data(),static_cast<unsigned>(state.ranges.size()),state.metadata.Get(),state.geometry_ranges.Get(),state.vertices.Get(),state.indices.Get(),state.geometry_revision,state.material_revision,state.shading_revision};
 std::fprintf(stderr,"DXR map update: generation=%llu geometry=%llu material=%llu triangles=%zu groups=%zu refits=%u rebuilds=%u capture_ms=%.4f BLAS_ms=%.4f TLAS_ms=%.4f bytes=%llu fenced publication\n",static_cast<unsigned long long>(state.generation),static_cast<unsigned long long>(state.geometry_revision),static_cast<unsigned long long>(state.material_revision),state.mesh.triangle_count,instances.size(),state.refits,state.rebuilds,state.capture_ms,state.blas_ms,state.tlas_ms,static_cast<unsigned long long>(state.resident_bytes));
 return true;
}
FILE* observe_file(const char* flag) {
 int p=M_CheckParm(const_cast<char*>(flag));if(!p)return nullptr;if(p+1>=myargc){std::fprintf(stderr,"DXR map: %s needs a new CSV path\n",flag);return nullptr;}
 HANDLE h=CreateFileA(myargv[p+1],GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE){std::fprintf(stderr,"DXR map: %s refuses existing/unavailable file\n",flag);return nullptr;}CloseHandle(h);
 return std::fopen(myargv[p+1],"w");
}
void collect() {
 if(!state.pending)return;state.pending=false;
 void* mapped=nullptr;D3D12_RANGE range={0,64001*sizeof(Hit)};
 if(failed(state.readback->Map(0,&range,&mapped),"map hits readback")){state.enabled=false;return;}
 const Hit* hits=static_cast<const Hit*>(mapped);
 unsigned eligible=0,hit_count=0,matched=0,normal_matched=0;double absolute_sum=0,max_error=0;
 bool details=state.details&&state.hit_tic==state.frame_tic&&state.last_hit_tic!=state.frame_tic;
 for(unsigned i=0;i<64000;i++) if(state.frame_flags[i]&1) {
  eligible++;const auto& h=hits[i];float expected=state.expected[i];double error=h.valid?std::abs(h.depth-expected):expected;
  if(h.valid){hit_count++;absolute_sum+=error;max_error=std::max(max_error,error);if(error<=std::max(0.5,expected*0.01))matched++;
   int channels_ok=0;for(unsigned c=0;c<3;c++) {int encoded=static_cast<int>((h.normal[c]*0.5f+0.5f)*255.f+0.5f);if(std::abs(encoded-static_cast<int>(state.reference_normal[i*4+c]))<=2)channels_ok++;}if(channels_ok==3)normal_matched++;
  }
  if(details)std::fprintf(state.details,"%u,%d,%llu,%u,%u,%u,%.7g,%.7g,%.7g,%.7g,%.7g,%u,%u,%u,%u,%u,%u,%u,%u\n",state.frame_id,state.frame_tic,static_cast<unsigned long long>(state.generation),i%320,i/320,state.frame_flags[i],expected,h.depth,h.normal[0],h.normal[1],h.normal[2],state.reference_normal[i*4],state.reference_normal[i*4+1],state.reference_normal[i*4+2],h.surface,h.primitive,h.geometry,h.instance,h.valid);
 }
 if(details){state.last_hit_tic=state.frame_tic;std::fflush(state.details);}
 auto probe=hits[64000];float point[3];for(unsigned i=0;i<3;i++)point[i]=state.frame.sampled.position[i]-probe.depth*(i==0?state.frame.sampled.forward_cos:(i==2?state.frame.sampled.forward_sin:0));
 if(state.stats) {
  std::fprintf(state.stats,"%u,%d,%llu,%s,%zu,%u,%u,%u,%u,%.7g,%.7g,%u,%u,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g\n",state.frame_id,state.frame_tic,static_cast<unsigned long long>(state.generation),state.map_name,state.mesh.triangle_count,eligible,hit_count,matched,normal_matched,hit_count?absolute_sum/hit_count:0,max_error,probe.valid,probe.surface,probe.depth,point[0],point[1],point[2],state.frame.sampled.position[0],state.frame.sampled.position[2],state.frame.sampled.position[1],state.frame.sampled.forward_cos,state.frame.sampled.forward_sin,-probe.depth*(state.frame.sampled.forward_cos*state.frame.sampled.forward_cos+state.frame.sampled.forward_sin*state.frame.sampled.forward_sin));std::fflush(state.stats);
 }
 if(details||state.frame_id==1)std::fprintf(stderr,"DXR map GPU: frame=%u tic=%d eligible=%u hits=%u depth_within_1pct=%u normal_matches=%u reverse_offscreen_hit=%u surface=%u distance=%.3f point=(%.3f,%.3f,%.3f)\n",state.frame_id,state.frame_tic,eligible,hit_count,matched,normal_matched,probe.valid,probe.surface,probe.depth,point[0],point[1],point[2]);
 D3D12_RANGE no_write={0,0};state.readback->Unmap(0,&no_write);
}
void release_scene() {
 collect();state.view={};state.vertices.Reset();state.indices.Reset();state.instances.Reset();state.blas.Reset();state.tlas.Reset();state.scratch.Reset();state.metadata.Reset();state.geometry_ranges.Reset();MapMesh_Free(&state.mesh);state.groups.clear();state.ranges.clear();state.ready=false;state.capture_pending=true;
}
void transition(ID3D12GraphicsCommandList4* list,ID3D12Resource* resource,D3D12_RESOURCE_STATES& before,D3D12_RESOURCE_STATES after) {
 if(before==after)return;D3D12_RESOURCE_BARRIER b={};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};list->ResourceBarrier(1,&b);before=after;
}
}
extern "C" void DxrMap_Init(void* device,void* queue,unsigned width,unsigned height,int mode,int disabled) {
 state.width=width;state.height=height;state.row_pitch=(width*4+255)&~255u;state.mode=mode;
 state.queue=static_cast<ID3D12CommandQueue*>(queue);auto base=static_cast<ID3D12Device*>(device);
 D3D12_FEATURE_DATA_D3D12_OPTIONS5 options={};D3D12_FEATURE_DATA_SHADER_MODEL shader={D3D_SHADER_MODEL_6_5};
 ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> adapter;DXGI_ADAPTER_DESC1 description={};
 bool hardware=SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->EnumAdapterByLuid(base->GetAdapterLuid(),IID_PPV_ARGS(&adapter)))&&SUCCEEDED(adapter->GetDesc1(&description))&&!(description.Flags&DXGI_ADAPTER_FLAG_SOFTWARE);
 state.supported=hardware&&SUCCEEDED(base->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&options,sizeof(options)))&&options.RaytracingTier>=D3D12_RAYTRACING_TIER_1_1&&SUCCEEDED(base->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&shader,sizeof(shader)))&&shader.HighestShaderModel>=D3D_SHADER_MODEL_6_5&&SUCCEEDED(base->QueryInterface(IID_PPV_ARGS(&state.device)));
 state.disabled=disabled!=0;state.enabled=mode&&state.supported&&!state.disabled;
 if(mode)std::fprintf(stderr,"DXR map: requested mode=%s hardware=%d tier=%u SM=0x%x enabled=%d; independent of NGX\n",mode==2?"normal":"depth",hardware,options.RaytracingTier,shader.HighestShaderModel,state.enabled);
 state.updates=observe_file("-map-updates");
 int watch=M_CheckParm("-map-watch-sector");if(watch&&watch+1<myargc){char* end=nullptr;unsigned long n=std::strtoul(myargv[watch+1],&end,10);if(!*end&&n<1000000)state.watch_sector=static_cast<unsigned>(n);}
 if(state.updates)std::fprintf(state.updates,"frame,game_tic,generation,geometry_revision,material_revision,shading_revision,reset_reasons,history_valid,triangles,refits,rebuilds,capture_ms,blas_ms,tlas_ms,resident_gpu_bytes,watch_sector,floor_height,ceiling_height\n");
 state.stats=observe_file("-map-stats");state.details=observe_file("-map-hits");
 int p=M_CheckParm("-map-hit-tic");if(p&&p+1<myargc){char* end=nullptr;long value=std::strtol(myargv[p+1],&end,10);if(*myargv[p+1]&&!*end&&value>=0&&value<10000000)state.hit_tic=static_cast<int>(value);else std::fprintf(stderr,"DXR map: invalid hit tic; detailed observation disabled\n");}
 if(state.stats)std::fprintf(state.stats,"frame,game_tic,generation,map,triangles,eligible,hits,depth_matches,normal_matches,mean_abs_depth_error,max_abs_depth_error,offscreen_hit,offscreen_surface,offscreen_t,hit_x,hit_height,hit_y,camera_x,camera_y,camera_height,forward_x,forward_y,offscreen_forward_dot\n");
 if(state.details)std::fprintf(state.details,"frame,game_tic,generation,x,y,flags,reference_depth,ray_depth,nx,ny,nz,reference_nr,reference_ng,reference_nb,surface,primitive,geometry,instance,hit\n");
}
extern "C" void DxrMap_LevelLoaded(const char* name) {
 state.generation++;state.world={};state.geometry_revision=state.material_revision=state.shading_revision=0;state.capture_pending=true;state.lighting_pending=false;state.mapping_pending=false;state.loaded=true;state.failed_generation=false;state.last_hit_tic=-1;
 std::snprintf(state.map_name,sizeof(state.map_name),"%.8s",name?name:"unknown");
 if(state.mode)std::fprintf(stderr,"DXR map: new generation=%llu map=%s marked dirty; snapshot deferred until restored-world scene frame\n",static_cast<unsigned long long>(state.generation),state.map_name);
}
extern "C" void DxrMap_DetectChanges(void) {
 if((!state.enabled&&!state.consumers)||!state.supported||state.disabled||!state.loaded||state.failed_generation||!GB_HasScenePixels())return;
 MapWorldRevision current={};if(!MapSource_Survey(&current))return;
 bool initial=!state.world.sectors;
 bool geometry=initial||current.geometry!=state.world.geometry;
 bool material=initial||current.material!=state.world.material;
 bool lighting=initial||current.lighting!=state.world.lighting;
 bool mapping=initial||current.mapping!=state.world.mapping;
 if(geometry){state.geometry_revision++;state.capture_pending=true;if(!initial)GB_RequestResetReason(GB_RESET_GEOMETRY);}
 if(geometry||material){state.material_revision++;state.capture_pending=true;}
 if(mapping)state.mapping_pending=true;
 if(geometry||material||lighting||mapping){state.shading_revision++;state.lighting_pending=true;if(!initial&&!geometry)GB_RequestResetReason(GB_RESET_SHADING);}
 state.world=current;
}
extern "C" void DxrMap_Prepare(void) {
 collect();if((!state.enabled&&!state.consumers)||!state.supported||state.disabled||!state.loaded||state.failed_generation||!GB_GetFrameInputs()->scene_valid)return;
 {
  try {
   if(state.enabled && !state.pipeline && !load_pipeline())throw std::runtime_error("map pipeline unavailable");
   if(state.enabled && (!state.rays||!state.flags||!state.original||!state.output||!state.hits||!state.readback)) {
    state.output_state=state.hit_state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    /* Retry the complete diagnostic allocation after a partial failure. */
    if(!state.pipeline)throw std::runtime_error("map pipeline unavailable");
    state.rays=buffer(64000*4*sizeof(float),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
    state.flags=buffer(64000*sizeof(uint32_t),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
    state.original=buffer(static_cast<UINT64>(state.width)*state.height*4,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
    state.output=buffer(static_cast<UINT64>(state.row_pitch)*state.height,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    state.hits=buffer(64001*sizeof(Hit),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    state.readback=buffer(64001*sizeof(Hit),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_FLAG_NONE);
    if(!state.rays||!state.flags||!state.original||!state.output||!state.hits||!state.readback)throw std::runtime_error("map diagnostic allocation failed");
   }
   state.refits=state.rebuilds=0;state.capture_ms=state.blas_ms=state.tlas_ms=0;
   if(!state.ready||state.capture_pending) { if(!build_scene())throw std::runtime_error("map scene unavailable");state.ready=true;state.capture_pending=false;state.mapping_pending=false; }
   if(state.mapping_pending){if(!MapSource_UpdateMapping(&state.mesh)||!upload(state.vertices.Get(),state.mesh.vertices,state.mesh.vertex_count*sizeof(MapMeshVertex)))throw std::runtime_error("map UV update failed");state.mapping_pending=false;}
   if(state.lighting_pending){MapSource_UpdateLightLevels(&state.mesh);state.view.shading_revision=state.shading_revision;state.lighting_pending=false;}
   if(state.updates){double floor=0,ceiling=0;MapSource_GetSectorHeights(state.watch_sector,&floor,&ceiling);auto frame=GB_GetFrameInputs();std::fprintf(state.updates,"%u,%d,%llu,%llu,%llu,%llu,%u,%u,%zu,%u,%u,%.6f,%.6f,%.6f,%llu,%u,%.3f,%.3f\n",frame->frame_id,frame->game_tic,static_cast<unsigned long long>(state.generation),static_cast<unsigned long long>(state.geometry_revision),static_cast<unsigned long long>(state.material_revision),static_cast<unsigned long long>(state.shading_revision),frame->reset_reasons,frame->history_valid,state.mesh.triangle_count,state.refits,state.rebuilds,state.capture_ms,state.blas_ms,state.tlas_ms,static_cast<unsigned long long>(state.resident_bytes),state.watch_sector,floor,ceiling);std::fflush(state.updates);}
  }catch(const std::exception& error){std::fprintf(stderr,"DXR map: generation=%llu initialization failed: %s; normal present retained\n",static_cast<unsigned long long>(state.generation),error.what());release_scene();state.failed_generation=true;return;}
 }
}
extern "C" int DxrMap_Render(void* commands,void* backbuffer) {
 const auto frame=GB_GetFrameInputs();if(!state.enabled||!state.ready||!state.pipeline||!state.output||!frame->scene_valid)return 0;
 try {
 ComPtr<ID3D12GraphicsCommandList4> list;auto base=static_cast<ID3D12GraphicsCommandList*>(commands);
 if(failed(base->QueryInterface(IID_PPV_ARGS(&list)),"map frame list4")||failed(state.device->GetDeviceRemovedReason(),"map frame device")){state.enabled=false;return 0;}
 std::vector<float> rays(64000*4);state.frame_flags.assign(64000,0);
 auto kind=GB_SurfaceKind();auto overlay=GB_OverlayMask();auto depth=GB_Depth();
 for(unsigned i=0;i<64000;i++) {
  float ray[3];if(kind[i]!=GB_KIND_SPRITE&&GB_SampleRay(i%320,i/320,ray)) {std::memcpy(&rays[i*4],ray,12);rays[i*4+3]=depth[i];state.frame_flags[i]=1;}
  if(overlay[i])state.frame_flags[i]|=2;
 }
 std::vector<unsigned char> original(static_cast<size_t>(state.width)*state.height*4);GB_ComposePresent(original.data(),state.width,state.height);
 if(!upload(state.rays.Get(),rays.data(),rays.size()*sizeof(float))||!upload(state.flags.Get(),state.frame_flags.data(),64000*sizeof(uint32_t))||!upload(state.original.Get(),original.data(),original.size())){state.enabled=false;return 0;}
 state.expected.assign(depth,depth+64000);state.reference_normal.assign(GB_NormalRGBA(),GB_NormalRGBA()+64000*4);
 state.frame=*frame;state.frame_tic=frame->game_tic;state.frame_id=frame->frame_id;
 transition(list.Get(),state.output.Get(),state.output_state,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);transition(list.Get(),state.hits.Get(),state.hit_state,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 list->SetPipelineState(state.pipeline.Get());list->SetComputeRootSignature(state.root.Get());
 ID3D12Resource* resources[]={state.tlas.Get(),state.metadata.Get(),state.rays.Get(),state.flags.Get(),state.original.Get(),state.geometry_ranges.Get()};for(unsigned i=0;i<6;i++)list->SetComputeRootShaderResourceView(i,resources[i]->GetGPUVirtualAddress());
 list->SetComputeRootUnorderedAccessView(6,state.output->GetGPUVirtualAddress());list->SetComputeRootUnorderedAccessView(7,state.hits->GetGPUVirtualAddress());
 FrameConstants constants={state.width,state.height,state.row_pitch/4,static_cast<uint32_t>(state.mode),{frame->sampled.position[0],frame->sampled.position[1],frame->sampled.position[2]},state.view.geometry_count,{-frame->sampled.forward_cos,0,-frame->sampled.forward_sin},0};
 list->SetComputeRoot32BitConstants(8,12,&constants,0);list->Dispatch(40,25,1);
 transition(list.Get(),state.output.Get(),state.output_state,D3D12_RESOURCE_STATE_COPY_SOURCE);transition(list.Get(),state.hits.Get(),state.hit_state,D3D12_RESOURCE_STATE_COPY_SOURCE);
 list->CopyBufferRegion(state.readback.Get(),0,state.hits.Get(),0,64001*sizeof(Hit));
 D3D12_TEXTURE_COPY_LOCATION src={},dst={};src.pResource=state.output.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint.Footprint={DXGI_FORMAT_B8G8R8A8_UNORM,state.width,state.height,1,state.row_pitch};dst.pResource=static_cast<ID3D12Resource*>(backbuffer);dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
 state.pending=true;return 1;
 }catch(const std::exception& error){state.enabled=false;std::fprintf(stderr,"DXR map: frame allocation failed: %s; normal present retained\n",error.what());return 0;}
}
extern "C" void DxrMap_Toggle(void) {if(!state.mode||!state.supported||state.disabled)return;state.enabled=!state.enabled;GB_RequestResetReason(GB_RESET_VIEW);std::fprintf(stderr,"DXR map: diagnostic %s\n",state.enabled?"enabled":"disabled");}
extern "C" void DxrMap_Unload(void) {
 if(state.ready||state.pending)wait_queue();release_scene();state.loaded=false;
 if(state.mode&&state.generation)std::fprintf(stderr,"DXR map: generation=%llu unloaded before level memory free\n",static_cast<unsigned long long>(state.generation));
}
extern "C" void DxrMap_Shutdown(void) {
 DxrMap_Unload();if(state.stats)std::fclose(state.stats);if(state.details)std::fclose(state.details);if(state.updates)std::fclose(state.updates);state=State();
}
extern "C" const DxrMapSceneView* DxrMap_GetScene(void) {return state.ready?&state.view:nullptr;}
extern "C" const MapSurface* DxrMap_GetSurface(unsigned instance,unsigned geometry,unsigned primitive) {
 if(!state.ready)return nullptr;
 for(unsigned i=0;i<state.view.geometry_count;i++){auto range=state.view.geometry_ranges[i];if(instance==range.instance_id&&geometry==range.geometry_index&&primitive<range.triangle_count)return &state.mesh.surfaces[state.mesh.triangle_surfaces[range.triangle_base+primitive]];}
 return nullptr;
}

extern "C" void DxrMap_AllowGameplay(void) {state.disabled=false;state.failed_generation=false;}
extern "C" void DxrMap_RequestSceneFor(unsigned consumer,int requested) {
 if(requested){state.consumers|=consumer;state.failed_generation=false;}else state.consumers&=~consumer;
}
extern "C" void DxrMap_RequestScene(int requested) {DxrMap_RequestSceneFor(DXR_MAP_CONSUMER_LIGHTING,requested);}
extern "C" int DxrMap_Available(void) { return state.supported && !state.disabled; }
