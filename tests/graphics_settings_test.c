/* Exercise the public settings seam with real files, no renderer simulation. */
#include "graphics_settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void write(const char *path,const char *data){FILE *f=fopen(path,"wb");assert(f);assert(fwrite(data,1,strlen(data),f)==strlen(data));assert(!fclose(f));}
int main(int argc,char **argv){
 char sidecar[4096],before[8192],after[8192];unsigned p=99,v=99;size_t a,b;FILE *f;assert(argc==3);
 snprintf(sidecar,sizeof(sidecar),"%s.graphics.cfg",argv[2]);
 if(!strcmp(argv[1],"parse")){
  const char *valid="schema=1\r\nrt=1\r\nsr=0\r\nnr=1\r\n";
  assert(Graphics_Parse(valid,strlen(valid),&p,&v)&&p==7&&v==5);
  const char *invalid[]={"schema=2\nrt=1\n","schema=1\nrt=1\nrt=0\n","schema=1\nsr=2\n","schema=1\nnrx=1\n","rt=1\n"};
  for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++){p=v=99;assert(!Graphics_Parse(invalid[i],strlen(invalid[i]),&p,&v)&&p==99&&v==99);}
  memset(before,'x',sizeof(before));assert(!Graphics_Parse(before,sizeof(before),&p,&v));
  assert(!Graphics_Parse("schema=1\0nr=1",14,&p,&v));
  puts("PASS bounded transactional preference parser");return 0;
 }
 if(!strcmp(argv[1],"basic")){
  write(sidecar,"schema=1\nrt=1\nsr=0\n");
  Graphics_Init(argv[2],2,2,2);assert(Graphics_Get(0).requested&&Graphics_Get(1).requested&&!Graphics_Get(2).requested);
  assert(Graphics_TakePending()==7&&Graphics_TakePending()==0);
  Graphics_SetRequested(2,1);assert(Graphics_TakePending()==4);assert(Graphics_Get(2).epoch==1);
  Graphics_SetActual(2,GRAPHICS_UNVERIFIED,"graphics.reason.execution_unverified");assert(Graphics_Get(2).requested);
  Graphics_Init(argv[2],0,0,0);assert(Graphics_Get(0).requested&&!Graphics_Get(1).requested&&Graphics_Get(2).requested);
  Graphics_SetRequested(0,0);Graphics_Init(argv[2],0,0,0);assert(!Graphics_Get(0).requested&&!Graphics_Get(1).requested&&Graphics_Get(2).requested);
  puts("PASS independent requests, safe pending, saved/CLI precedence, only edited key persisted");return 0;
 }
 if(!strcmp(argv[1],"defaults")){
  Graphics_Init(argv[2],2,0,0);assert(Graphics_Get(1).requested);Graphics_SetRequested(2,1);
  Graphics_Init(argv[2],0,0,0);assert(!Graphics_Get(1).requested&&Graphics_Get(2).requested);
  Graphics_Init(argv[2],2,0,0);assert(Graphics_Get(1).requested&&Graphics_Get(2).requested);
  puts("PASS missing preference keys retain each mode default");return 0;
 }
 if(!strcmp(argv[1],"oversize")){
  memset(before,'x',6000);f=fopen(sidecar,"wb");assert(f);assert(fwrite(before,1,6000,f)==6000);fclose(f);
  Graphics_Init(argv[2],0,0,0);assert(!strcmp(Graphics_PreferenceReason(),"graphics.reason.prefs_invalid"));
  Graphics_SetRequested(0,1);assert(Graphics_Get(0).requested&&!strcmp(Graphics_PreferenceReason(),"graphics.reason.save_failed"));
  f=fopen(sidecar,"rb");assert(f);b=fread(after,1,sizeof(after),f);fclose(f);assert(b==6000&&!memcmp(before,after,b));
  puts("PASS oversized old profile preserved, request still applies, save failure visible");return 0;
 }
 if(!strcmp(argv[1],"refuse")){
  Graphics_Init(argv[2],0,0,0);Graphics_SetRequested(0,1);
  assert(Graphics_Get(0).requested&&!strcmp(Graphics_PreferenceReason(),"graphics.reason.save_failed"));
  puts("PASS unreadable/directory old profile is not replaced");return 0;
 }
 if(!strcmp(argv[1],"reload")){
  Graphics_Init(argv[2],0,0,0);assert(Graphics_Get(0).requested&&!Graphics_Get(1).requested&&Graphics_Get(2).requested);
  puts("PASS persisted profile survives a separate process");return 0;
 }
 (void)a;return 2;
}
