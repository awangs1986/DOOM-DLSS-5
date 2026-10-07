/* GPLv2; see LICENSE.TXT. Restricted, transactional UTF-8 language catalog. */
#include "language.h"
#include "d_englsh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef _WIN32
#include <windows.h>
#endif

struct lang_entry { char key[65]; char value[LANG_MAX_VALUE + 1]; };
struct lang_catalog_s { size_t count; struct lang_entry entries[LANG_MAX_ENTRIES]; };
static lang_catalog_t *active;
static int started;
static char selected[LANG_MAX_ID + 1] = "en";
static const struct { const char *key; const char *value; } english[] = {
    {"graphics.title", "Graphics"},
    {"graphics.rt", "Ray tracing (RT)"},
    {"graphics.sr", "DLSS Super Resolution"},
    {"graphics.nr", "DLSS5 / DLSSNR"},
    {"graphics.actual", "Actual"},
    {"graphics.state.off", "OFF"},
    {"graphics.state.pending", "PENDING"},
    {"graphics.state.active", "ACTIVE"},
    {"graphics.state.unavailable", "UNAVAILABLE"},
    {"graphics.state.fallback", "FALLBACK"},
    {"graphics.state.unverified", "UNVERIFIED"},
    {"graphics.state.unknown", "UNKNOWN"},
    {"graphics.state.paused", "PAUSED"},
    {"graphics.reason.none", ""},
    {"graphics.reason.off", "Requested off."},
    {"graphics.reason.pending", "Applying at next safe frame."},
    {"graphics.reason.non_scene", "Paused outside the normal world view."},
    {"graphics.reason.no_dxr", "DXR 1.1 / shader model 6.5 unavailable."},
    {"graphics.reason.no_effects", "No valid light or reflection settings beside the program."},
    {"graphics.reason.rt_partial", "Some RT effects failed. Other effects continue."},
    {"graphics.reason.effects_disabled", "Configured RT effects are disabled by explicit effect flags."},
    {"graphics.reason.rt_failed", "RT resources failed. Normal display continues."},
    {"graphics.reason.not_compiled", "This mode was built without NVIDIA NGX. Original display remains."},
    {"graphics.reason.runtime", "Official DLSS runtime missing or incompatible. Restore it and re-enable."},
    {"graphics.reason.init", "DLSS initialization failed. Re-enable to retry."},
    {"graphics.reason.capability", "DLSS capability or driver unavailable. Re-enable to recheck."},
    {"graphics.reason.create", "DLSS feature creation failed. RT/native display continues."},
    {"graphics.reason.evaluate", "DLSS execution failed. RT/native display continues."},
    {"graphics.reason.hires", "Optional carrier buffers failed. SR or RT/native display continues."},
    {"graphics.reason.carrier", "DLAA carrier failed. Successful SR remains."},
    {"graphics.reason.backend_missing", "Swapper NR consumer is not loaded. Optional startup dependency; restart after setup."},
    {"graphics.reason.restart", "Optional backend needs startup loading. Restart after existing setup."},
    {"graphics.reason.unsupported", "Loaded NR backend cannot be safely controlled. Actual execution is unknown."},
    {"graphics.reason.confirmation_pending", "Waiting for the original NR checkbox callback."},
    {"graphics.reason.confirmation_failed", "NR checkbox confirmation failed. Actual execution is unknown."},
    {"graphics.reason.execution_unverified", "NR setting confirmed. Actual NR GPU execution is unverified."},
    {"graphics.reason.no_input", "NR request remains. This native route is not supplying an input while SR is off; support is unverified."},
    {"graphics.reason.save_failed", "Preference save failed; this session still uses your request."},
    {"graphics.reason.prefs_invalid", "Invalid saved graphics profile. Using mode defaults."},
    {"graphics.reason.legacy_rr", "This mode evaluates legacy Ray Reconstruction, separately from SR/NR."},

    {"quit.prompt", QUITMSG},
    {"quit.confirm", DOSY},
    {"menu.messages", "Messages"},
    {"option.messages.on", MSGON},
    {"option.messages.off", MSGOFF},
    {"option.state.on", "ON"},
    {"option.state.off", "OFF"},
    {"option.detail.high", "High"},
    {"option.detail.low", "Low"},
    {"prompt.yes_no", PRESSYN},
    {"quicksave.prompt", "quicksave over your game named\n\n'{save_name}'?"},
    {"quickload.prompt", "do you want to quickload the game named\n\n'{save_name}'?"},
    {"option.gamma.0", GAMMALVL0},
    {"option.gamma.1", GAMMALVL1},
    {"option.gamma.2", GAMMALVL2},
    {"option.gamma.3", GAMMALVL3},
    {"option.gamma.4", GAMMALVL4},
    {"pickup.armor", GOTARMOR},
    {"pickup.mega_armor", GOTMEGA},
    {"pickup.health_bonus", GOTHTHBONUS},
    {"pickup.armor_bonus", GOTARMBONUS},
    {"pickup.supercharge", GOTSUPER},
    {"pickup.mega_sphere", GOTMSPHERE},
    {"pickup.blue_card", GOTBLUECARD},
    {"pickup.yellow_card", GOTYELWCARD},
    {"pickup.red_card", GOTREDCARD},
    {"pickup.blue_skull", GOTBLUESKUL},
    {"pickup.yellow_skull", GOTYELWSKUL},
    {"pickup.red_skull", GOTREDSKULL},
    {"pickup.stimpack", GOTSTIM},
    {"pickup.medikit_needed", GOTMEDINEED},
    {"pickup.medikit", GOTMEDIKIT},
    {"pickup.invulnerability", GOTINVUL},
    {"pickup.berserk", GOTBERSERK},
    {"pickup.invisibility", GOTINVIS},
    {"pickup.radiation_suit", GOTSUIT},
    {"pickup.map", GOTMAP},
    {"pickup.light_visor", GOTVISOR},
    {"pickup.clip", GOTCLIP},
    {"pickup.clip_box", GOTCLIPBOX},
    {"pickup.rocket", GOTROCKET},
    {"pickup.rocket_box", GOTROCKBOX},
    {"pickup.cell", GOTCELL},
    {"pickup.cell_box", GOTCELLBOX},
    {"pickup.shells", GOTSHELLS},
    {"pickup.shell_box", GOTSHELLBOX},
    {"pickup.backpack", GOTBACKPACK},
    {"pickup.bfg", GOTBFG9000},
    {"pickup.chaingun", GOTCHAINGUN},
    {"pickup.chainsaw", GOTCHAINSAW},
    {"pickup.launcher", GOTLAUNCHER},
    {"pickup.plasma", GOTPLASMA},
    {"pickup.shotgun", GOTSHOTGUN},
    {"pickup.super_shotgun", GOTSHOTGUN2}
};

