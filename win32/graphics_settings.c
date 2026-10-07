/* GPLv2. Independent menu requests; bounded profile sidecar, no argv mutation. */
#include "graphics_settings.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif
#define PREF_LIMIT 4096
static GraphicsStatus states[GRAPHICS_COUNT];
static unsigned saved_presence, saved_values, pending, sequence;
static char path[4096];
static const char *preference_reason="graphics.reason.none";
int Graphics_Parse(const void *data,size_t length,unsigned *presence,unsigned *values)
{
 char copy[PREF_LIMIT+1],*line; unsigned found=0, bits=0; int schema=0;
 if(!data||!presence||!values||!length||length>PREF_LIMIT||memchr(data,0,length))return 0;
 memcpy(copy,data,length);copy[length]=0;line=copy;
 while(*line){
  char *end=strchr(line,'\n'),*equal,*tail; int index=-1;
  if(end)*end++=0;
  tail=line+strlen(line);if(tail>line&&tail[-1]=='\r')*--tail=0;
  while(*line==' '||*line=='\t')line++;
  while(tail>line&&(tail[-1]==' '||tail[-1]=='\t'))*--tail=0;
  if(*line&&*line!='#'){
   equal=strchr(line,'=');if(!equal||strchr(equal+1,'='))return 0;*equal++=0;
   if(!strcmp(line,"schema")){if(schema||strcmp(equal,"1"))return 0;schema=1;}
   else {
    if(!strcmp(line,"rt"))index=GRAPHICS_RT;
    if(!strcmp(line,"sr"))index=GRAPHICS_SR;
    if(!strcmp(line,"nr"))index=GRAPHICS_NR;
    if(index<0||(found&(1u<<index))||(strcmp(equal,"0")&&strcmp(equal,"1")))return 0;
    found|=1u<<index;if(*equal=='1')bits|=1u<<index;
   }
  }
  if(!end)break;
  line=end;
 }
 if(!schema)return 0;
 *presence=found;*values=bits;return 1;
}
void Graphics_Init(const char *profile,unsigned defaults,unsigned cli_mask,unsigned cli_values)
{
 char absolute[4096],bytes[PREF_LIMIT+1];FILE *file;size_t length;unsigned effective;int i;
 saved_presence=saved_values=0;pending=7;path[0]=0;
 preference_reason="graphics.reason.none";
#ifdef _WIN32
 {DWORD n=profile?GetFullPathNameA(profile,sizeof(absolute),absolute,NULL):0;if(!n||n>=sizeof(absolute)-32)absolute[0]=0;}
#else
 if(!profile)absolute[0]=0;
 else if(profile[0]=='/')snprintf(absolute,sizeof(absolute),"%s",profile);
 else {if(!getcwd(absolute,sizeof(absolute)))absolute[0]=0;else {size_t n=strlen(absolute);snprintf(absolute+n,sizeof(absolute)-n,"/%s",profile);}}
#endif
 if(absolute[0]&&snprintf(path,sizeof(path),"%s.graphics.cfg",absolute)>=(int)sizeof(path))path[0]=0;
 if(!path[0])preference_reason="graphics.reason.save_failed";
 file=path[0]?fopen(path,"rb"):NULL;
 if(file){length=fread(bytes,1,sizeof(bytes),file);if(ferror(file)||!Graphics_Parse(bytes,length,&saved_presence,&saved_values)){
  saved_presence=saved_values=0;preference_reason="graphics.reason.prefs_invalid";
 }fclose(file);}
 effective=((defaults&~saved_presence)|(saved_values&saved_presence));
 effective=(effective&~cli_mask)|(cli_values&cli_mask);
 for(i=0;i<GRAPHICS_COUNT;i++){states[i].requested=(effective>>i)&1;states[i].state=GRAPHICS_PENDING;states[i].reason="graphics.reason.pending";states[i].epoch=0;}
 fprintf(stderr,"Graphics preferences: profile=%s presence=%u values=%u defaults=%u cli_mask=%u effective=%u reason=%s\n",path,saved_presence,saved_values,defaults,cli_mask,effective,preference_reason);
}
static int save(void)
{
 char temporary[4200],backup[4200];FILE *file,*old,*copy;int good=1,i;unsigned long process;
#ifdef _WIN32
 process=GetCurrentProcessId();
#else
 process=(unsigned long)getpid();
#endif
 if(!path[0])return 0;
 snprintf(temporary,sizeof(temporary),"%s.tmp.%lu.%u",path,process,sequence);
 snprintf(backup,sizeof(backup),"%s.bak.%lu.%u",path,process,sequence);
 file=fopen(temporary,"wbx");if(!file)return 0;
 if(fprintf(file,"schema=1\n")<0)good=0;
 for(i=0;i<GRAPHICS_COUNT;i++)if(saved_presence&(1u<<i))if(fprintf(file,"%s=%u\n",i==0?"rt":(i==1?"sr":"nr"),(saved_values>>i)&1)<0)good=0;
 if(fflush(file))good=0;
 if(fclose(file))good=0;
 if(!good){remove(temporary);return 0;}
 errno=0;old=fopen(path,"rb");
 if(!old&&errno!=ENOENT){remove(temporary);return 0;}
 if(old){char previous[PREF_LIMIT+1];size_t n=fread(previous,1,sizeof(previous),old);
  good=!ferror(old)&&n<=PREF_LIMIT;if(fclose(old))good=0;
  if(!good){remove(temporary);return 0;} /* refuse oversized/unreadable original */
  copy=fopen(backup,"wbx");if(!copy){remove(temporary);return 0;}
  good=fwrite(previous,1,n,copy)==n;if(fclose(copy))good=0;
  if(!good){remove(temporary);return 0;}
 }
#ifdef _WIN32
 good=MoveFileExA(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
 good=rename(temporary,path)==0;
#endif
 if(!good)remove(temporary);
 return good;
}
GraphicsStatus Graphics_Get(int feature){return states[feature>=0&&feature<GRAPHICS_COUNT?feature:0];}
void Graphics_SetRequested(int feature,int enabled)
{
 if(feature<0||feature>=GRAPHICS_COUNT)return;
 enabled=enabled!=0;sequence++;states[feature].requested=enabled;states[feature].epoch=sequence;
 states[feature].state=GRAPHICS_PENDING;states[feature].reason="graphics.reason.pending";pending|=1u<<feature;
 saved_presence|=1u<<feature;saved_values=(saved_values&~(1u<<feature))|((unsigned)enabled<<feature);
 preference_reason=save()?"graphics.reason.none":"graphics.reason.save_failed";
 fprintf(stderr,"Graphics menu request: feature=%d requested=%d epoch=%u saved=%s\n",feature,enabled,sequence,preference_reason);
}
unsigned Graphics_TakePending(void){unsigned result=pending;pending=0;return result;}
void Graphics_SetActual(int feature,int state,const char *reason){
 GraphicsStatus *s=&states[feature];if(!reason)reason="graphics.reason.none";
 if(s->state!=state||strcmp(s->reason,reason))fprintf(stderr,"Graphics actual: feature=%d requested=%d state=%s reason=%s epoch=%u\n",feature,s->requested,Graphics_StateKey(state),reason,s->epoch);
 s->state=state;s->reason=reason;
}
const char *Graphics_StateKey(int state){static const char *keys[]={"graphics.state.off","graphics.state.pending","graphics.state.active","graphics.state.unavailable","graphics.state.fallback","graphics.state.unverified","graphics.state.unknown","graphics.state.paused"};return keys[state>=0&&state<8?state:6];}
const char *Graphics_PreferenceReason(void){return preference_reason;}
const char *Graphics_ProfilePath(void){return path;}

int Graphics_DefaultResource(const char *name,char *output,size_t capacity)
{
#ifdef _WIN32
 char exe[4096],*slash;DWORD n=GetModuleFileNameA(NULL,exe,sizeof(exe));
 if(!n||n>=sizeof(exe)||!(slash=strrchr(exe,'\\')))return 0;
 slash[1]=0;return snprintf(output,capacity,"%s%s",exe,name)>0&&strlen(exe)+strlen(name)<capacity;
#else
 (void)name;(void)output;(void)capacity;return 0;
#endif
}
int Graphics_ReadDefaultLight(float values[8])
{
 char name[4096],text[513],*cursor,*end;FILE *file;size_t n;double parsed[8];int used=0,i;
 if(!Graphics_DefaultResource("rt-light.cfg",name,sizeof(name))||(file=fopen(name,"rb"))==NULL)return 0;
 n=fread(text,1,sizeof(text)-1,file);i=!ferror(file)&&n<sizeof(text)-1&&!memchr(text,0,n);fclose(file);
 if(!i){fprintf(stderr,"RT default light: invalid bounded file\n");return 0;}text[n]=0;
 if(sscanf(text,"schema=1 light=%n",&used)!=0||!used)return 0;
 cursor=text+used;
 for(i=0;i<8;i++){parsed[i]=strtod(cursor,&end);if(end==cursor||!isfinite(parsed[i])||fabs(parsed[i])>1e8)return 0;cursor=end;}
 while(isspace((unsigned char)*cursor))cursor++;
 if(*cursor||parsed[3]<0||parsed[3]>1||parsed[4]<0||parsed[4]>1||parsed[5]<0||parsed[5]>1||parsed[6]<0||parsed[7]<=.0625||parsed[7]>8192)return 0;
 for(i=0;i<8;i++)values[i]=(float)parsed[i];
 return 1;
}
