/* Copy engine data into the portable map domain builder. GPLv2. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "doomdef.h"
#include "doomstat.h"
#include "r_local.h"
#include "r_state.h"
#include "r_sky.h"
#include "r_plane.h"
#include "map_source.h"
int MapSource_Capture(uint64_t generation, MapMesh *mesh, char *diagnostic, size_t capacity)
{
 MapInput input; MapPoint *points=NULL;MapSector *sector_data=NULL;MapSide *side_data=NULL;
 MapLine *line_data=NULL;MapSeg *seg_data=NULL;MapLeaf *leaf_data=NULL;MapNode *node_data=NULL;
 int i,j,result=0;
 memset(&input,0,sizeof(input));
 if(numvertexes<3||numvertexes>1000000||numsectors<1||numsectors>1000000||
    numsides<1||numsides>1000000||numlines<1||numlines>1000000||numsegs<1||numsegs>1000000||
    numsubsectors<1||numsubsectors>=32768||numnodes<0||numnodes>=32768) {
  if(diagnostic&&capacity)snprintf(diagnostic,capacity,"engine map counts outside supported limits");return 0;
 }
 points=calloc(numvertexes,sizeof(*points));sector_data=calloc(numsectors,sizeof(*sector_data));
 side_data=calloc(numsides,sizeof(*side_data));line_data=calloc(numlines,sizeof(*line_data));
 seg_data=calloc(numsegs,sizeof(*seg_data));leaf_data=calloc(numsubsectors,sizeof(*leaf_data));
 if(numnodes)node_data=calloc(numnodes,sizeof(*node_data));
 if(!points||!sector_data||!side_data||!line_data||!seg_data||!leaf_data||(numnodes&&!node_data)) {
  if(diagnostic&&capacity)snprintf(diagnostic,capacity,"map snapshot allocation failed");goto done;
 }
 for(i=0;i<numvertexes;i++){points[i].x=vertexes[i].x/65536.0;points[i].y=vertexes[i].y/65536.0;}
 for(i=0;i<numsectors;i++){
  sector_data[i].floor_height=sectors[i].floorheight/65536.0;sector_data[i].ceiling_height=sectors[i].ceilingheight/65536.0;
  sector_data[i].floor_material=sectors[i].floorpic;sector_data[i].ceiling_material=sectors[i].ceilingpic;
  sector_data[i].lightlevel=sectors[i].lightlevel;
  sector_data[i].floor_sky=sectors[i].floorpic==skyflatnum;sector_data[i].ceiling_sky=sectors[i].ceilingpic==skyflatnum;
 }
 for(i=0;i<numsides;i++){
  side_data[i].sector=(uint32_t)(sides[i].sector-sectors);side_data[i].x_offset=sides[i].textureoffset/65536.0;side_data[i].y_offset=sides[i].rowoffset/65536.0;
  side_data[i].material[MAP_WALL_MID]=sides[i].midtexture;side_data[i].material[MAP_WALL_UPPER]=sides[i].toptexture;side_data[i].material[MAP_WALL_LOWER]=sides[i].bottomtexture;
  for(j=0;j<3;j++) side_data[i].texture_height[j]=textureheight[texturetranslation[side_data[i].material[j]]]/65536.0;
 }
 for(i=0;i<numlines;i++){
  line_data[i].vertex[0]=(uint32_t)(lines[i].v1-vertexes);line_data[i].vertex[1]=(uint32_t)(lines[i].v2-vertexes);
  line_data[i].side[0]=lines[i].sidenum[0]<0?MAP_SIDE_NONE:(uint32_t)lines[i].sidenum[0];
  line_data[i].side[1]=lines[i].sidenum[1]<0?MAP_SIDE_NONE:(uint32_t)lines[i].sidenum[1];line_data[i].flags=lines[i].flags;
 }
 for(i=0;i<numsegs;i++){
  seg_data[i].vertex[0]=(uint32_t)(segs[i].v1-vertexes);seg_data[i].vertex[1]=(uint32_t)(segs[i].v2-vertexes);seg_data[i].line=(uint32_t)(segs[i].linedef-lines);
  seg_data[i].side=segs[i].sidedef==&sides[segs[i].linedef->sidenum[0]]?0:1;seg_data[i].offset=segs[i].offset/65536.0;
 }
 for(i=0;i<numsubsectors;i++){
  leaf_data[i].first_seg=(unsigned short)subsectors[i].firstline;leaf_data[i].seg_count=(unsigned short)subsectors[i].numlines;leaf_data[i].sector=(uint32_t)(subsectors[i].sector-sectors);
 }
 for(i=0;i<numnodes;i++){
  node_data[i].x=nodes[i].x/65536.0;node_data[i].y=nodes[i].y/65536.0;node_data[i].dx=nodes[i].dx/65536.0;node_data[i].dy=nodes[i].dy/65536.0;
  node_data[i].child[0]=nodes[i].children[0];node_data[i].child[1]=nodes[i].children[1];
 }
 input.points=points;input.point_count=numvertexes;input.sectors=sector_data;input.sector_count=numsectors;
 input.sides=side_data;input.side_count=numsides;input.lines=line_data;input.line_count=numlines;
 input.segs=seg_data;input.seg_count=numsegs;input.leaves=leaf_data;input.leaf_count=numsubsectors;input.nodes=node_data;input.node_count=numnodes;input.generation=generation;
 result=MapMesh_Build(&input,mesh,diagnostic,capacity);
done:
 free(points);free(sector_data);free(side_data);free(line_data);free(seg_data);free(leaf_data);free(node_data);return result;
}

/* FNV values are comparison tokens within one loaded generation, not security hashes. */
static void revision_word(uint64_t *hash, uint32_t value)
{
 unsigned byte;
 for(byte=0;byte<4;byte++){*hash^=(value>>(byte*8))&255u;*hash*=UINT64_C(1099511628211);}
}
int MapSource_Survey(MapWorldRevision *r)
{
 int i,j;
 if(!r||numsectors<1||numsectors>1000000||numsides<1||numsides>1000000)return 0;
 memset(r,0,sizeof(*r));r->sectors=numsectors;r->sides=numsides;
 r->geometry=r->material=r->lighting=UINT64_C(14695981039346656037);
 for(i=0;i<numsectors;i++){
  revision_word(&r->geometry,sectors[i].floorheight);revision_word(&r->geometry,sectors[i].ceilingheight);
  revision_word(&r->geometry,sectors[i].floorpic==skyflatnum);revision_word(&r->geometry,sectors[i].ceilingpic==skyflatnum);
  revision_word(&r->material,sectors[i].floorpic);revision_word(&r->material,sectors[i].ceilingpic);
  revision_word(&r->lighting,sectors[i].lightlevel);
 }
 for(i=0;i<numsides;i++){
  int ids[3]={sides[i].midtexture,sides[i].toptexture,sides[i].bottomtexture};
  revision_word(&r->material,sides[i].textureoffset);revision_word(&r->material,sides[i].rowoffset);
  for(j=0;j<3;j++){
   revision_word(&r->material,ids[j]);
   /* Animation may alter masked span/pegging through resolved texture height. */
   revision_word(&r->geometry,textureheight[texturetranslation[ids[j]]]);
  }
 }
 return 1;
}
void MapSource_UpdateLightLevels(MapMesh *mesh)
{
 size_t i;if(!mesh)return;
 for(i=0;i<mesh->surface_count;i++)if(mesh->surfaces[i].active&&mesh->surfaces[i].sector<(unsigned)numsectors)
  mesh->surfaces[i].lightlevel=sectors[mesh->surfaces[i].sector].lightlevel;
}
int MapSource_GetSectorHeights(unsigned sector,double *floor,double *ceiling)
{
 if(sector>=(unsigned)numsectors)return 0;
 *floor=sectors[sector].floorheight/65536.0;*ceiling=sectors[sector].ceilingheight/65536.0;return 1;
}
