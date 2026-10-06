/* Static-map domain geometry, independent of renderer and engine memory. GPLv2. */
#include "map_mesh.h"
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <limits>
namespace {
const size_t max_items = 1000000, max_polygon = 4096;
const double epsilon = 0.00001;
void require(bool condition, const char *reason) { if (!condition) throw std::runtime_error(reason); }
bool finite(double v) { return std::isfinite(v) && std::abs(v) <= 10000000; }
double cross(MapPoint a, MapPoint b, MapPoint p) { return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x); }
double area(const std::vector<MapPoint>& p) {
 double sum=0; for(size_t i=0;i<p.size();i++) {auto a=p[i],b=p[(i+1)%p.size()];sum+=a.x*b.y-a.y*b.x;} return sum/2;
}
void clean(std::vector<MapPoint>& p) {
 std::vector<MapPoint> q;
 for(auto v:p) if(q.empty() || std::hypot(v.x-q.back().x,v.y-q.back().y)>epsilon) q.push_back(v);
 if(q.size()>1 && std::hypot(q.front().x-q.back().x,q.front().y-q.back().y)<=epsilon) q.pop_back();
 bool removed=true;
 while(removed && q.size()>=3) {
  removed=false;
  for(size_t i=0;i<q.size();i++) {
   auto a=q[(i+q.size()-1)%q.size()],b=q[i],c=q[(i+1)%q.size()];
   if(std::abs(cross(a,c,b))<=epsilon*std::max(1.0,std::hypot(c.x-a.x,c.y-a.y))) {q.erase(q.begin()+i);removed=true;break;}
  }
 }
 p.swap(q);
}
std::vector<MapPoint> clip(const std::vector<MapPoint>& p, MapPoint a, MapPoint b, bool right) {
 std::vector<MapPoint> q; if(p.empty()) return q;
 double tolerance=epsilon*std::max(1.0,std::hypot(b.x-a.x,b.y-a.y));
 for(size_t i=0;i<p.size();i++) {
  auto v=p[i],w=p[(i+1)%p.size()]; double cv=cross(a,b,v)*(right?1:-1),cw=cross(a,b,w)*(right?1:-1);
  bool iv=cv<=tolerance,iw=cw<=tolerance;
  if(iv) q.push_back(v);
  if(iv!=iw) {
   double denominator=cv-cw; require(std::abs(denominator)>0,"invalid clipping intersection");
   double t=std::clamp(cv/denominator,0.0,1.0); q.push_back({v.x+(w.x-v.x)*t,v.y+(w.y-v.y)*t});
  }
 }
 require(q.size()<=max_polygon,"BSP polygon vertex limit exceeded"); clean(q); return q;
}
struct Builder {
 const MapInput& in;
 std::vector<MapMeshVertex> vertices;
 std::vector<uint32_t> indices, triangle_surfaces;
 std::vector<MapSurface> surfaces;
 std::vector<double> leaf_areas;
 std::vector<unsigned char> seen_nodes, seen_leaves;
 double minx,miny,maxx,maxy;
 uint32_t empty_leaves=0;
 explicit Builder(const MapInput& input):in(input) {}
 void validate() {
  require(in.points && in.point_count>=3 && in.point_count<=max_items,"invalid vertex count");
  require(in.sectors && in.sector_count && in.sector_count<=max_items,"invalid sector count");
  require(in.sides && in.side_count && in.side_count<=max_items,"invalid side count");
  require(in.lines && in.line_count && in.line_count<=max_items,"invalid line count");
  require(in.leaves && in.leaf_count && in.leaf_count<MAP_CHILD_LEAF,"invalid leaf count");
  require(in.segs && in.seg_count && in.seg_count<=max_items,"invalid seg count");
  require(in.node_count<MAP_CHILD_LEAF && (!in.node_count || in.nodes),"invalid node count");
  require(in.line_count*6+in.sector_count*2<=max_items,"surface slot limit exceeded");
  minx=maxx=in.points[0].x;miny=maxy=in.points[0].y;
  for(size_t i=0;i<in.point_count;i++) {auto p=in.points[i];require(finite(p.x)&&finite(p.y),"invalid map point");minx=std::min(minx,p.x);maxx=std::max(maxx,p.x);miny=std::min(miny,p.y);maxy=std::max(maxy,p.y);}
  minx-=64;maxx+=64;miny-=64;maxy+=64;
  for(size_t i=0;i<in.sector_count;i++) {auto s=in.sectors[i];require(finite(s.floor_height)&&finite(s.ceiling_height)&&s.floor_height<=s.ceiling_height,"invalid sector heights");}
  for(size_t i=0;i<in.side_count;i++) {auto s=in.sides[i];require(s.sector<in.sector_count&&finite(s.x_offset)&&finite(s.y_offset),"invalid sidedef");for(auto h:s.texture_height) require(finite(h)&&h>=0,"invalid texture height");}
  for(size_t i=0;i<in.line_count;i++) {auto l=in.lines[i];require(l.vertex[0]<in.point_count&&l.vertex[1]<in.point_count,"invalid linedef vertex");require(l.side[0]<in.side_count&&(l.side[1]==MAP_SIDE_NONE||l.side[1]<in.side_count),"invalid linedef side");}
  for(size_t i=0;i<in.seg_count;i++) {auto s=in.segs[i];require(s.vertex[0]<in.point_count&&s.vertex[1]<in.point_count&&s.line<in.line_count&&s.side<2,"invalid seg");require(in.lines[s.line].side[s.side]!=MAP_SIDE_NONE&&finite(s.offset),"invalid seg side/offset");}
  for(size_t i=0;i<in.leaf_count;i++) {auto l=in.leaves[i];require(l.sector<in.sector_count&&l.seg_count&&l.first_seg<=in.seg_count&&l.seg_count<=in.seg_count-l.first_seg,"invalid leaf range");}
  for(size_t i=0;i<in.node_count;i++) {auto n=in.nodes[i];require(finite(n.x)&&finite(n.y)&&finite(n.dx)&&finite(n.dy)&&(n.dx!=0||n.dy!=0),"invalid BSP partition");for(auto c:n.child) require(c&MAP_CHILD_LEAF?(c&~MAP_CHILD_LEAF)<in.leaf_count:c<in.node_count,"invalid BSP child");}
  surfaces.resize(in.line_count*6+in.sector_count*2);leaf_areas.resize(in.leaf_count);
  seen_nodes.resize(in.node_count);seen_leaves.resize(in.leaf_count);
 }
 void triangle(uint32_t a,uint32_t b,uint32_t c,uint32_t surface) {
  require(triangle_surfaces.size()<max_items,"triangle limit exceeded");
  const auto& va=vertices[a];const auto& vb=vertices[b];const auto& vc=vertices[c];
  double ab[3],ac[3];for(unsigned i=0;i<3;i++){ab[i]=static_cast<double>(vb.position[i])-va.position[i];ac[i]=static_cast<double>(vc.position[i])-va.position[i];}
  double nx=ab[1]*ac[2]-ab[2]*ac[1],ny=ab[2]*ac[0]-ab[0]*ac[2],nz=ab[0]*ac[1]-ab[1]*ac[0];
  require(nx*nx+ny*ny+nz*nz>0,"triangle degenerates after GPU float conversion");
  indices.insert(indices.end(),{a,b,c});triangle_surfaces.push_back(surface);surfaces[surface].triangle_count++;
 }
 uint32_t vertex(MapPoint p,double height,double u,double v) {
  require(vertices.size()<max_items*3,"vertex limit exceeded");
  MapMeshVertex out={{static_cast<float>(p.x),static_cast<float>(height),static_cast<float>(p.y)},{static_cast<float>(u),static_cast<float>(v)}};
  vertices.push_back(out);return static_cast<uint32_t>(vertices.size()-1);
 }
 MapSurface& setup(uint32_t id,unsigned kind,uint32_t line,uint32_t side,uint32_t sector,uint32_t back,uint32_t flags,int material) {
  auto& s=surfaces[id];if(!s.active) {s.active=1;s.kind=kind;s.line=line;s.side=side;s.sector=sector;s.back_sector=back;s.flags=flags;s.base_material=material;} return s;
 }
 void plane(uint32_t leaf,const std::vector<MapPoint>& p,bool ceiling) {
  auto sector=in.leaves[leaf].sector;auto source=in.sectors[sector];if(source.floor_height==source.ceiling_height||(ceiling?source.ceiling_sky:source.floor_sky)) return;
  auto id=MapMesh_PlaneSurface(in.line_count,sector,ceiling);
  auto& s=setup(id,ceiling?MAP_CEILING:MAP_FLOOR,MAP_SIDE_NONE,MAP_SIDE_NONE,sector,MAP_SIDE_NONE,0,ceiling?source.ceiling_material:source.floor_material);
  s.legacy_normal[1]=s.inward_normal[1]=ceiling?-1.f:1.f;
  uint32_t start=static_cast<uint32_t>(vertices.size());
  for(auto v:p) vertex(v,ceiling?source.ceiling_height:source.floor_height,v.x,-v.y);
  for(uint32_t i=1;i+1<p.size();i++) {if(ceiling) triangle(start,start+i+1,start+i,id);else triangle(start,start+i,start+i+1,id);}
 }
 void leaf(uint32_t id,std::vector<MapPoint> p) {
  require(!seen_leaves[id],"BSP leaf referenced more than once");seen_leaves[id]=1;
  auto leaf=in.leaves[id];
  for(uint32_t i=0;i<leaf.seg_count;i++) {
   auto seg=in.segs[leaf.first_seg+i];require(in.sides[in.lines[seg.line].side[seg.side]].sector==leaf.sector,"leaf seg sector mismatch");
   auto a=in.points[seg.vertex[0]],b=in.points[seg.vertex[1]];
   if(std::hypot(b.x-a.x,b.y-a.y)<=epsilon) continue;
   p=clip(p,a,b,true);
  }
  if(p.size()<3||std::abs(area(p))<=epsilon) {empty_leaves++;return;}
  require(area(p)<0,"invalid leaf winding");
  for(size_t i=0;i<p.size();i++) {
   auto a=p[i],b=p[(i+1)%p.size()],c=p[(i+2)%p.size()];require(cross(a,b,c)<=epsilon*std::max(1.0,std::hypot(b.x-a.x,b.y-a.y)),"nonconvex clipped leaf");
   require(std::abs(a.x-minx)>epsilon&&std::abs(a.x-maxx)>epsilon&&std::abs(a.y-miny)>epsilon&&std::abs(a.y-maxy)>epsilon,"unbounded leaf retains artificial map bounds");
  }
  leaf_areas[id]=-area(p);plane(id,p,false);plane(id,p,true);
 }
 void walk(uint32_t child,std::vector<MapPoint> p,unsigned depth) {
  require(depth<=512,"BSP depth limit exceeded");
  if(child&MAP_CHILD_LEAF) {leaf(child&~MAP_CHILD_LEAF,std::move(p));return;}
  require(!seen_nodes[child],"BSP cycle/shared node");seen_nodes[child]=1;
  auto n=in.nodes[child];MapPoint a={n.x,n.y},b={n.x+n.dx,n.y+n.dy};
  auto left=clip(p,a,b,false);walk(n.child[0],clip(p,a,b,true),depth+1);walk(n.child[1],std::move(left),depth+1);
 }
 void wall(size_t line,unsigned side,unsigned tier,double low,double high,double anchor) {
  if(high-low<=epsilon) return;
  auto l=in.lines[line];auto sd=in.sides[l.side[side]];auto a=in.points[l.vertex[side]],b=in.points[l.vertex[side^1]];double length=std::hypot(b.x-a.x,b.y-a.y);if(length<=epsilon)return;
  uint32_t back=l.side[side^1]==MAP_SIDE_NONE?MAP_SIDE_NONE:in.sides[l.side[side^1]].sector;
  auto id=MapMesh_WallSurface(line,side,tier);auto& s=setup(id,tier,static_cast<uint32_t>(line),l.side[side],sd.sector,back,l.flags,sd.material[tier]);
  s.x_offset=static_cast<float>(sd.x_offset);s.y_offset=static_cast<float>(sd.y_offset);s.v_anchor=static_cast<float>(anchor+sd.y_offset);
  s.texture_height=static_cast<float>(sd.texture_height[tier]);
  s.legacy_normal[0]=static_cast<float>(-(b.y-a.y)/length);s.legacy_normal[2]=static_cast<float>((b.x-a.x)/length);
  s.inward_normal[0]=-s.legacy_normal[0];s.inward_normal[2]=-s.legacy_normal[2];
  uint32_t va=vertex(a,low,sd.x_offset,anchor+sd.y_offset-low),vb=vertex(b,low,sd.x_offset+length,anchor+sd.y_offset-low);
  uint32_t vc=vertex(b,high,sd.x_offset+length,anchor+sd.y_offset-high),vd=vertex(a,high,sd.x_offset,anchor+sd.y_offset-high);
  triangle(va,vb,vc,id);triangle(va,vc,vd,id);
 }
 void walls() {
  for(size_t i=0;i<in.line_count;i++) for(unsigned side=0;side<2;side++) {
   auto l=in.lines[i];if(l.side[side]==MAP_SIDE_NONE)continue;auto sd=in.sides[l.side[side]];auto f=in.sectors[sd.sector];
   if(!(l.flags&MAP_FLAG_TWOSIDED)||l.side[side^1]==MAP_SIDE_NONE) {
    wall(i,side,MAP_WALL_MID,f.floor_height,f.ceiling_height,(l.flags&MAP_FLAG_DONTPEGBOTTOM)?f.floor_height+sd.texture_height[MAP_WALL_MID]:f.ceiling_height);
   } else {
    auto b=in.sectors[in.sides[l.side[side^1]].sector];
    if(!(f.ceiling_sky&&b.ceiling_sky)&&b.ceiling_height<f.ceiling_height)
     wall(i,side,MAP_WALL_UPPER,std::max(f.floor_height,b.ceiling_height),f.ceiling_height,(l.flags&MAP_FLAG_DONTPEGTOP)?f.ceiling_height:b.ceiling_height+sd.texture_height[MAP_WALL_UPPER]);
    if(b.floor_height>f.floor_height)
     wall(i,side,MAP_WALL_LOWER,f.floor_height,std::min(f.ceiling_height,b.floor_height),(l.flags&MAP_FLAG_DONTPEGBOTTOM)?f.ceiling_height:b.floor_height);
   }
  }
 }
 void run() {
  validate();walls();std::vector<MapPoint> bounds={{minx,miny},{minx,maxy},{maxx,maxy},{maxx,miny}};
  if(in.node_count)walk(static_cast<uint32_t>(in.node_count-1),std::move(bounds),0);else {require(in.leaf_count==1,"multiple leaves without BSP nodes");leaf(0,std::move(bounds));}
  for(auto visited:seen_leaves)require(visited,"BSP contains unreachable leaf");
  require(!triangle_surfaces.empty(),"map contains no opaque triangles");
 }
};
template<class T> T* copy(const std::vector<T>& data) {
 if(data.empty())return nullptr;
 auto result=static_cast<T*>(std::malloc(data.size()*sizeof(T)));
 if(!result)throw std::bad_alloc();
 std::memcpy(result,data.data(),data.size()*sizeof(T));return result;
}
}
extern "C" uint32_t MapMesh_WallSurface(size_t line,unsigned side,unsigned tier) {return static_cast<uint32_t>(line*6+side*3+tier);}
extern "C" uint32_t MapMesh_PlaneSurface(size_t lines,size_t sector,int ceiling) {return static_cast<uint32_t>(lines*6+sector*2+(ceiling?1:0));}
extern "C" void MapMesh_Free(MapMesh* mesh) {if(!mesh)return;std::free(mesh->vertices);std::free(mesh->indices);std::free(mesh->triangle_surfaces);std::free(mesh->surfaces);std::free(mesh->leaf_areas);std::memset(mesh,0,sizeof(*mesh));}
extern "C" int MapMesh_Build(const MapInput* input,MapMesh* output,char* diagnostic,size_t capacity) {
 if(diagnostic&&capacity)diagnostic[0]=0;
 if(!output)return 0;
 MapMesh temporary={};
 try {
  require(input,"missing map input");Builder builder(*input);builder.run();
  temporary.vertices=copy(builder.vertices);temporary.indices=copy(builder.indices);temporary.triangle_surfaces=copy(builder.triangle_surfaces);temporary.surfaces=copy(builder.surfaces);temporary.leaf_areas=copy(builder.leaf_areas);
  temporary.vertex_count=builder.vertices.size();temporary.triangle_count=builder.triangle_surfaces.size();temporary.surface_count=builder.surfaces.size();temporary.leaf_count=builder.leaf_areas.size();temporary.generation=input->generation;temporary.empty_leaves=builder.empty_leaves;
  *output=temporary;return 1;
 }catch(const std::exception& error) {MapMesh_Free(&temporary);if(diagnostic&&capacity)std::snprintf(diagnostic,capacity,"%s",error.what());return 0;}
}
