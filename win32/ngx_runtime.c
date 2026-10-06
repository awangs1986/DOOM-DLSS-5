/* Copyright (C) 2026 Nikolai Zhivotenko. GPLv2; see LICENSE.TXT. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "ngx_runtime.h"

/* Receipt strings are diagnostic data, never executable paths or commands.
   Walk string tokens so text inside a value cannot impersonate a JSON key. */
static char receipt_sdk[64], receipt_commit[64], sr_hash[65], rr_hash[65];
static HMODULE reported[16];
static unsigned reported_count, reported_stages;

static const char *json_string(const char *p, char *out, size_t capacity)
{
    size_t n = 0;
    if (*p++ != '"') return NULL;
    while (*p && *p != '"')
    {
        char c = *p++;
        if (c == '\\')
        {
            c = *p++;
            if (c != '"' && c != '\\' && c != '/') return NULL;
        }
        if ((unsigned char)c < 32 || n + 1 >= capacity) return NULL;
        out[n++] = c;
    }
    if (*p != '"') return NULL;
    out[n] = 0;
    return p + 1;
}

static int receipt_read(const wchar_t *directory)
{
    wchar_t path[MAX_PATH];
    char json[16385], key[128], value[512], runtime[64] = "";
    const char *p;
    FILE *file;
    size_t length;
    int depth = 0;
    strcpy(receipt_sdk, "unrecorded");
    strcpy(receipt_commit, "unrecorded");
    sr_hash[0] = rr_hash[0] = 0;
    if (swprintf(path, MAX_PATH, L"%ls\\ngx-install.json", directory) < 0) return 0;
    file = _wfopen(path, L"rb");
    if (!file) return 0;
    length = fread(json, 1, sizeof(json) - 1, file);
    if (!feof(file)) { fclose(file); return 0; }
    fclose(file);
    json[length] = 0;
    p = json;
    if (length >= 3 && (unsigned char)p[0] == 0xef && (unsigned char)p[1] == 0xbb && (unsigned char)p[2] == 0xbf) p += 3;
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    if (*p != '{') return 0;
    for (; *p; )
    {
        if (*p == '{') { depth++; if (depth == 2) runtime[0] = 0; p++; }
        else if (*p == '}') { if (--depth < 0) return 0; p++; }
        else if (*p == '"')
        {
            p = json_string(p, key, sizeof(key));
            if (!p) return 0;
            while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
            if (*p != ':') continue;
            p++;
            while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
            if (*p != '"') continue;
            p = json_string(p, value, sizeof(value));
            if (!p) return 0;
            if (depth == 1 && !strcmp(key, "sdkTag") && strlen(value) < sizeof(receipt_sdk)) strcpy(receipt_sdk, value);
            if (depth == 1 && !strcmp(key, "commit") && strlen(value) < sizeof(receipt_commit)) strcpy(receipt_commit, value);
            if (depth == 2 && !strcmp(key, "name") && strlen(value) < sizeof(runtime)) strcpy(runtime, value);
            if (depth == 2 && !strcmp(key, "sha256") && strlen(value) == 64 && strspn(value, "0123456789abcdefABCDEF") == 64)
            {
                if (!strcmp(runtime, "nvngx_dlss.dll")) strcpy(sr_hash, value);
                if (!strcmp(runtime, "nvngx_dlssd.dll")) strcpy(rr_hash, value);
            }
        }
        else p++;
    }
    return depth == 0;
}

static int file_sha256(const wchar_t *path, char result[65])
{
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    unsigned char buffer[65536], digest[32];
    HANDLE file;
    DWORD read = 0;
    unsigned i;
    int ok = 0;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) goto done;
    if (BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0) < 0) goto done;
    for (;;)
    {
        if (!ReadFile(file, buffer, sizeof(buffer), &read, NULL)) goto done;
        if (!read) break;
        if (BCryptHashData(hash, buffer, read, 0) < 0) goto done;
    }
    if (BCryptFinishHash(hash, digest, sizeof(digest), 0) < 0) goto done;
    for (i = 0; i < sizeof(digest); i++) sprintf(result + i * 2, "%02x", digest[i]);
    result[64] = 0;
    ok = 1;
done:
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    return ok;
}

