/* Bounded artistic single-bounce material settings. No engine pointers. GPLv2. */
#ifndef WINDOOM_REFLECTION_CONFIG_H
#define WINDOOM_REFLECTION_CONFIG_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define REFLECTION_MAX_MATERIALS 128
#define REFLECTION_MAX_CONFIG_BYTES 65536
typedef struct {
    char name[9];
    unsigned flat_namespace, reflect;
    float roughness, specular, emissive[3];
} ReflectionMaterialConfig;
typedef struct {
    float environment[3];
    unsigned material_count;
    ReflectionMaterialConfig materials[REFLECTION_MAX_MATERIALS];
} ReflectionConfig;
/* Transactional: failure leaves output unchanged. Version1 text is strict ASCII. */
int ReflectionConfig_Parse(const char *text, size_t length, ReflectionConfig *output,
                           char *reason, size_t reason_capacity);
#ifdef __cplusplus
}
#endif
#endif
