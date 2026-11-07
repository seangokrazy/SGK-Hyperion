#pragma once
#include <stdint.h>
// ABI layout adapted from the MIT-licensed Open MPC gain proof of concept.
struct AEffect;
typedef intptr_t (*Host)(AEffect*,int32_t,int32_t,intptr_t,void*,float);
struct AEffect {
 int32_t magic;intptr_t (*dispatcher)(AEffect*,int32_t,int32_t,intptr_t,void*,float);
 void (*process)(AEffect*,float**,float**,int32_t);
 void (*setParameter)(AEffect*,int32_t,float);float (*getParameter)(AEffect*,int32_t);
 int32_t numPrograms,numParams,numInputs,numOutputs,flags;intptr_t resvd1,resvd2;
 int32_t initialDelay,realQualities,offQualities;float ioRatio;void *object,*user;
 int32_t uniqueID,version;void (*processReplacing)(AEffect*,float**,float**,int32_t);
 void (*processDoubleReplacing)(AEffect*,double**,double**,int32_t);char future[56];
};
extern "C" AEffect* VSTPluginMain(Host);
