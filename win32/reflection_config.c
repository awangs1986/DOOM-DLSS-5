#include "reflection_config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
static int fail(char *reason,size_t capacity,unsigned line,const char *message)
{
    if(reason&&capacity)snprintf(reason,capacity,"line %u: %s",line,message);
    return 0;
}
static int number(const char *s,double low,double high,float *output)
{
    char *end;double value;
    if(!s||!*s)return 0;
    value=strtod(s,&end);
    if(*end||!isfinite(value)||value<low||value>high)return 0;
    *output=(float)value;return 1;
}
int ReflectionConfig_Parse(const char *text,size_t length,ReflectionConfig *output,char *reason,size_t capacity)
{
    ReflectionConfig parsed;size_t position=0;unsigned line=0;int version=0,environment=0;
    if(!text||!output||!length||length>REFLECTION_MAX_CONFIG_BYTES)return fail(reason,capacity,0,"invalid config size");
    memset(&parsed,0,sizeof(parsed));
    parsed.environment[0]=0.015f;parsed.environment[1]=0.025f;parsed.environment[2]=0.04f;
    while(position<length) {
        char row[512],*words[12];unsigned n=0,i;size_t start=position,size;
        line++;
        while(position<length&&text[position]!='\n')position++;
        size=position-start;if(position<length)position++;
        if(size>=sizeof(row))return fail(reason,capacity,line,"line too long");
        for(i=0;i<size;i++)if((unsigned char)text[start+i]>126||(!text[start+i])||((unsigned char)text[start+i]<32&&text[start+i]!='\t'&&text[start+i]!='\r'))return fail(reason,capacity,line,"non-ASCII/control byte");
        memcpy(row,text+start,size);row[size]=0;
        {char *comment=strchr(row,'#');if(comment)*comment=0;}
        {char *p=row;while(*p) {
            while(*p==' '||*p=='\t'||*p=='\r')p++;
            if(!*p)break;
            if(n>=12)return fail(reason,capacity,line,"too many fields");
            words[n++]=p;while(*p&&*p!=' '&&*p!='\t'&&*p!='\r')p++;
            if(*p)*p++=0;
        }}
        if(!n)continue;
        if(!version) {
            if(n!=2||strcmp(words[0],"version")||strcmp(words[1],"1"))return fail(reason,capacity,line,"expected version 1");
            version=1;continue;
        }
        if(!strcmp(words[0],"environment")) {
            if(environment||n!=4)return fail(reason,capacity,line,"invalid/duplicate environment");
            for(i=0;i<3;i++)if(!number(words[i+1],0,16,&parsed.environment[i]))return fail(reason,capacity,line,"environment must be finite linear RGB in [0,16]");
            environment=1;continue;
        }
        if(!strcmp(words[0],"material")) {
            ReflectionMaterialConfig entry;size_t name_length;
            if(n!=9||parsed.material_count>=REFLECTION_MAX_MATERIALS)return fail(reason,capacity,line,"material field/count limit");
            memset(&entry,0,sizeof(entry));
            if(!strcmp(words[1],"wall"))entry.flat_namespace=0;
            else if(!strcmp(words[1],"flat"))entry.flat_namespace=1;
            else return fail(reason,capacity,line,"expected wall or flat namespace");
            name_length=strlen(words[2]);if(!name_length||name_length>8)return fail(reason,capacity,line,"material name must be 1..8 bytes");
            for(i=0;i<name_length;i++) {unsigned char c=(unsigned char)words[2][i];if(!(isalnum(c)||c=='_'||c=='-'))return fail(reason,capacity,line,"invalid material name");entry.name[i]=(char)toupper(c);}
            if(!strcmp(words[3],"0"))entry.reflect=0;
            else if(!strcmp(words[3],"1"))entry.reflect=1;
            else return fail(reason,capacity,line,"reflect must be 0 or 1");
            if(!number(words[4],0,0.25,&entry.roughness)||!number(words[5],0,1,&entry.specular))return fail(reason,capacity,line,"roughness [0,0.25], specular [0,1] required");
            for(i=0;i<3;i++)if(!number(words[6+i],0,16,&entry.emissive[i]))return fail(reason,capacity,line,"emissive must be finite linear RGB in [0,16]");
            for(i=0;i<parsed.material_count;i++)if(entry.flat_namespace==parsed.materials[i].flat_namespace&&!strcmp(entry.name,parsed.materials[i].name))return fail(reason,capacity,line,"duplicate material");
            parsed.materials[parsed.material_count++]=entry;continue;
        }
        return fail(reason,capacity,line,"unknown directive");
    }
    if(!version)return fail(reason,capacity,line,"missing version");
    *output=parsed;if(reason&&capacity)reason[0]=0;return 1;
}
