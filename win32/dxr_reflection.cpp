/* Single deterministic map reflection, shared scene/material/lighting. GPLv2. */
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
#include <algorithm>
#include <stdexcept>
#include "dxr_reflection.h"
#include "reflection_config.h"
#include "dxr_lighting.h"
#include "dxr_map.h"
#include "rt_materials.h"
#include "r_material.h"
#include "gbuffer.h"
#include "gpu_timing.h"
extern "C" {
#include "m_argv.h"
}
using Microsoft::WRL::ComPtr;
namespace {
constexpr unsigned count=64000;
struct Sample {float position[3];unsigned flags;float normal[3];unsigned source_ambient;unsigned original,material,kind,descriptor;};
struct Setting {float roughness,specular;unsigned reflect,reserved;float emissive[3];unsigned padding;};
struct Result {
 float position[3];unsigned flags;float hit_position[3];unsigned status;
 float uv[2];unsigned surface,triangle;float reflection[3];unsigned descriptor;
 float hit_t;unsigned steps,shadow_steps,shadow_status;unsigned source_index,ambient_index,original,final_color;
 unsigned alpha_checks,rejected,instance,geometry;float hit_normal[3];unsigned reserved;
 float candidate_uv[2];unsigned candidate_surface,candidate_alpha;
};
struct Constants {
 float camera[3];unsigned use_lighting;float light_position[3];unsigned shadows;
 float light_color[3],intensity;float radius;unsigned width,height,row_words;
 float environment[3];unsigned light_enabled;unsigned bounds[8];unsigned color_map_count,padding[3];
};
static_assert(sizeof(Sample)==48&&sizeof(Setting)==32&&sizeof(Result)==144&&sizeof(Constants)==128,"reflection shader layout");
struct State {
 ComPtr<ID3D12Device> device;
 ComPtr<ID3D12Resource> samples,palette,output,results,readback,parameters,color_maps,constants_buffer,base_linear,base_rgba;
 ComPtr<ID3D12RootSignature> root;
 ComPtr<ID3D12PipelineState> trace_pipeline,compose_pipeline;
 D3D12_RESOURCE_STATES output_state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS,result_state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
 ReflectionConfig config={};Constants constants={};std::vector<Setting> settings;
 bool configured=false,requested=false,active=false,pending=false,failed=false,history_reset=false;
 const char *reason="not-configured";FILE *stats=nullptr,*details=nullptr;
 unsigned budget=64,frame=0,visible=0;int detail_tic=-1,last_detail_tic=-1,tic=0;uint64_t generation=0,revision=0;
 unsigned temporal_reset=0,ngx_evaluated=0,fsr_evaluated=0,frame_reset_reasons=0,frame_history_valid=0;
 uint64_t geometry_revision=0,shading_revision=0;
} state;
bool error(HRESULT hr,const char *operation) {
    if(SUCCEEDED(hr))return false;
    std::fprintf(stderr,"RT reflection: %s failed (0x%08lx); original scene retained\n",operation,(unsigned long)hr);
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
 D3D12_ROOT_PARAMETER parameters[17]={};
 for(unsigned i=0;i<14;i++){parameters[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;parameters[i].Descriptor.ShaderRegister=i;}
 for(unsigned i=0;i<2;i++){parameters[i+14].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;parameters[i+14].Descriptor.ShaderRegister=i;}
 parameters[16].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;parameters[16].Descriptor.ShaderRegister=0;
 D3D12_ROOT_SIGNATURE_DESC desc={};desc.NumParameters=17;desc.pParameters=parameters;
 ComPtr<ID3DBlob> blob,errors;
 if(error(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors),"root serialization")||error(state.device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&state.root)),"root creation"))return false;
 const wchar_t *names[]={L"shaders\\dxr_reflection_trace.cso",L"shaders\\dxr_reflection_compose.cso"};
 for(unsigned n=0;n<2;n++) {
  wchar_t path[MAX_PATH];DWORD length=GetModuleFileNameW(nullptr,path,MAX_PATH);if(!length||length>=MAX_PATH)return false;
  auto slash=wcsrchr(path,L'\\');if(!slash)return false;*(slash+1)=0;
  if(wcslen(path)+wcslen(names[n])>=MAX_PATH)return false;wcscat_s(path,names[n]);
  FILE *file=nullptr;_wfopen_s(&file,path,L"rb");if(!file){state.reason="missing-reflection-shader";return false;}
  if(std::fseek(file,0,SEEK_END)){std::fclose(file);return false;}long size=std::ftell(file);
  if(size<=0||size>16*1024*1024){std::fclose(file);return false;}
  std::vector<unsigned char> bytes;try{bytes.resize((size_t)size);}catch(...){std::fclose(file);throw;}
  std::rewind(file);bool ok=std::fread(bytes.data(),1,bytes.size(),file)==bytes.size();std::fclose(file);if(!ok)return false;
  D3D12_COMPUTE_PIPELINE_STATE_DESC p={};p.pRootSignature=state.root.Get();p.CS={bytes.data(),bytes.size()};
  ID3D12PipelineState **out=n?state.compose_pipeline.ReleaseAndGetAddressOf():state.trace_pipeline.ReleaseAndGetAddressOf();
  if(error(state.device->CreateComputePipelineState(&p,IID_PPV_ARGS(out)),"reflection pipeline"))return false;
 }
 return true;
}
void collect() {
 if(!state.pending)return;state.pending=false;
 void *mapped=nullptr;D3D12_RANGE range={0,count*(sizeof(Result)+4)};
 if(error(state.readback->Map(0,&range,&mapped),"readback")){state.failed=true;state.reason="reflection-readback-failed";return;}
 auto results=static_cast<const Result*>(mapped);auto final=reinterpret_cast<const unsigned*>(results+count);
 unsigned rays=0,hits=0,miss=0,errors=0,exhausted=0,shadow=0,blocked=0,changed=0,checks=0,rejected=0,max_steps=0;
 bool detail=state.details&&state.tic==state.detail_tic&&state.last_detail_tic!=state.tic;
 for(unsigned i=0;i<count;i++) {
  const auto &r=results[i];rays+=(r.flags&2)!=0;hits+=(r.flags&4)!=0;miss+=(r.flags&8)!=0;errors+=(r.flags&16)!=0;exhausted+=(r.flags&32)!=0;
  shadow+=(r.flags&64)!=0;blocked+=(r.flags&128)!=0;changed+=r.original!=final[i];checks+=r.alpha_checks;rejected+=r.rejected;max_steps=std::max(max_steps,std::max(r.steps,r.shadow_steps));
  if(detail&&(r.flags&2))std::fprintf(state.details,"%u,%d,%llu,%llu,%u,%u,%u,%u,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%.9g,%.9g,%.9g,%u,%u,%u,%u,%.9g,%.9g,%.9g,%.9g,%.9g,%u,%u,%llu,%llu\n",
   state.frame,state.tic,(unsigned long long)state.generation,(unsigned long long)state.revision,i%320,i/320,r.flags,r.status,
   r.position[0],r.position[1],r.position[2],r.hit_position[0],r.hit_position[1],r.hit_position[2],r.uv[0],r.uv[1],r.hit_t,r.surface,r.triangle,r.instance,r.geometry,r.source_index,r.ambient_index,r.steps,r.shadow_steps,r.shadow_status,r.original,final[i],r.reflection[0],r.reflection[1],r.reflection[2],r.descriptor,(r.reserved>>16)&255,r.reserved&255,(r.reserved>>8)&255,r.hit_normal[0],r.hit_normal[1],r.hit_normal[2],r.candidate_uv[0],r.candidate_uv[1],r.candidate_surface,r.candidate_alpha,(unsigned long long)state.geometry_revision,(unsigned long long)state.shading_revision);
 }
 if(detail){state.last_detail_tic=state.tic;std::fflush(state.details);}
 if(state.stats){std::fprintf(state.stats,"%u,%d,%llu,%llu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",state.frame,state.tic,(unsigned long long)state.generation,(unsigned long long)state.revision,rays,hits,miss,errors,exhausted,shadow,blocked,changed,checks,rejected,max_steps,state.budget,state.history_reset?1:0,state.temporal_reset,state.ngx_evaluated,state.fsr_evaluated,state.frame_reset_reasons,state.frame_history_valid);std::fflush(state.stats);}
 if(state.frame==1||detail)std::fprintf(stderr,"RT reflection GPU: tic=%d rays=%u hits=%u miss=%u errors=%u exhausted=%u hit-shadow=%u changed=%u steps<=%u; sprites excluded, no recursion\n",state.tic,rays,hits,miss,errors,exhausted,shadow,changed,state.budget);
 D3D12_RANGE none={0,0};state.readback->Unmap(0,&none);
}
float linear(unsigned char channel) {float s=channel/255.f;return s<=0.04045f?s/12.92f:std::pow((s+0.055f)/1.055f,2.4f);}
FILE *observation(const char *flag) {
    int p=M_CheckParm(const_cast<char*>(flag));if(!p||p+1>=myargc)return nullptr;
    HANDLE file=CreateFileA(myargv[p+1],GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){std::fprintf(stderr,"RT reflection: %s refuses existing/unavailable path\n",flag);return nullptr;}
    CloseHandle(file);return std::fopen(myargv[p+1],"w");
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
extern "C" void DxrReflection_Init(void *device,unsigned width,unsigned height) {
 state.device=static_cast<ID3D12Device*>(device);state.constants.width=width;state.constants.height=height;state.constants.row_words=(width*4+255)/256*64;
 int arg=M_CheckParm("-rt-materials");
 if(arg&&arg+1<myargc) {
  FILE *file=nullptr;fopen_s(&file,myargv[arg+1],"rb");
  if(file) {
   try {
    std::vector<char> text(REFLECTION_MAX_CONFIG_BYTES+1);size_t size=std::fread(text.data(),1,text.size(),file);bool ok=!std::ferror(file);std::fclose(file);file=nullptr;char reason[160]={};
    state.configured=ok&&ReflectionConfig_Parse(text.data(),size,&state.config,reason,sizeof(reason));
    if(!state.configured)std::fprintf(stderr,"RT reflection: invalid config: %s; original/light scene retained\n",reason);
   } catch(const std::exception &e) {
    if(file)std::fclose(file);state.configured=false;state.reason="reflection-config-allocation-failed";
    std::fprintf(stderr,"RT reflection: %s; original/light scene retained\n",e.what());
   }
  } else std::fprintf(stderr,"RT reflection: config unavailable; original/light scene retained\n");
 }
 int b=M_CheckParm("-rt-ray-budget");if(b&&b+1<myargc){char *end=nullptr;long value=std::strtol(myargv[b+1],&end,10);if(*myargv[b+1]&&!*end&&value>=1&&value<=64)state.budget=(unsigned)value;else{state.configured=false;state.reason="invalid-ray-budget";}}
 state.requested=state.configured&&!M_CheckParm("-nort")&&!M_CheckParm("-rt-reflections-off");
 if(width!=1280||height!=800){state.failed=true;state.reason="unsupported-output-size";}
 else if(!state.configured&&!std::strcmp(state.reason,"not-configured"))state.reason="not-configured";
 else if(state.configured)state.reason=state.requested?"pending-scene":"disabled";
 DxrMap_RequestSceneFor(DXR_MAP_CONSUMER_REFLECTION,state.requested);
 state.stats=observation("-rt-reflection-stats");state.details=observation("-rt-reflection-pixels");
 int tic=M_CheckParm("-rt-reflection-pixel-tic");if(tic&&tic+1<myargc){char *end=nullptr;long t=std::strtol(myargv[tic+1],&end,10);if(*myargv[tic+1]&&!*end&&t>=0&&t<10000000)state.detail_tic=(int)t;}
 if(state.stats)std::fprintf(state.stats,"frame,game_tic,generation,material_revision,rays,hits,miss,errors,exhausted,hit_shadow_rays,hit_shadow_blocked,changed_pixels,alpha_checks,alpha_rejected,max_steps,budget,reflection_reset_requested,temporal_reset_arg,ngx_evaluated,fsr_evaluated,frame_input_reset_reasons,frame_input_history_valid\n");
 if(state.details)std::fprintf(state.details,"frame,game_tic,generation,material_revision,x,y,flags,status,world_x,world_height,world_y,hit_x,hit_height,hit_y,u,v,hit_distance,surface,triangle,instance,geometry,source_index,ambient_index,steps,shadow_steps,shadow_status,original_rgba,final_rgba,reflection_r,reflection_g,reflection_b,receiver_descriptor,receiver_kind,receiver_source_index,receiver_ambient_index,hit_normal_x,hit_normal_height,hit_normal_y,reflection_candidate_u,reflection_candidate_v,reflection_candidate_surface,reflection_candidate_alpha,geometry_revision,shading_revision\n");
 if(state.configured)std::fprintf(stderr,"RT reflection: requested=%d material rules=%u one bounce, <=64000 reflected rays + <=64000 hit-shadow rays, steps<=%u; stateless spatial low-roughness approximation; recurring SR reset\n",state.requested,state.config.material_count,state.budget);
}
extern "C" void DxrReflection_RequestEnabled(int enabled) {
 bool requested=enabled!=0&&state.configured;if(requested==state.requested)return;
 state.requested=requested;state.active=state.history_reset=false;state.failed=false;
 state.reason=requested?"pending-scene":"disabled";DxrMap_RequestSceneFor(DXR_MAP_CONSUMER_REFLECTION,requested);GB_RequestResetReason(GB_RESET_EXPLICIT);
}
extern "C" DxrReflectionStatus DxrReflection_GetStatus(void) {return {state.requested?1:0,state.active?1:0,state.configured&&DxrMap_Available()?1:0,state.reason};}
extern "C" void DxrReflection_Prepare(void) {
 collect();state.active=state.history_reset=false;state.visible=0;
 if(!state.requested||state.failed||!GB_GetFrameInputs()->scene_valid)return;
 if(!DxrMap_Available()){state.reason="dxr-unavailable";return;}
 const auto *scene=DxrMap_GetScene();if(!scene){state.reason="scene-unavailable";return;}
 RtMaterials_Prepare(scene);const auto *materials=RtMaterials_GetView();if(!materials){state.reason="material-catalog-unavailable";return;}
 try {
  if((!state.trace_pipeline||!state.compose_pipeline)&&!pipeline()){state.failed=true;if(!std::strcmp(state.reason,"pending-scene"))state.reason="reflection-pipeline-failed";return;}
  if(!state.samples||!state.output||!state.results) {
   state.samples=buffer(count*sizeof(Sample),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
   state.palette=buffer(256*4,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
   state.constants_buffer=buffer(256,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
   state.base_linear=buffer(count*16,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
   state.base_rgba=buffer(count*4,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
   state.output=buffer(count*4ull+(UINT64)state.constants.row_words*state.constants.height*4,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   state.results=buffer(count*sizeof(Result),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   if(state.stats||state.details)state.readback=buffer(count*(sizeof(Result)+4ull),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_FLAG_NONE);
   if(!state.samples||!state.palette||!state.constants_buffer||!state.base_linear||!state.base_rgba||!state.output||!state.results||((state.stats||state.details)&&!state.readback))throw std::runtime_error("buffer allocation failed");
  }
  state.settings.assign(materials->descriptor_count,Setting{});
  for(unsigned i=0;i<materials->descriptor_count;i++) {
   const auto &d=materials->descriptors[i];R_MaterialDescription description={};
   if(!(d.flat_namespace?R_DescribeFlat(d.base_id,&description):R_DescribeTexture(d.base_id,&description)))throw std::runtime_error("base material unavailable");
   for(unsigned j=0;j<state.config.material_count;j++) {
    const auto &c=state.config.materials[j];if(c.flat_namespace!=d.flat_namespace||_stricmp(c.name,description.name))continue;
    state.settings[i]={c.roughness,c.specular,c.reflect,0,{c.emissive[0],c.emissive[1],c.emissive[2]},0};break;
   }
  }
  state.parameters=buffer(state.settings.size()*sizeof(Setting),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
  if(!state.parameters||!upload(state.parameters.Get(),state.settings.data(),state.settings.size()*sizeof(Setting)))throw std::runtime_error("parameter upload failed");
  unsigned rows=R_ColorMapCount();if(!rows||rows>256)throw std::runtime_error("COLORMAP row bounds rejected");
  std::vector<unsigned> maps(rows*256);unsigned char row[256];
  for(unsigned r=0;r<rows;r++){if(!R_CopyColorMap(r,row,sizeof(row)))throw std::runtime_error("COLORMAP copy unavailable");for(unsigned i=0;i<256;i++)maps[r*256+i]=row[i];}
  state.color_maps=buffer(maps.size()*4,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
  if(!state.color_maps||!upload(state.color_maps.Get(),maps.data(),maps.size()*4))throw std::runtime_error("COLORMAP upload failed");
  state.constants.color_map_count=rows;
 }catch(const std::exception &e){state.failed=true;state.reason="reflection-prepare-failed";std::fprintf(stderr,"RT reflection: %s; base scene retained\n",e.what());}
}
extern "C" int DxrReflection_Evaluate(void *commands,void *scene_texture) {
 if(!state.requested||state.failed||!state.trace_pipeline||!state.compose_pipeline||!state.parameters||GB_GetDebugView()!=GB_VIEW_COLOR||!GB_GetFrameInputs()->scene_valid)return 0;
 const auto *scene=DxrMap_GetScene();const auto *materials=RtMaterials_GetView();if(!scene||!materials)return 0;
 try {
  std::vector<Sample> samples(count);std::vector<float> base(count*4);std::vector<unsigned> original(count);unsigned palette[256];
  auto color=GB_ColorRGBA();auto normal=GB_GeometricNormalXYZ();auto material=GB_MaterialSamples();auto kind=GB_SurfaceKind();auto raw=GB_RawPaletteRGB();auto gamma=GB_GammaLUT();
  for(unsigned i=0;i<count;i++) {
   auto &s=samples[i];s.descriptor=UINT32_MAX;s.material=material[i].material_id;s.kind=material[i].kind;s.source_ambient=material[i].source_index|((unsigned)material[i].ambient_index<<8);
   s.original=color[i*4]|((unsigned)color[i*4+1]<<8)|((unsigned)color[i*4+2]<<16)|0xff000000;original[i]=s.original;
   for(unsigned c=0;c<3;c++){s.normal[c]=normal[i*3+c];base[i*4+c]=linear(raw[material[i].ambient_index*3+c]);}
   if(material[i].valid&&kind[i]<=GB_KIND_CEILING&&GB_SampleWorldPosition(i%320,i/320,s.position)) {
    s.flags=1;base[i*4+3]=1;unsigned flat=s.kind==GB_KIND_FLOOR||s.kind==GB_KIND_CEILING;
    // Software material context records the original sidedef/flat ID. The
    // borrowed atlas descriptor independently resolves its current animation.
    for(unsigned d=0;d<materials->descriptor_count;d++)if(materials->descriptors[d].flat_namespace==flat&&materials->descriptors[d].base_id==s.material){s.descriptor=d;break;}
    if(s.descriptor<state.settings.size()&&state.settings[s.descriptor].reflect&&s.kind<=GB_KIND_FLOOR)state.visible++;
   }
  }
  for(unsigned i=0;i<256;i++)palette[i]=raw[i*3]|((unsigned)raw[i*3+1]<<8)|((unsigned)raw[i*3+2]<<16)|((unsigned)gamma[i]<<24);
  const auto *lighting=DxrLighting_GetSceneView();auto light=DxrLighting_GetConfig();state.constants.use_lighting=lighting?1:0;
  std::memcpy(state.constants.camera,GB_GetFrameInputs()->sampled.position,12);std::memcpy(state.constants.light_position,light.position,12);std::memcpy(state.constants.light_color,light.color,12);
  state.constants.intensity=light.intensity;state.constants.radius=light.radius;state.constants.shadows=light.shadows;state.constants.light_enabled=light.configured&&light.requested;
  std::memcpy(state.constants.environment,state.config.environment,12);
  unsigned bounds[]={scene->geometry_count,(unsigned)scene->mesh->triangle_count,(unsigned)scene->mesh->vertex_count,(unsigned)(scene->mesh->triangle_count*3),materials->surface_count,materials->descriptor_count,materials->pixel_count,state.budget};std::memcpy(state.constants.bounds,bounds,sizeof(bounds));
  if(!upload(state.samples.Get(),samples.data(),samples.size()*sizeof(Sample))||!upload(state.palette.Get(),palette,sizeof(palette))||!upload(state.base_linear.Get(),base.data(),base.size()*4)||!upload(state.base_rgba.Get(),original.data(),original.size()*4)||!upload(state.constants_buffer.Get(),&state.constants,sizeof(state.constants)))throw std::runtime_error("frame upload failed");
  auto *list=static_cast<ID3D12GraphicsCommandList*>(commands);
  transition(list,state.output.Get(),state.output_state,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);transition(list,state.results.Get(),state.result_state,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  list->SetComputeRootSignature(state.root.Get());list->SetPipelineState(state.trace_pipeline.Get());
  list->SetComputeRootShaderResourceView(0,scene->tlas_gpu_address);
  ID3D12Resource *resources[]={state.samples.Get(),state.palette.Get(),static_cast<ID3D12Resource*>(scene->primitive_resource),static_cast<ID3D12Resource*>(scene->geometry_resource),static_cast<ID3D12Resource*>(scene->vertex_resource),static_cast<ID3D12Resource*>(scene->index_resource),static_cast<ID3D12Resource*>(materials->surface_resource),static_cast<ID3D12Resource*>(materials->descriptor_resource),static_cast<ID3D12Resource*>(materials->pixel_resource),state.parameters.Get(),state.color_maps.Get(),lighting?static_cast<ID3D12Resource*>(lighting->linear_resource):state.base_linear.Get(),lighting?static_cast<ID3D12Resource*>(lighting->packed_resource):state.base_rgba.Get()};
  for(unsigned i=0;i<13;i++){if(!resources[i])throw std::runtime_error("borrowed scene resource unavailable");list->SetComputeRootShaderResourceView(i+1,resources[i]->GetGPUVirtualAddress());}
  list->SetComputeRootUnorderedAccessView(14,state.output->GetGPUVirtualAddress());list->SetComputeRootUnorderedAccessView(15,state.results->GetGPUVirtualAddress());list->SetComputeRootConstantBufferView(16,state.constants_buffer->GetGPUVirtualAddress());
  list->Dispatch(40,25,1);GpuTiming_ReflectionTraceEnd(list);
  D3D12_RESOURCE_BARRIER b={};b.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;b.UAV.pResource=state.results.Get();list->ResourceBarrier(1,&b);
  list->SetPipelineState(state.compose_pipeline.Get());list->Dispatch(40,25,1);
  transition(list,state.output.Get(),state.output_state,D3D12_RESOURCE_STATE_COPY_SOURCE);transition(list,state.results.Get(),state.result_state,D3D12_RESOURCE_STATE_COPY_SOURCE);
  copy(list,static_cast<ID3D12Resource*>(scene_texture),false);
  if(state.readback){list->CopyBufferRegion(state.readback.Get(),0,state.results.Get(),0,count*sizeof(Result));list->CopyBufferRegion(state.readback.Get(),count*sizeof(Result),state.output.Get(),0,count*4);state.pending=true;}
  state.frame=GB_GetFrameInputs()->frame_id;state.tic=GB_GetFrameInputs()->game_tic;state.generation=scene->generation;state.revision=scene->material_revision;
  state.geometry_revision=scene->geometry_revision;state.shading_revision=scene->shading_revision;
  state.active=true;state.history_reset=state.visible>0;state.reason=state.visible?"active-map-reflection":"active-no-reflector-visible";return 1;
 }catch(const std::exception &e){state.failed=true;state.reason="reflection-frame-failed";std::fprintf(stderr,"RT reflection: %s; base scene retained\n",e.what());return 0;}
}
extern "C" int DxrReflection_PresentNearest(void *commands,void *backbuffer) {if(!state.active)return 0;copy(static_cast<ID3D12GraphicsCommandList*>(commands),static_cast<ID3D12Resource*>(backbuffer),true);return 1;}
extern "C" int DxrReflection_NeedsHistoryReset(void) {return state.active&&state.history_reset;}
extern "C" void DxrReflection_RecordTemporalEvaluation(int reset,int ngx,int fsr) {
 if(!state.active)return;
 state.temporal_reset=reset?1:0;state.ngx_evaluated=ngx?1:0;state.fsr_evaluated=fsr?1:0;
 state.frame_reset_reasons=GB_GetFrameInputs()->reset_reasons;state.frame_history_valid=GB_GetFrameInputs()->history_valid?1:0;
}
extern "C" void DxrReflection_Shutdown(void) {collect();DxrMap_RequestSceneFor(DXR_MAP_CONSUMER_REFLECTION,0);if(state.stats)std::fclose(state.stats);if(state.details)std::fclose(state.details);state=State();}
