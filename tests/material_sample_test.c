/* Actual software column/span samples before COLORMAP; public G-buffer API. */
#include "doomdef.h"
#include "r_local.h"
#include "v_video.h"
#include "gbuffer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
byte *screens[5];
fixed_t viewx,viewy,viewz,projection=160*FRACUNIT,centeryfrac=84*FRACUNIT;
int centerx=160,centery=84,detailshift;
angle_t viewangle,xtoviewangle[SCREENWIDTH+1];
lighttable_t *fixedcolormap;
void I_Error(char *format,...) { (void)format;abort(); }
int main(void)
{
    byte source[4096]={0}, map[256]={0}, raw[768]={0}, base[768]={0}, gamma[256];
    const GB_MaterialSample *sample;
    int i;
    screens[0]=calloc(1,64000);assert(screens[0]);
    viewwidth=320;viewheight=168;R_InitBuffer(320,168);GB_Init();GB_BeginFrame();
    for(i=0;i<256;i++){map[i]=(byte)(255-i);gamma[i]=(byte)i;}
    source[0]=7;source[1]=0;
    GB_SetMaterialContext(GB_KIND_WALL,11);
    GB_SetColumn(20,256,1,0,0,GB_KIND_WALL);
    dc_source=source;dc_colormap=map;dc_x=20;dc_yl=30;dc_yh=31;
    dc_iscale=FRACUNIT;dc_texturemid=centeryfrac-30*FRACUNIT;
    R_DrawColumn();sample=GB_MaterialSamples();
    assert(sample[30*320+20].source_index==7 && sample[30*320+20].ambient_index==248);
    assert(sample[31*320+20].source_index==0 && sample[31*320+20].ambient_index==255);
    assert(sample[30*320+20].valid && sample[30*320+20].material_id==11);
    assert(screens[0][30*320+20]==248);
    GB_SetMaterialContext(GB_KIND_FLOOR,5);GB_WriteSpan(100,40,41,300,0,1,0);
    ds_source=source;ds_colormap=map;ds_y=100;ds_x1=40;ds_x2=41;
    ds_xfrac=ds_yfrac=ds_ystep=0;ds_xstep=FRACUNIT;
    R_DrawSpan();assert(sample[100*320+40].source_index==7 && sample[100*320+41].source_index==0);
    assert(sample[100*320+40].kind==GB_KIND_FLOOR && sample[100*320+40].material_id==5);
    base[7*3]=23;raw[7*3]=99;gamma[99]=123;
    GB_SetBasePaletteRGB(base);GB_SetRawPaletteRGB(raw,gamma);
    assert(GB_BasePaletteRGB()[7*3]==23 && GB_RawPaletteRGB()[7*3]==99 && GB_GammaLUT()[99]==123);
    GB_CaptureScene(screens[0]);GB_SetMaterialContext(GB_KIND_SPRITE,77);
    GB_RecordMaterialSample(30*320+20,18,19);assert(sample[30*320+20].source_index==7);
    GB_BeginFrame();fixedcolormap=map;GB_SetMaterialContext(GB_KIND_WALL,11);
    R_DrawColumn();assert(!sample[30*320+20].valid);fixedcolormap=NULL;
    free(screens[0]);puts("actual column/span raw albedo, ambient, palette and overlay isolation passed");return 0;
}
