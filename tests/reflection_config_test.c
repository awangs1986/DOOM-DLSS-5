#include "reflection_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void rejected(const char *text)
{
    ReflectionConfig before,after;char reason[128];
    memset(&before,0x5a,sizeof(before));after=before;
    assert(!ReflectionConfig_Parse(text,strlen(text),&after,reason,sizeof(reason)));
    assert(reason[0]&&memcmp(&before,&after,sizeof(before))==0);
}
int main(void)
{
    ReflectionConfig result;char reason[128];
    const char *valid="# example\nversion 1\nenvironment 0.1 0.2 0.3\nmaterial wall mirror 1 0.1 0.04 1 0 0\nmaterial flat mirror 0 0 1 0 0 0\n";
    assert(ReflectionConfig_Parse(valid,strlen(valid),&result,reason,sizeof(reason)));
    assert(result.material_count==2&&!strcmp(result.materials[0].name,"MIRROR"));
    assert(result.materials[0].reflect&&!result.materials[1].reflect&&result.materials[1].flat_namespace);
    assert(result.materials[0].roughness==0.1f&&result.materials[0].emissive[0]==1);
    assert(ReflectionConfig_Parse("version 1\n",10,&result,reason,sizeof(reason))&&result.material_count==0);
    rejected("version 2\n");rejected("material wall MIRROR 1 0 1 0 0 0\n");
    rejected("version 1\nmaterial wall TOO_LONG_NAME 1 0 1 0 0 0\n");
    rejected("version 1\nmaterial wall MIRROR 1 nan 1 0 0 0\n");
    rejected("version 1\nmaterial wall MIRROR 1 0.26 1 0 0 0\n");
    rejected("version 1\nmaterial wall MIRROR 2 0 1 0 0 0\n");
    rejected("version 1\nmaterial wall MIRROR 1 0 1 inf 0 0\n");
    rejected("version 1\nmaterial wall MIRROR 1 0 1 0 0 0\nmaterial wall mirror 1 0 1 0 0 0\n");
    rejected("version 1\nenvironment 0 0 0\nenvironment 1 1 1\n");
    rejected("version 1\nmaterial wall MIRROR 1 0 1 0 0 0 extra\n");
    rejected("version 1\nunknown 1\n");
    {char text[REFLECTION_MAX_CONFIG_BYTES+1];memset(text,' ',sizeof(text));assert(!ReflectionConfig_Parse(text,sizeof(text),&result,reason,sizeof(reason)));}
    puts("reflection config: namespaces, numeric bounds, strict grammar and transactional failure PASS");
    return 0;
}
