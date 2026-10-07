/* Bounded opaque-map point lighting. Shared renderer owns queue/TLAS. GPLv2. */
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <exception>
#include "dxr_lighting.h"
#include "dxr_map.h"
#include "gbuffer.h"
#include "rt_materials.h"
extern "C" {
#include "m_argv.h"
}
using Microsoft::WRL::ComPtr;
namespace {
constexpr unsigned count=320*200;
struct Sample {
    float position[3]; unsigned flags;
    float normal[3]; unsigned source_ambient;
    unsigned original,material,kind,reserved;
};
struct Result {
    float position[3]; unsigned flags;
    float distance,cosine; unsigned visible,original;
    unsigned final_color,source_ambient,material,kind;
    unsigned trace_status,steps,alpha_checks,alpha_rejected;
    unsigned candidate_surface,candidate_alpha;float candidate_uv[2];
};
struct TraceBounds {
    unsigned range_count,triangle_count,vertex_count,index_count;
    unsigned surface_count,descriptor_count,pixel_count,budget;
};
struct Constants {
    float position[3]; unsigned shadow;
    float color[3],intensity;
    float radius; unsigned width,height,row_words;
    TraceBounds bounds;unsigned opaque_diagnostic,alpha_debug,pad[2];
};
static_assert(sizeof(Sample)==48 && sizeof(Result)==80 && sizeof(Constants)==96,"shader layout");
struct State {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12Resource> samples,palette,output,results,readback,linear,dummy_material;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> pipeline;
    D3D12_RESOURCE_STATES output_state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    D3D12_RESOURCE_STATES result_state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    D3D12_RESOURCE_STATES linear_state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    DxrLightingSceneView view={};
    Constants constants={};
    bool configured=false,requested=false,active=false,pending=false,failed=false;
    const char *reason="not-configured";
    FILE *stats=nullptr,*details=nullptr;
    int detail_tic=-1,last_detail_tic=-1,tic=0;
    unsigned frame=0;
    uint64_t generation=0;
} state;
bool error(HRESULT hr,const char *operation) {
    if(SUCCEEDED(hr))return false;
    std::fprintf(stderr,"RT point light: %s failed (0x%08lx); original scene retained\n",operation,(unsigned long)hr);
    return true;
}
ComPtr<ID3D12Resource> buffer(UINT64 bytes,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES initial,D3D12_RESOURCE_FLAGS flags) {
    ComPtr<ID3D12Resource> resource;
    D3D12_HEAP_PROPERTIES heap={};heap.Type=type;
    D3D12_RESOURCE_DESC desc={};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width=bytes;desc.Height=desc.DepthOrArraySize=desc.MipLevels=1;
    desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;desc.Flags=flags;
    if(error(state.device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,initial,nullptr,IID_PPV_ARGS(&resource)),"buffer allocation"))return {};
    return resource;
}
bool upload(ID3D12Resource *resource,const void *data,size_t bytes) {
    void *mapped=nullptr;D3D12_RANGE no_read={0,0};
    if(error(resource->Map(0,&no_read,&mapped),"upload map"))return false;
    std::memcpy(mapped,data,bytes);D3D12_RANGE written={0,bytes};resource->Unmap(0,&written);return true;
}
void transition(ID3D12GraphicsCommandList *list,ID3D12Resource *resource,D3D12_RESOURCE_STATES& before,D3D12_RESOURCE_STATES after) {
    if(before==after)return;
    D3D12_RESOURCE_BARRIER b={};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};list->ResourceBarrier(1,&b);before=after;
}
bool pipeline() {
    wchar_t path[MAX_PATH];DWORD length=GetModuleFileNameW(nullptr,path,MAX_PATH);
    if(!length||length>=MAX_PATH)return false;
    auto slash=wcsrchr(path,L'\\');if(!slash)return false;*(slash+1)=0;
    if(wcslen(path)+wcslen(L"shaders\\dxr_lighting.cso")>=MAX_PATH)return false;
    wcscat_s(path,L"shaders\\dxr_lighting.cso");
    FILE *file=nullptr;_wfopen_s(&file,path,L"rb");
    if(!file){state.reason="missing-lighting-shader";return false;}
    if(std::fseek(file,0,SEEK_END)){std::fclose(file);return false;}
    long size=std::ftell(file);
    if(size<=0||size>16*1024*1024){std::fclose(file);return false;}
    std::vector<unsigned char> bytes;
    try{bytes.resize((size_t)size);}catch(...){std::fclose(file);throw;}
    std::rewind(file);bool read=std::fread(bytes.data(),1,bytes.size(),file)==bytes.size();std::fclose(file);
    if(!read)return false;
    D3D12_ROOT_PARAMETER parameters[14]={};
    for(unsigned i=0;i<10;i++){parameters[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;parameters[i].Descriptor.ShaderRegister=i;}
    for(unsigned i=0;i<3;i++){parameters[i+10].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;parameters[i+10].Descriptor.ShaderRegister=i;}
    parameters[13].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[13].Constants.ShaderRegister=0;parameters[13].Constants.Num32BitValues=24;
    D3D12_ROOT_SIGNATURE_DESC description={};description.NumParameters=14;description.pParameters=parameters;
    ComPtr<ID3DBlob> blob,errors;
    if(error(D3D12SerializeRootSignature(&description,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors),"lighting root serialization")||
       error(state.device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&state.root)),"lighting root"))return false;
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc={};desc.pRootSignature=state.root.Get();desc.CS={bytes.data(),bytes.size()};
    return !error(state.device->CreateComputePipelineState(&desc,IID_PPV_ARGS(&state.pipeline)),"lighting pipeline");
}
FILE *observation(const char *flag) {
    int p=M_CheckParm(const_cast<char*>(flag));if(!p||p+1>=myargc)return nullptr;
    HANDLE file=CreateFileA(myargv[p+1],GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){std::fprintf(stderr,"RT point light: %s refuses existing/unavailable path\n",flag);return nullptr;}
    CloseHandle(file);return std::fopen(myargv[p+1],"w");
}
void collect() {
    if(!state.pending)return;state.pending=false;
    if(!state.readback)return;
    void *mapped=nullptr;D3D12_RANGE range={0,count*sizeof(Result)};
    if(error(state.readback->Map(0,&range,&mapped),"lighting readback")){state.failed=true;state.reason="readback-failed";return;}
    const auto *results=static_cast<const Result*>(mapped);
    unsigned eligible=0,rays=0,blocked=0,added=0,errors=0,exhausted=0,checks=0,rejected=0;double contribution=0;
    bool detail=state.details&&state.tic==state.detail_tic&&state.last_detail_tic!=state.tic;
    for(unsigned i=0;i<count;i++) {
        const auto &r=results[i];eligible+=(r.flags&1)!=0;rays+=(r.flags&2)!=0;
        blocked+=(r.flags&4)!=0;added+=(r.flags&8)!=0;
        contribution+=r.final_color!=r.original;errors+=r.trace_status==2;exhausted+=r.trace_status==3;checks+=r.alpha_checks;rejected+=r.alpha_rejected;
        if(detail&&(r.flags&1))std::fprintf(state.details,"%u,%d,%llu,%u,%u,%.7g,%.7g,%.7g,%u,%u,%u,%u,%.7g,%.7g,%u,%u,%u,%u,%u,%u,%u,%u,%u,%.9g,%.9g,%u\n",
          state.frame,state.tic,(unsigned long long)state.generation,i%320,i/320,r.position[0],r.position[1],r.position[2],r.kind,r.material,r.source_ambient&255,(r.source_ambient>>8)&255,r.distance,r.cosine,r.visible,r.flags,r.original,r.final_color,r.trace_status,r.steps,r.alpha_checks,r.alpha_rejected,r.candidate_surface,r.candidate_uv[0],r.candidate_uv[1],r.candidate_alpha);
    }
    if(detail){state.last_detail_tic=state.tic;std::fflush(state.details);}
    if(state.stats){std::fprintf(state.stats,"%u,%d,%llu,%u,%u,%u,%u,%.0f,%.7g,%.7g,%.7g,%u,%.7g,%.7g,%u,%u,%u,%u\n",state.frame,state.tic,(unsigned long long)state.generation,eligible,rays,blocked,added,contribution,state.constants.position[0],state.constants.position[1],state.constants.position[2],state.constants.shadow,state.constants.intensity,state.constants.radius,checks,rejected,errors,exhausted);std::fflush(state.stats);}
    if(state.frame==1||detail)std::fprintf(stderr,"RT point light GPU: tic=%d generation=%llu eligible=%u rays=%u blocked=%u contributed=%u budget=64000 alpha_checks=%u rejected=%u errors=%u exhausted=%u\n",state.tic,(unsigned long long)state.generation,eligible,rays,blocked,added,checks,rejected,errors,exhausted);
    D3D12_RANGE no_write={0,0};state.readback->Unmap(0,&no_write);
}
void copy(ID3D12GraphicsCommandList *list,ID3D12Resource *destination,bool expanded) {
    D3D12_TEXTURE_COPY_LOCATION source={},target={};
    source.pResource=state.output.Get();source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint.Offset=expanded?count*4ull:0;
    source.PlacedFootprint.Footprint={expanded?DXGI_FORMAT_B8G8R8A8_UNORM:DXGI_FORMAT_R8G8B8A8_UNORM,
        expanded?state.constants.width:320,expanded?state.constants.height:200,1,
        expanded?state.constants.row_words*4:320*4};
    target.pResource=destination;target.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    list->CopyTextureRegion(&target,0,0,0,&source,nullptr);
}
}
extern "C" void DxrLighting_Init(void *device,unsigned width,unsigned height) {
    state.device=static_cast<ID3D12Device*>(device);
    RtMaterials_Init(device);
    state.constants.width=width;state.constants.height=height;state.constants.row_words=(width*4+255)/256*64;
    state.constants.shadow=!M_CheckParm("-rt-shadows-off");
    state.constants.bounds.budget=64;state.constants.opaque_diagnostic=M_CheckParm("-rt-alpha-opaque")!=0;state.constants.alpha_debug=M_CheckParm("-rt-alpha-debug")!=0;
    int budget=M_CheckParm("-rt-alpha-budget");if(budget&&budget+1<myargc){char *end=nullptr;long n=std::strtol(myargv[budget+1],&end,10);if(*myargv[budget+1]&&!*end&&n>=1&&n<=64)state.constants.bounds.budget=(unsigned)n;}
    if(state.constants.opaque_diagnostic||state.constants.alpha_debug||state.constants.bounds.budget!=64)std::fprintf(stderr,"RT alpha diagnostic: opaque=%u debug=%u budget=%u; comparison mode\n",state.constants.opaque_diagnostic,state.constants.alpha_debug,state.constants.bounds.budget);
    int arg=M_CheckParm("-rt-light");
    if(arg) {
        float values[8];bool valid=arg+8<myargc;
        for(int i=0;i<8&&valid;i++){char *end=nullptr;double v=std::strtod(myargv[arg+1+i],&end);valid=*myargv[arg+1+i]&&!*end&&std::isfinite(v)&&std::abs(v)<=1e8;values[i]=(float)v;}
        if(valid)valid=values[3]>=0&&values[3]<=1&&values[4]>=0&&values[4]<=1&&values[5]>=0&&values[5]<=1&&values[6]>=0&&values[7]>0.0625&&values[7]<=8192;
        if(valid){state.constants.position[0]=values[0];state.constants.position[1]=values[2];state.constants.position[2]=values[1];std::memcpy(state.constants.color,values+3,12);state.constants.intensity=values[6];state.constants.radius=values[7];state.configured=true;}
        else std::fprintf(stderr,"RT point light: invalid -rt-light; expected DOOM x y height, linear RGB[0,1], intensity[0,1e8], radius(0.0625,8192]; original rendering retained\n");
    }
    if(width!=1280||height!=800){state.reason="unsupported-output-size";state.failed=true;}
    state.requested=state.configured&&!M_CheckParm("-nort")&&!M_CheckParm("-rt-light-off");
    state.reason=state.configured?(state.requested?"pending-scene":"disabled"):"not-configured";
    DxrMap_RequestScene(state.requested);
    state.stats=observation("-rt-light-stats");state.details=observation("-rt-light-pixels");
    int tic=M_CheckParm("-rt-light-pixel-tic");
    if(tic&&tic+1<myargc){char *end=nullptr;long value=std::strtol(myargv[tic+1],&end,10);if(*myargv[tic+1]&&!*end&&value>=0&&value<10000000)state.detail_tic=(int)value;}
    if(state.stats)std::fprintf(state.stats,"frame,game_tic,generation,eligible,rays,blocked,contributed,changed_pixels,light_x,light_height,light_y,shadows,intensity,radius,alpha_checks,alpha_rejected,alpha_errors,alpha_exhausted\n");
    if(state.details)std::fprintf(state.details,"frame,game_tic,generation,x,y,world_x,world_height,world_y,kind,material,source_index,ambient_index,distance,cosine,visible,flags,original_rgba,final_rgba,trace_status,steps,alpha_checks,alpha_rejected,candidate_surface,candidate_u,candidate_v,candidate_alpha\n");
    if(state.configured)std::fprintf(stderr,"RT point light: requested=%d position=(%.6g,%.6g,%.6g) linear RGB=(%.6g,%.6g,%.6g) intensity=%.6g radius=%.6g shadows=%u lights<=1 rays<=64000\n",state.requested,state.constants.position[0],state.constants.position[1],state.constants.position[2],state.constants.color[0],state.constants.color[1],state.constants.color[2],state.constants.intensity,state.constants.radius,state.constants.shadow);
}
extern "C" void DxrLighting_RequestEnabled(int enabled) {
    bool requested=enabled!=0&&state.configured;
    if(requested==state.requested)return;
    state.requested=requested;state.failed=false;state.active=false;
    state.reason=requested?"pending-scene":"disabled";DxrMap_RequestScene(requested);GB_RequestResetReason(GB_RESET_EXPLICIT);
}
extern "C" DxrLightingStatus DxrLighting_GetStatus(void) {
    return {state.requested?1:0,state.active?1:0,DxrMap_Available()&&state.configured?1:0,state.reason};
}
extern "C" DxrLightingConfig DxrLighting_GetConfig(void) {
    DxrLightingConfig out={};out.configured=state.configured;out.requested=state.requested;out.shadows=state.constants.shadow;
    std::memcpy(out.position,state.constants.position,12);std::memcpy(out.color,state.constants.color,12);out.intensity=state.constants.intensity;out.radius=state.constants.radius;return out;
}
extern "C" const DxrLightingSceneView *DxrLighting_GetSceneView(void) { return state.active?&state.view:nullptr; }
extern "C" void DxrLighting_Prepare(void) {
    collect();state.active=false;
    if(!state.requested||state.failed||!GB_GetFrameInputs()->scene_valid)return;
    if(!DxrMap_Available()){state.reason="dxr-unavailable";return;}
    if(!DxrMap_GetScene()){state.reason="scene-unavailable";return;}
    RtMaterials_Prepare(DxrMap_GetScene());
    try {
        if(!state.pipeline&&!pipeline()){state.failed=true;if(!std::strcmp(state.reason,"pending-scene"))state.reason="lighting-pipeline-failed";return;}
        if(!state.samples||!state.palette||!state.output||!state.results||!state.linear||!state.dummy_material) {
            state.output_state=state.result_state=state.linear_state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            state.samples=buffer(count*sizeof(Sample),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
            state.palette=buffer(256*sizeof(unsigned),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
            state.output=buffer(count*4ull+(UINT64)state.constants.row_words*state.constants.height*4,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            state.linear=buffer(count*16ull,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            state.dummy_material=buffer(256,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
            unsigned char zero[256]={};if(state.dummy_material)upload(state.dummy_material.Get(),zero,sizeof(zero));
            state.results=buffer(count*sizeof(Result),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            if(state.stats||state.details)state.readback=buffer(count*sizeof(Result),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_FLAG_NONE);
            if(!state.samples||!state.palette||!state.output||!state.results||!state.linear||!state.dummy_material||((state.stats||state.details)&&!state.readback)){state.failed=true;state.reason="lighting-allocation-failed";return;}
        }
    }catch(const std::exception& e){state.failed=true;state.reason="lighting-allocation-failed";std::fprintf(stderr,"RT point light: prepare failed: %s\n",e.what());}
}
extern "C" int DxrLighting_Evaluate(void *commands,void *scene_texture) {
    if(!state.requested||state.failed||!state.pipeline||!state.output||GB_GetDebugView()!=GB_VIEW_COLOR||!GB_GetFrameInputs()->scene_valid)return 0;
    const auto *scene=DxrMap_GetScene();if(!scene)return 0;
    try {
        std::vector<Sample> samples(count);unsigned palette[256];
        auto color=GB_ColorRGBA();auto normal=GB_NormalRGBA();auto material=GB_MaterialSamples();auto kind=GB_SurfaceKind();
        for(unsigned i=0;i<count;i++) {
            auto &s=samples[i];s.source_ambient=material[i].source_index|((unsigned)material[i].ambient_index<<8);s.material=material[i].material_id;s.kind=material[i].kind;
            s.original=color[i*4]|((unsigned)color[i*4+1]<<8)|((unsigned)color[i*4+2]<<16)|0xff000000;
            for(unsigned c=0;c<3;c++)s.normal[c]=normal[i*4+c]/127.5f-1;
            if(material[i].valid&&kind[i]<=GB_KIND_CEILING&&GB_SampleWorldPosition(i%320,i/320,s.position))s.flags=1;
        }
        auto raw=GB_RawPaletteRGB();auto gamma=GB_GammaLUT();
        for(unsigned i=0;i<256;i++)palette[i]=raw[i*3]|((unsigned)raw[i*3+1]<<8)|((unsigned)raw[i*3+2]<<16)|((unsigned)gamma[i]<<24);
        if(!upload(state.samples.Get(),samples.data(),samples.size()*sizeof(Sample))||!upload(state.palette.Get(),palette,sizeof(palette))){state.failed=true;state.reason="lighting-upload-failed";return 0;}
        auto *list=static_cast<ID3D12GraphicsCommandList*>(commands);
        const auto *materials=RtMaterials_GetView();
        state.constants.bounds.range_count=scene->geometry_count;state.constants.bounds.triangle_count=(unsigned)scene->mesh->triangle_count;
        state.constants.bounds.vertex_count=(unsigned)scene->mesh->vertex_count;state.constants.bounds.index_count=(unsigned)scene->mesh->triangle_count*3;
        state.constants.bounds.surface_count=materials?materials->surface_count:0;state.constants.bounds.descriptor_count=materials?materials->descriptor_count:0;state.constants.bounds.pixel_count=materials?materials->pixel_count:0;
        transition(list,state.output.Get(),state.output_state,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        transition(list,state.results.Get(),state.result_state,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        transition(list,state.linear.Get(),state.linear_state,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        list->SetPipelineState(state.pipeline.Get());list->SetComputeRootSignature(state.root.Get());
        list->SetComputeRootShaderResourceView(0,scene->tlas_gpu_address);
        list->SetComputeRootShaderResourceView(1,state.samples->GetGPUVirtualAddress());list->SetComputeRootShaderResourceView(2,state.palette->GetGPUVirtualAddress());
        ID3D12Resource *scene_resources[]={static_cast<ID3D12Resource*>(scene->primitive_resource),static_cast<ID3D12Resource*>(scene->geometry_resource),static_cast<ID3D12Resource*>(scene->vertex_resource),static_cast<ID3D12Resource*>(scene->index_resource)};
        for(unsigned i=0;i<4;i++)list->SetComputeRootShaderResourceView(3+i,scene_resources[i]->GetGPUVirtualAddress());
        ID3D12Resource *material_resources[]={materials?static_cast<ID3D12Resource*>(materials->surface_resource):state.dummy_material.Get(),materials?static_cast<ID3D12Resource*>(materials->descriptor_resource):state.dummy_material.Get(),materials?static_cast<ID3D12Resource*>(materials->pixel_resource):state.dummy_material.Get()};
        for(unsigned i=0;i<3;i++)list->SetComputeRootShaderResourceView(7+i,material_resources[i]->GetGPUVirtualAddress());
        list->SetComputeRootUnorderedAccessView(10,state.output->GetGPUVirtualAddress());list->SetComputeRootUnorderedAccessView(11,state.results->GetGPUVirtualAddress());list->SetComputeRootUnorderedAccessView(12,state.linear->GetGPUVirtualAddress());
        list->SetComputeRoot32BitConstants(13,24,&state.constants,0);list->Dispatch(40,25,1);
        transition(list,state.output.Get(),state.output_state,D3D12_RESOURCE_STATE_COPY_SOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        transition(list,state.results.Get(),state.result_state,D3D12_RESOURCE_STATE_COPY_SOURCE);
        transition(list,state.linear.Get(),state.linear_state,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        state.view={state.linear.Get(),state.output.Get()};
        copy(list,static_cast<ID3D12Resource*>(scene_texture),false);
        if(state.readback){list->CopyBufferRegion(state.readback.Get(),0,state.results.Get(),0,count*sizeof(Result));state.pending=true;}
        state.frame=GB_GetFrameInputs()->frame_id;state.tic=GB_GetFrameInputs()->game_tic;state.generation=scene->generation;
        state.active=true;state.reason="active-alpha-point-light";return 1;
    }catch(const std::exception& e){state.failed=true;state.reason="lighting-frame-failed";std::fprintf(stderr,"RT point light: frame failed: %s; original scene retained\n",e.what());return 0;}
}
extern "C" int DxrLighting_PresentNearest(void *commands,void *backbuffer) {
    if(!state.active)return 0;
    copy(static_cast<ID3D12GraphicsCommandList*>(commands),static_cast<ID3D12Resource*>(backbuffer),true);return 1;
}
extern "C" void DxrLighting_Shutdown(void) {
    collect();RtMaterials_Shutdown();DxrMap_RequestScene(0);if(state.stats)std::fclose(state.stats);if(state.details)std::fclose(state.details);state=State();
}
