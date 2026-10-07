/* Public bounded copier behavior, including opaque index zero. GPLv2. */
#include "r_material.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    unsigned char patch[32]={2,0,4,0,0,0,0,0,16,0,0,0,24,0,0,0,
                             1,2,0,0,42,0,255,0,0,1,0,9,0,255,0,0};
    unsigned char indices[16],alpha[16],old[16];
    memset(indices,7,sizeof(indices));memset(alpha,0,sizeof(alpha));
    assert(R_ComposeMaterialPatch(patch,sizeof(patch),0,0,4,4,indices,alpha,16));
    assert(indices[4]==0 && alpha[4]==255); /* index zero is not transparency */
    assert(indices[8]==42 && alpha[8]==255);
    assert(indices[1]==9 && alpha[1]==255 && alpha[0]==0 && alpha[12]==0);
    /* Later transparent gaps preserve previous opaque posts. */
    assert(R_ComposeMaterialPatch(patch,sizeof(patch),-1,1,4,4,indices,alpha,16));
    assert(indices[4]==9 && alpha[4]==255 && indices[8]==42);
    memcpy(old,indices,16);
    patch[9]=255; /* invalid directory offset: no partial mutation */
    assert(!R_ComposeMaterialPatch(patch,sizeof(patch),0,0,4,4,indices,alpha,16));
    assert(!memcmp(old,indices,16));patch[9]=0;
    assert(!R_ComposeMaterialPatch(patch,21,0,0,4,4,indices,alpha,16));
    assert(!memcmp(old,indices,16));
    assert(!R_ComposeMaterialPatch(patch,sizeof(patch),0,0,4097,4,indices,alpha,16));
    assert(!R_ComposeMaterialPatch(patch,sizeof(patch),0,0,4,4,indices,alpha,15));
    patch[13]=patch[14]=patch[15]=255;
    assert(!R_ComposeMaterialPatch(patch,sizeof(patch),0,0,4,4,indices,alpha,16));
    puts("bounded patch copy: opaque zero, gaps, placement and malformed bounds passed");
    return 0;
}
