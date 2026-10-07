/* Copied final-world revision contract; no renderer/GPU simulation. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "r_local.h"
#include "r_state.h"
#include "r_sky.h"
#include "map_source.h"
int numvertexes,numsectors,numsides,numlines,numsegs,numsubsectors,numnodes;
vertex_t *vertexes;sector_t *sectors;side_t *sides;line_t *lines;seg_t *segs;subsector_t *subsectors;node_t *nodes;
fixed_t *textureheight;int *texturetranslation;int skyflatnum;
int main(void)
{
 sector_t sector={0};side_t side={0};fixed_t heights[2]={64*FRACUNIT,64*FRACUNIT};int translation[2]={0,1};
 MapWorldRevision initial,changed;MapMesh mesh={0};MapSurface surface={0};MapMeshVertex vertices[3]={0};uint32_t indices[3]={0,1,2},triangle=0;
 numsectors=numsides=1;sectors=&sector;sides=&side;textureheight=heights;texturetranslation=translation;skyflatnum=99;
 sector.floorheight=0;sector.ceilingheight=128*FRACUNIT;sector.lightlevel=160;
 assert(MapSource_Survey(&initial));
 sector.lightlevel=80;assert(MapSource_Survey(&changed));
 assert(initial.geometry==changed.geometry&&initial.material==changed.material&&initial.mapping==changed.mapping&&initial.lighting!=changed.lighting);
 mesh.surfaces=&surface;mesh.surface_count=1;surface.active=1;surface.lightlevel=160;
 MapSource_UpdateLightLevels(&mesh);assert(surface.lightlevel==80);puts("PASS light-only change copies current light without geometry/material invalidation");
 initial=changed;side.textureoffset=3*FRACUNIT;side.rowoffset=7*FRACUNIT;assert(MapSource_Survey(&changed));
 assert(initial.geometry==changed.geometry&&initial.material==changed.material&&initial.lighting==changed.lighting&&initial.mapping!=changed.mapping);
 mesh.vertices=vertices;mesh.indices=indices;mesh.triangle_surfaces=&triangle;mesh.vertex_count=3;mesh.triangle_count=1;surface.kind=MAP_WALL_MID;surface.side=0;surface.v_anchor=128;
 assert(MapSource_UpdateMapping(&mesh));assert(surface.x_offset==3&&surface.y_offset==7&&surface.v_anchor==135);
 for(unsigned i=0;i<3;i++)assert(vertices[i].uv[0]==3&&vertices[i].uv[1]==7);
 assert(MapSource_UpdateMapping(&mesh));for(unsigned i=0;i<3;i++)assert(vertices[i].uv[0]==3&&vertices[i].uv[1]==7);
 puts("PASS scrolling UV refresh is idempotent and does not invalidate geometry/material references");
 initial=changed;sector.floorheight=-8*FRACUNIT;assert(MapSource_Survey(&changed));assert(initial.geometry!=changed.geometry&&initial.material==changed.material);
 puts("PASS final sector height change invalidates geometry");
 initial=changed;sector.floorpic=4;assert(MapSource_Survey(&changed));assert(initial.material!=changed.material&&initial.geometry==changed.geometry);
 puts("PASS same-generation base material changes referenced-material revision");
 initial=changed;sector.ceilingpic=skyflatnum;assert(MapSource_Survey(&changed));assert(initial.geometry!=changed.geometry&&initial.material!=changed.material);
 puts("PASS sky topology classification changes geometry and material references");
 initial=changed;side.midtexture=1;assert(MapSource_Survey(&changed));
 assert(initial.geometry!=changed.geometry&&initial.material!=changed.material&&initial.mapping==changed.mapping);
 puts("PASS equal-height MID activation invalidates geometry and material references");
 return 0;
}