static void diagnostic(char *dst, size_t capacity, const char *fmt, ...)
{
    va_list args;
    if (!dst || !capacity) return;
    va_start(args, fmt);
    vsnprintf(dst, capacity, fmt, args);
    va_end(args);
    dst[capacity - 1] = 0;
}

/* Reject overlong encodings, surrogates, NUL, and code points above U+10FFFF. */
static int valid_utf8(const unsigned char *p, size_t n)
{
    size_t i = 0;
    while (i < n) {
        unsigned c = p[i++], value, minimum;
        size_t extra, j;
        if (!c) return 0;
        if (c < 128) continue;
        if (c >= 0xc2 && c <= 0xdf) { extra = 1; value = c & 31; minimum = 128; }
        else if (c >= 0xe0 && c <= 0xef) { extra = 2; value = c & 15; minimum = 2048; }
        else if (c >= 0xf0 && c <= 0xf4) { extra = 3; value = c & 7; minimum = 65536; }
        else return 0;
        if (extra > n - i) return 0;
        for (j = 0; j < extra; ++j) {
            c = p[i++];
            if ((c & 0xc0) != 0x80) return 0;
            value = (value << 6) | (c & 63);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return 0;
    }
    return 1;
}

static char *trim(char *p)
{
    char *end;
    while (*p == ' ' || *p == '\t') ++p;
    end = p + strlen(p);
    while (end > p && (end[-1] == ' ' || end[-1] == '\t')) --end;
    *end = 0;
    return p;
}

static int key_char(unsigned c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_';
}

const char *Lang_Lookup(const lang_catalog_t *catalog, const char *key)
{
    size_t i;
    if (!catalog || !key) return NULL;
    for (i = 0; i < catalog->count; ++i)
        if (!strcmp(key, catalog->entries[i].key)) return catalog->entries[i].value;
    return NULL;
}

lang_catalog_t *Lang_Parse(const void *bytes, size_t length, char *diag, size_t diag_size)
{
    char *copy, *line, *cursor;
    lang_catalog_t *catalog;
    int section = 0, schema = 0, pack_seen = 0, strings_seen = 0;
    size_t line_number = 0;
    const char *reason = "invalid syntax";
    diagnostic(diag, diag_size, "");
    if (!bytes || !length || length > LANG_MAX_FILE) {
        diagnostic(diag, diag_size, "empty or oversized language pack"); return NULL;
    }
    if (!valid_utf8(bytes, length)) {
        diagnostic(diag, diag_size, "invalid UTF-8 or embedded NUL"); return NULL;
    }
    copy = malloc(length + 1);
    catalog = calloc(1, sizeof(*catalog));
    if (!copy || !catalog) {
        free(copy); free(catalog);
        diagnostic(diag, diag_size, "out of memory"); return NULL;
    }
    memcpy(copy, bytes, length); copy[length] = 0;
    cursor = copy;
    if (length >= 3 && !memcmp(cursor, "\xef\xbb\xbf", 3)) cursor += 3;
    while (*cursor) {
        char *eq, *key, *value;
        size_t i, out = 0;
        line = cursor;
        while (*cursor && *cursor != '\n') ++cursor;
        if (*cursor) *cursor++ = 0;
        ++line_number;
        i = strlen(line);
        if (i && line[i - 1] == '\r') line[i - 1] = 0;
        line = trim(line);
        if (!*line || *line == ';' || *line == '#') continue;
        if (!strcmp(line, "[pack]")) {
            if (pack_seen++) { reason = "duplicate section"; goto bad; }
            section = 1; continue;
        }
        if (!strcmp(line, "[strings]")) {
            if (strings_seen++) { reason = "duplicate section"; goto bad; }
            section = 2; continue;
        }
        eq = strchr(line, '=');
        if (!eq || !section) goto bad;
        *eq = 0; key = trim(line); value = trim(eq + 1);
        if (section == 1) {
            if (strcmp(key, "schema") || schema || strcmp(value, "1")) {
                reason = "unknown schema or invalid pack metadata"; goto bad;
            }
            schema = 1; continue;
        }
        i = strlen(key);
        if (!i || i > 64) { reason = "invalid text key"; goto bad; }
        for (i = 0; key[i]; ++i)
            if (!key_char((unsigned char)key[i])) { reason = "invalid text key"; goto bad; }
        if (Lang_Lookup(catalog, key)) { reason = "duplicate text key"; goto bad; }
        if (catalog->count == LANG_MAX_ENTRIES) { reason = "too many text entries"; goto bad; }
        strcpy(catalog->entries[catalog->count].key, key);
        for (i = 0; value[i]; ++i) {
            unsigned char c = (unsigned char)value[i];
            if (c == '\\') {
                c = (unsigned char)value[++i];
                if (c == 'n') c = '\n';
                else if (c == 't') c = '\t';
                else if (c != '\\') { reason = "invalid escape"; goto bad; }
            } else if (c < 32 || c == 127) { reason = "invalid control character"; goto bad; }
            if (out == LANG_MAX_VALUE) { reason = "text value too long"; goto bad; }
            catalog->entries[catalog->count].value[out++] = (char)c;
        }
        if (!out) { reason = "empty text value"; goto bad; }
        catalog->entries[catalog->count].value[out] = 0;
        ++catalog->count;
    }
    if (!schema || !strings_seen) { reason = "missing schema or strings section"; goto bad; }
    free(copy); return catalog;
bad:
    diagnostic(diag, diag_size, "line %lu: %s", (unsigned long)line_number, reason);
    free(copy); free(catalog); return NULL;
}

void Lang_Free(lang_catalog_t *catalog) { free(catalog); }

static int valid_id(const char *id)
{
    size_t i, n = id ? strlen(id) : 0;
    if (!n || n > LANG_MAX_ID) return 0;
    for (i = 0; i < n; ++i)
        if (!((id[i] >= 'a' && id[i] <= 'z') || (id[i] >= '0' && id[i] <= '9') || id[i] == '-' || id[i] == '_')) return 0;
    return 1;
}

void Lang_Shutdown(void)
{
    Lang_Free(active); active = NULL;
    /* Startup is deliberately single-shot: consumers can retain pointers. */
}

const char *Lang_Selected(void) { return selected; }

const char *Lang_Text(const char *key)
{
    const char *translation = Lang_Lookup(active, key);
    size_t i;
    if (!key) return "";
    if (translation) return translation;
    for (i = 0; i < sizeof(english) / sizeof(english[0]); ++i)
        if (!strcmp(key, english[i].key)) return english[i].value;
    return "";
}

void Lang_Startup(const char *directory, const char *selection)
{
    char path[4096], config[4096], saved[LANG_MAX_ID + 3], diag[160];
    const char *id = selection;
    FILE *f;
    size_t n;
    unsigned char *bytes;
    if (started) return;
    started = 1;
    atexit(Lang_Shutdown);
    if (!directory || !directory[0]) {
        printf("Language: cannot resolve executable directory; using en\n"); return;
    }
    if (snprintf(config, sizeof(config), "%s/language.cfg", directory) >= (int)sizeof(config)) {
        printf("Language: executable path too long; using en\n"); return;
    }
    if (!id) {
        f = fopen(config, "rb");
        if (f) {
            n = fread(saved, 1, sizeof(saved) - 1, f);
            if (ferror(f)) n = 0;
            fclose(f); saved[n] = 0;
            while (n && (saved[n - 1] == '\n' || saved[n - 1] == '\r')) saved[--n] = 0;
            if (valid_id(saved)) id = saved;
            else printf("Language: invalid saved selection; using en\n");
        }
    }
    if (!id) id = "en";
    if (!valid_id(id)) { printf("Language: invalid selection; using en\n"); return; }
    if (strcmp(id, "en")) {
        if (snprintf(path, sizeof(path), "%s/languages/%s.ini", directory, id) >= (int)sizeof(path)) {
            printf("Language: pack path too long; using en\n"); return;
        }
        f = fopen(path, "rb");
        if (!f) { printf("Language: cannot open %s; using en\n", path); return; }
        bytes = malloc(LANG_MAX_FILE + 1);
        if (!bytes) { fclose(f); printf("Language: out of memory; using en\n"); return; }
        n = fread(bytes, 1, LANG_MAX_FILE + 1, f);
        if (ferror(f)) { diagnostic(diag, sizeof(diag), "read failed"); active = NULL; }
        else active = Lang_Parse(bytes, n, diag, sizeof(diag));
        fclose(f); free(bytes);
        if (!active) { printf("Language: %s: %s; using en\n", path, diag); return; }
    }
    strcpy(selected, id);
    printf("Language: %s (missing keys use built-in English; font: ASCII)\n", selected);
    if (selection) {
        /* Publish preference only after a complete successful load. */
        if (snprintf(path, sizeof(path), "%s.tmp", config) >= (int)sizeof(path)) return;
        f = fopen(path, "wb");
        if (!f) { printf("Language: cannot save preference\n"); return; }
        {
            int written = fprintf(f, "%s\n", selected) > 0;
            int closed = fclose(f) == 0;
#ifdef _WIN32
            int published = written && closed && MoveFileExA(path, config, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
            int published = written && closed && rename(path, config) == 0;
#endif
            if (!published) { remove(path); printf("Language: cannot save preference\n"); }
        }
    }
}

void Lang_MenuText(const char *src, char *dst, size_t capacity, size_t columns, size_t lines)
{
    size_t out = 0, column = 0, line = 1;
    if (!capacity) return;
    if (!src || !columns || !lines) { dst[0] = 0; return; }
    while (*src && out + 1 < capacity) {
        unsigned char c = (unsigned char)*src++;
        if (c >= 128) {
            while (((unsigned char)*src & 0xc0) == 0x80) ++src;
            c = '?';
        }
        if (c == '\t') c = ' ';
        if (c != '\n' && (c < 32 || c == 96 || c > 122)) c = '?';
        if (c == '\n' || column == columns) {
            if (line == lines) break;
            dst[out++] = '\n'; ++line; column = 0;
            if (c == '\n') continue;
            if (out + 1 >= capacity) break;
        }
        dst[out++] = (char)c; ++column;
    }
    dst[out] = 0;
}

void Lang_QuitMessage(char *dst, size_t capacity)
{
    char prompt[160], confirm[64];
    if (!capacity) return;
    Lang_MenuText(Lang_Text("quit.prompt"), prompt, sizeof(prompt), 30, 5);
    Lang_MenuText(Lang_Text("quit.confirm"), confirm, sizeof(confirm), 30, 2);
    snprintf(dst, capacity, "%s\n\n%s", prompt, confirm);
    dst[capacity - 1] = 0;
}

int Lang_HasTranslation(const char *key) { return Lang_Lookup(active, key) != NULL; }

static const char *english_text(const char *key)
{
    size_t i;
    for (i = 0; i < sizeof(english) / sizeof(english[0]); ++i)
        if (!strcmp(key, english[i].key)) return english[i].value;
    return "";
}

/* Templates only use the save_name TEXT parameter. Escaped braces are literal.
 * Validate completely before rendering, so a bad translated suffix cannot leak. */
static int template_valid(const char *text)
{
    int found = 0;
    while (*text) {
        if (*text == '{') {
            if (text[1] == '{') text += 2;
            else if (!strncmp(text, "{save_name}", 11)) {
                if (found++) return 0;
                text += 11;
            } else return 0;
        } else if (*text == '}') {
            if (text[1] != '}') return 0;
            text += 2;
        } else ++text;
    }
    return found == 1;
}

static size_t utf8_span(unsigned char c)
{
    if (c < 128) return 1;
    if (c < 0xe0) return 2;
    if (c < 0xf0) return 3;
    return 4;
}

static int append_codepoint(char *dst, size_t capacity, size_t *out, const char *text)
{
    size_t span = utf8_span((unsigned char)*text);
    if (span >= capacity - *out) return 0;
    memcpy(dst + *out, text, span); *out += span;
    return 1;
}

int Lang_Format(const char *key, const lang_arg_t *args, size_t count,
                char *dst, size_t capacity, char *diag, size_t diag_size)
{
    const char *text, *parameter = "<unknown save>";
    size_t out = 0, i;
    int success = 1;
    diagnostic(diag, diag_size, "");
    if (!dst || !capacity) {
        diagnostic(diag, diag_size, "template output buffer is empty"); return 0;
    }
    dst[0] = 0;
    if (!key || (strcmp(key, "quicksave.prompt") && strcmp(key, "quickload.prompt"))) {
        diagnostic(diag, diag_size, "unknown template key"); return 0;
    }
    if (count != 1 || !args || !args[0].name || strcmp(args[0].name, "save_name") ||
        args[0].type != LANG_ARG_TEXT || !args[0].text ||
        strlen(args[0].text) > 128 || !valid_utf8((const unsigned char *)args[0].text, strlen(args[0].text))) {
        diagnostic(diag, diag_size, "invalid template parameter name, count, type or UTF-8");
        success = 0;
    } else {
        parameter = args[0].text;
        for (i = 0; parameter[i]; ++i) {
            if ((unsigned char)parameter[i] < 32 || (unsigned char)parameter[i] == 127) {
                parameter = "<unknown save>"; success = 0;
                diagnostic(diag, diag_size, "invalid control character in template parameter"); break;
            }
        }
    }
    text = success ? Lang_Text(key) : english_text(key);
    if (!template_valid(text)) {
        text = english_text(key); success = 0;
        diagnostic(diag, diag_size, "invalid translated template; using English template");
    }
    while (*text) {
        if (!strncmp(text, "{save_name}", 11)) {
            const char *p = parameter;
            while (*p) {
                if (!append_codepoint(dst, capacity, &out, p)) goto done;
                p += utf8_span((unsigned char)*p);
            }
            text += 11;
        } else if ((*text == '{' && text[1] == '{') || (*text == '}' && text[1] == '}')) {
            if (!append_codepoint(dst, capacity, &out, text)) break;
            text += 2;
        } else {
            if (!append_codepoint(dst, capacity, &out, text)) break;
            text += utf8_span((unsigned char)*text);
        }
    }
done:
    dst[out] = 0;
    return success;
}

void Lang_SaveMessage(int load, const char *save_name, char *dst, size_t capacity,
                      char *diag, size_t diag_size)
{
    char body[LANG_MAX_VALUE + 129], prompt[160], confirm[64];
    lang_arg_t arg = {"save_name", LANG_ARG_TEXT, save_name};
    if (!capacity) return;
    Lang_Format(load ? "quickload.prompt" : "quicksave.prompt", &arg, 1,
                body, sizeof(body), diag, diag_size);
    Lang_MenuText(body, prompt, sizeof(prompt), 30, 5);
    Lang_MenuText(Lang_Text("prompt.yes_no"), confirm, sizeof(confirm), 30, 2);
    snprintf(dst, capacity, "%s\n\n%s", prompt, confirm);
    dst[capacity - 1] = 0;
}
