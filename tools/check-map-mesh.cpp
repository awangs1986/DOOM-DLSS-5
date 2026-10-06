/* Public map-domain acceptance fixtures; no renderer or private-function hooks. GPLv2. */
#include "map_mesh.h"
#include <vector>
#include <stdexcept>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <limits>
struct Fixture {
 std::vector<MapPoint> points;std::vector<MapSector> sectors;std::vector<MapSide> sides;
 std::vector<MapLine> lines;std::vector<MapSeg> segs;std::vector<MapLeaf> leaves;std::vector<MapNode> nodes;
 Fixture(){sectors.push_back({0,8,1,2,0,0});}
 uint32_t point(double x,double y){points.push_back({x,y});return static_cast<uint32_t>(points.size()-1);}
 uint32_t side(unsigned sector){MapSide s={};s.sector=sector;s.material[0]=10;s.material[1]=11;s.material[2]=12;s.texture_height[0]=64;s.texture_height[1]=32;s.texture_height[2]=16;sides.push_back(s);return static_cast<uint32_t>(sides.size()-1);}
 uint32_t line(uint32_t a,uint32_t b,unsigned sector=0){MapLine l={{a,b},{side(sector),MAP_SIDE_NONE},0};lines.push_back(l);return static_cast<uint32_t>(lines.size()-1);}
 void seg(unsigned line,unsigned side=0,int a=-1,int b=-1){auto l=lines[line];segs.push_back({{a<0?l.vertex[side]:static_cast<uint32_t>(a),b<0?l.vertex[side^1]:static_cast<uint32_t>(b)},line,side,0});}
 void leaf(unsigned first,unsigned count,unsigned sector=0){leaves.push_back({first,count,sector});}
 MapInput input(){return {points.data(),points.size(),sectors.data(),sectors.size(),sides.data(),sides.size(),lines.data(),lines.size(),segs.data(),segs.size(),leaves.data(),leaves.size(),nodes.data(),nodes.size(),17};}
};
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
MapMesh build(Fixture& f){MapMesh mesh={};auto in=f.input();char diagnostic[256];check(MapMesh_Build(&in,&mesh,diagnostic,sizeof(diagnostic))!=0,diagnostic);return mesh;}
double area(const MapMesh& mesh,unsigned kind){double result=0;for(size_t i=0;i<mesh.triangle_count;i++)if(mesh.surfaces[mesh.triangle_surfaces[i]].kind==kind){auto a=mesh.vertices[mesh.indices[i*3]].position,b=mesh.vertices[mesh.indices[i*3+1]].position,c=mesh.vertices[mesh.indices[i*3+2]].position;double ny=(b[2]-a[2])*(c[0]-a[0])-(b[0]-a[0])*(c[2]-a[2]);check(kind==MAP_FLOOR?ny>0:ny<0,"plane winding/normal mismatch");result+=std::abs(ny)/2;}return result;}
bool covered(const MapMesh& mesh,double x,double y,unsigned kind){for(size_t i=0;i<mesh.triangle_count;i++)if(mesh.surfaces[mesh.triangle_surfaces[i]].kind==kind){auto a=mesh.vertices[mesh.indices[i*3]].position,b=mesh.vertices[mesh.indices[i*3+1]].position,c=mesh.vertices[mesh.indices[i*3+2]].position;double d1=(b[0]-a[0])*(y-a[2])-(b[2]-a[2])*(x-a[0]),d2=(c[0]-b[0])*(y-b[2])-(c[2]-b[2])*(x-b[0]),d3=(a[0]-c[0])*(y-c[2])-(a[2]-c[2])*(x-c[0]);if((d1>=-1e-6&&d2>=-1e-6&&d3>=-1e-6)||(d1<=1e-6&&d2<=1e-6&&d3<=1e-6))return true;}return false;}
Fixture rectangle(){Fixture f;for(auto p:std::vector<MapPoint>{{0,0},{0,4},{8,4},{8,0}})f.point(p.x,p.y);for(unsigned i=0;i<4;i++){f.line(i,(i+1)%4);f.seg(i);}f.leaf(0,4);return f;}
Fixture adjacent(){Fixture f;f.sectors.push_back({0,8,3,4,0,0});for(auto p:std::vector<MapPoint>{{0,0},{0,4},{4,4},{8,4},{8,0},{4,0}})f.point(p.x,p.y);
 f.line(0,1,0);f.line(1,2,0);f.line(2,3,1);f.line(3,4,1);f.line(4,5,1);f.line(5,0,0);
 auto shared=f.line(5,2,1);f.lines[shared].side[1]=f.side(0);f.lines[shared].flags=MAP_FLAG_TWOSIDED;
 f.seg(0);f.seg(1);f.seg(shared,1);f.seg(5);f.leaf(0,4,0);
 f.seg(shared);f.seg(2);f.seg(3);f.seg(4);f.leaf(4,4,1);
 f.nodes.push_back({4,0,0,1,{MAP_CHILD_LEAF|1,MAP_CHILD_LEAF|0}});return f;
}
int main(){try {
 {auto f=rectangle();auto m=build(f);check(area(m,MAP_FLOOR)==32&&area(m,MAP_CEILING)==32,"rectangle area");auto again=build(f);check(m.triangle_count==again.triangle_count&&!std::memcmp(m.triangle_surfaces,again.triangle_surfaces,m.triangle_count*4)&&!std::memcmp(m.vertices,again.vertices,m.vertex_count*sizeof(MapMeshVertex)),"stable identity/UV rebuild");check(m.generation==17,"generation copy");MapMesh_Free(&again);MapMesh_Free(&m);std::puts("PASS rectangle area, winding, owned stable rebuild");}
 {Fixture f;for(auto p:std::vector<MapPoint>{{0,0},{0,8},{4,8},{4,4},{8,4},{8,0},{4,0}})f.point(p.x,p.y);for(unsigned i=0;i<6;i++)f.line(i,(i+1)%6);
  f.seg(3);f.seg(4);f.seg(5,0,5,6);f.leaf(0,3);f.seg(0);f.seg(1);f.seg(2);f.seg(5,0,6,0);f.leaf(3,4);f.nodes.push_back({4,0,0,1,{MAP_CHILD_LEAF|0,MAP_CHILD_LEAF|1}});
  auto m=build(f);check(area(m,MAP_FLOOR)==48&&!covered(m,6,6,MAP_FLOOR)&&covered(m,2,6,MAP_FLOOR)&&covered(m,6,2,MAP_FLOOR),"concave L/open raw seg closure");MapMesh_Free(&m);std::puts("PASS concave L with open leaf segs closed by BSP");}
 {Fixture f;for(auto p:std::vector<MapPoint>{{0,0},{0,8},{8,8},{8,0},{2,2},{6,2},{6,6},{2,6}})f.point(p.x,p.y);for(unsigned i=0;i<4;i++)f.line(i,(i+1)%4);for(unsigned i=0;i<4;i++)f.line(i+4,(i+1)%4+4);
  f.seg(0);f.seg(1);f.seg(3);f.seg(7);f.leaf(0,4); // left
  f.seg(1);f.seg(2);f.seg(3);f.seg(5);f.leaf(4,4); // right
  f.seg(1);f.seg(6);f.leaf(8,2); // top
  f.seg(3);f.seg(4);f.leaf(10,2); // bottom
  f.seg(7);f.leaf(12,1); // void center clips to zero width
  f.nodes.push_back({0,6,1,0,{MAP_CHILD_LEAF|4,MAP_CHILD_LEAF|2}});
  f.nodes.push_back({0,2,1,0,{MAP_CHILD_LEAF|3,0}});
  f.nodes.push_back({6,0,0,1,{MAP_CHILD_LEAF|1,1}});
  f.nodes.push_back({2,0,0,1,{2,MAP_CHILD_LEAF|0}});
  auto m=build(f);check(area(m,MAP_FLOOR)==48&&!covered(m,4,4,MAP_FLOOR)&&covered(m,1,4,MAP_FLOOR)&&covered(m,7,4,MAP_FLOOR)&&covered(m,4,1,MAP_FLOOR)&&covered(m,4,7,MAP_FLOOR),"hole must remain empty");check(m.empty_leaves==1,"void leaf recorded");MapMesh_Free(&m);std::puts("PASS hole ring without filling interior void");}
 {auto f=adjacent();auto m=build(f);check(area(m,MAP_FLOOR)==32,"adjacent combined area");for(unsigned s=0;s<2;s++)for(unsigned t=0;t<3;t++)check(!m.surfaces[MapMesh_WallSurface(6,s,t)].active,"equal-height portal is not wall");MapMesh_Free(&m);
  f.sectors[1].floor_height=2;f.sectors[1].ceiling_height=6;auto side=f.lines[6].side[1];f.sides[side].x_offset=5;f.sides[side].y_offset=3;
  m=build(f);auto lower=m.surfaces[MapMesh_WallSurface(6,1,MAP_WALL_LOWER)],upper=m.surfaces[MapMesh_WallSurface(6,1,MAP_WALL_UPPER)];check(lower.triangle_count==2&&upper.triangle_count==2&&lower.v_anchor==5&&upper.v_anchor==41&&upper.texture_height==32&&lower.texture_height==16,"height steps and default pegging");check(lower.side==side&&lower.sector==0&&lower.back_sector==1&&lower.base_material==12&&lower.legacy_normal[0]==1&&lower.inward_normal[0]==-1,"stable side/material/normal metadata");MapMesh_Free(&m);
  f.lines[6].flags|=MAP_FLAG_DONTPEGBOTTOM|MAP_FLAG_DONTPEGTOP;m=build(f);check(m.surfaces[MapMesh_WallSurface(6,1,MAP_WALL_LOWER)].v_anchor==11&&m.surfaces[MapMesh_WallSurface(6,1,MAP_WALL_UPPER)].v_anchor==11,"flag pegging anchors");MapMesh_Free(&m);
  f.sectors[0].ceiling_sky=f.sectors[1].ceiling_sky=1;m=build(f);check(!m.surfaces[MapMesh_WallSurface(6,1,MAP_WALL_UPPER)].active&&area(m,MAP_CEILING)==0,"sky portal/planes do not occlude");MapMesh_Free(&m);std::puts("PASS adjacency portal/height steps/UV pegging/sky/normal contracts");}
 {auto f=rectangle();f.segs.push_back({{0,0},0,0,0});f.leaves[0].seg_count++;auto m=build(f);check(area(m,MAP_FLOOR)==32,"zero seg ignored without corrupting area");MapMesh_Free(&m);
  auto input=f.input();f.segs[0].vertex[0]=999;char error[128];check(!MapMesh_Build(&input,&m,error,sizeof(error))&&m.vertices==nullptr&&error[0],"bad index rejection");f.segs[0].vertex[0]=0;
  f.nodes.push_back({4,0,0,1,{0,MAP_CHILD_LEAF}});input=f.input();check(!MapMesh_Build(&input,&m,error,sizeof(error))&&m.vertices==nullptr,"BSP cycle rejection");f.nodes[0].child[0]=MAP_CHILD_LEAF|77;input=f.input();check(!MapMesh_Build(&input,&m,error,sizeof(error)),"invalid child rejection");f.nodes.clear();f.leaves[0].first_seg=999;input=f.input();check(!MapMesh_Build(&input,&m,error,sizeof(error)),"malformed leaf range rejection");f.leaves[0].first_seg=0;f.points[0].x=std::numeric_limits<double>::quiet_NaN();input=f.input();check(!MapMesh_Build(&input,&m,error,sizeof(error)),"nonfinite point rejection");std::puts("PASS zero edge / malformed range / cyclic BSP / invalid child / nonfinite guards");}
 {auto f=rectangle();for(auto& p:f.points){p.x=9999999.0+p.x*0.005;p.y*=0.005;}auto in=f.input();MapMesh m={};char error[128];check(!MapMesh_Build(&in,&m,error,sizeof(error))&&std::strstr(error,"float conversion"),"GPU float collapse rejection");std::puts("PASS GPU float conversion rejects collapsed triangles");}
 return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
