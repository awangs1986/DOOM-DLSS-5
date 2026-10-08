/* Actual masked wall post coverage vs sprite normals; public G-buffer API. */
#include "doomdef.h"
#include "r_local.h"
#include "v_video.h"
#include "gbuffer.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
byte *screens[5];
fixed_t viewx,viewy,viewz,projection=160*FRACUNIT,centeryfrac=84*FRACUNIT;
int centerx=160,centery=84,detailshift;
angle_t viewangle,xtoviewangle[SCREENWIDTH+1];
fixed_t *finecosine=&finesine[FINEANGLES/4];
lighttable_t *fixedcolormap;
void (*colfunc)(void)=R_DrawColumn;
void I_Error(char *fmt,...) {(void)fmt;abort();}
int main(void) {
 byte post[7]={0,2,0,7,0,0,255},map[256];short bottom[320],top[320];int i;
 const GB_MaterialSample *samples;
 screens[0]=calloc(1,64000);assert(screens[0]);
 viewwidth=320;viewheight=168;R_InitBuffer(320,168);GB_Init();GB_BeginFrame();
 for(i=0;i<256;i++)map[i]=(byte)(255-i);
 for(i=0;i<320;i++){top[i]=-1;bottom[i]=168;}
 mfloorclip=bottom;mceilingclip=top;spryscale=FRACUNIT;sprtopscreen=30*FRACUNIT;
 dc_x=20;dc_colormap=map;dc_iscale=FRACUNIT;dc_texturemid=centeryfrac-30*FRACUNIT;
 GB_SetMaterialContext(GB_KIND_WALL,42);GB_SetColumn(20,256,1,0,0,GB_KIND_WALL);
 R_DrawMaskedColumn((column_t*)post);samples=GB_MaterialSamples();
 assert(GB_SurfaceKind()[30*320+20]==GB_KIND_WALL&&GB_Depth()[30*320+20]==256);
 assert(samples[30*320+20].valid&&samples[30*320+20].material_id==42&&samples[30*320+20].source_index==7);
 assert(samples[31*320+20].source_index==0&&samples[31*320+20].valid);
 assert(!samples[32*320+20].valid); /* post gap, not rectangle coverage */
 assert(GB_NormalRGBA()[(30*320+20)*4]==255);
 dc_x=21;GB_SetMaterialContext(GB_KIND_SPRITE,99);R_DrawMaskedColumn((column_t*)post);
 assert(GB_SurfaceKind()[30*320+21]==GB_KIND_SPRITE&&!samples[30*320+21].valid);
 GB_CaptureScene(screens[0]);dc_x=20;GB_SetMaterialContext(GB_KIND_SPRITE,99);R_DrawMaskedColumn((column_t*)post);
 assert(samples[30*320+20].material_id==42&&GB_SurfaceKind()[30*320+20]==GB_KIND_WALL);
 free(screens[0]);puts("actual masked wall post normals/opaque zero/gaps/sprite/overlay isolation passed");return 0;
}