static void file_version(const wchar_t *path, char result[64])
{
    DWORD unused, size = GetFileVersionInfoSizeW(path, &unused);
    VS_FIXEDFILEINFO *info = NULL;
    UINT bytes = 0;
    void *data = size ? malloc(size) : NULL;
    strcpy(result, "unavailable");
    if (data && GetFileVersionInfoW(path, 0, size, data) &&
        VerQueryValueW(data, L"\\", (void **)&info, &bytes) && bytes >= sizeof(*info))
        snprintf(result, 64, "%u.%u.%u.%u", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS), HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
    free(data);
}

int NgxRuntime_Prepare(const wchar_t *directory)
{
    wchar_t path[MAX_PATH];
    HMODULE probe;
    int receipt_ok = receipt_read(directory);
    char hash[65] = "unavailable";
    if (!receipt_ok)
    {
        strcpy(receipt_sdk, "unrecorded");
        strcpy(receipt_commit, "unrecorded");
        sr_hash[0] = rr_hash[0] = 0;
    }
    fprintf(stderr, "NGX identity: deployment_sdk=%s deployment_commit=%s receipt=%s\n", receipt_sdk, receipt_commit, receipt_ok ? "read" : "missing-or-invalid");
    if (swprintf(path, MAX_PATH, L"%ls\\nvngx_dlss.dll", directory) < 0) return 0;
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
    {
        fprintf(stderr, "NGX fallback: reason=runtime-missing expected_path=%ls\n", path);
        return 0;
    }
    probe = LoadLibraryExW(path, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!probe)
    {
        fprintf(stderr, "NGX fallback: reason=runtime-incompatible win32_error=%lu path=%ls\n", GetLastError(), path);
        return 0;
    }
    FreeLibrary(probe);
    file_sha256(path, hash);
    fprintf(stderr, "NGX identity: local_sr_sha256=%s receipt_match=%s (preflight; not evidence of feature execution)\n", hash, sr_hash[0] ? (!_stricmp(hash, sr_hash) ? "yes" : "no") : "unrecorded");
    return 1;
}

void NgxRuntime_ReportLoaded(const char *stage)
{
    unsigned stage_bit = !strcmp(stage, "init") ? 1u : (!strcmp(stage, "sr-create") ? 2u : (!strcmp(stage, "sr-evaluate") ? 4u : (!strcmp(stage, "rr-evaluate") ? 8u : 0u)));
    HANDLE snapshot;
    MODULEENTRY32W module;
    if (reported_stages & stage_bit) return;
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) return;
    memset(&module, 0, sizeof(module));
    module.dwSize = sizeof(module);
    if (Module32FirstW(snapshot, &module)) do
    {
        wchar_t lower[MAX_PATH];
        char version[64], hash[65] = "unavailable";
        const char *expected;
        unsigned i;
        int seen = 0;
        wcsncpy(lower, module.szModule, MAX_PATH - 1); lower[MAX_PATH - 1] = 0;
        _wcslwr(lower);
        if (!wcsstr(lower, L"nvngx_dlss") || wcsstr(lower, L"nvngx_dlssg")) continue;
        for (i = 0; i < reported_count; i++) if (reported[i] == module.hModule) seen = 1;
        if (seen) continue;
        if (reported_count < sizeof(reported) / sizeof(reported[0])) reported[reported_count++] = module.hModule;
        file_version(module.szExePath, version);
        file_sha256(module.szExePath, hash);
        expected = wcsstr(lower, L"nvngx_dlssd") ? rr_hash : sr_hash;
        fprintf(stderr, "NGX loaded: stage=%s module=%ls path=%ls dll_file_version=%s sha256=%s receipt_match=%s\n", stage, module.szModule, module.szExePath, version, hash, expected[0] ? (!_stricmp(hash, expected) ? "yes" : "no") : "unrecorded");
    } while (Module32NextW(snapshot, &module));
    CloseHandle(snapshot);
    reported_stages |= stage_bit;
}

int NgxRuntime_AddonLoaded(void)
{
    return GetModuleHandleW(L"renodx-dlss5.addon64") != NULL;
}

void NgxRuntime_Reset(void)
{
    reported_count = reported_stages = 0;
    receipt_sdk[0] = receipt_commit[0] = sr_hash[0] = rr_hash[0] = 0;
}
