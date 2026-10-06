/* GPLv2; see LICENSE.TXT. Runtime text catalog; UTF-8 is distinct from fonts. */
#ifndef WINDOOM_LANGUAGE_H
#define WINDOOM_LANGUAGE_H
#include <stddef.h>
#define LANG_MAX_FILE 65536
#define LANG_MAX_VALUE 2048
#define LANG_MAX_ENTRIES 256
#define LANG_MAX_ID 32

typedef struct lang_catalog_s lang_catalog_t;
/* Failure publishes no partial catalog. diag is always NUL terminated if nonempty. */
lang_catalog_t *Lang_Parse(const void *bytes, size_t length, char *diag, size_t diag_size);
const char *Lang_Lookup(const lang_catalog_t *catalog, const char *key);
void Lang_Free(lang_catalog_t *catalog);
/* Executable-relative directory, optional -lang ID. No runtime hot switching. */
void Lang_Startup(const char *directory, const char *selection);
void Lang_Shutdown(void);
const char *Lang_Text(const char *key);
const char *Lang_Selected(void);
void Lang_QuitMessage(char *dst, size_t capacity);
#ifdef _WIN32
void Lang_InitGame(void);
#endif
/* Original font: printable ASCII, one '?' per unsupported UTF-8 code point.
 * Wrap/truncate by columns and lines, never split UTF-8 in the source. */
void Lang_MenuText(const char *src, char *dst, size_t capacity, size_t columns, size_t lines);
#endif
