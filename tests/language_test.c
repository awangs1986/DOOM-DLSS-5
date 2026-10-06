/* Public catalog/consumer behavior tests. GPLv2; see LICENSE.TXT. */
#include "language.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lang_catalog_t *parse(const char *text)
{
    char diag[160];
    lang_catalog_t *catalog = Lang_Parse(text, strlen(text), diag, sizeof(diag));
    if (!catalog) fprintf(stderr, "unexpected rejection: %s\n", diag);
    assert(catalog != NULL);
    assert(!diag[0]);
    return catalog;
}

static void reject(const void *text, size_t size)
{
    char diag[160];
    lang_catalog_t *catalog = Lang_Parse(text, size, diag, sizeof(diag));
    assert(catalog == NULL);
    assert(diag[0]);
}

#define REJECT(s) reject(s, sizeof(s) - 1)
int main(void)
{
    lang_catalog_t *catalog, *other;
    const char *retained;
    char menu[256], small[4], *large;
    size_t n;
    catalog = parse("\xef\xbb\xbf[pack]\r\nschema = 1\r\n[strings]\r\nquit.prompt = Salir?\\nOtra linea\r\nfuture.text = a\\tb\\\\c\r\n");
    assert(!strcmp(Lang_Lookup(catalog, "quit.prompt"), "Salir?\nOtra linea"));
    assert(!strcmp(Lang_Lookup(catalog, "future.text"), "a\tb\\c"));
    assert(Lang_Lookup(catalog, "missing") == NULL);
    retained = Lang_Lookup(catalog, "quit.prompt");
    other = parse("[pack]\nschema=1\n[strings]\nquit.prompt=Quit?\n");
    Lang_Free(other);
    assert(retained == Lang_Lookup(catalog, "quit.prompt"));
    assert(!strcmp(retained, "Salir?\nOtra linea"));
    Lang_Free(catalog);
    catalog = parse("; comment\n[pack]\nschema=1\n\n# comment\n[strings]\n");
    assert(Lang_Lookup(catalog, "quit.prompt") == NULL);
    Lang_Free(catalog);
    catalog = parse("[pack]\nschema=1\n[strings]\nquit.prompt=中文\n");
    assert(!strcmp(Lang_Lookup(catalog, "quit.prompt"), "中文"));
    Lang_Free(catalog);
    REJECT("[pack]\nschema=2\n[strings]\nquit.prompt=valid\n");
    REJECT("[strings]\nquit.prompt=valid\n");
    REJECT("[pack]\nschema=1\n[strings]\nquit.prompt=valid\nquit.prompt=duplicate\n");
    REJECT("[pack]\nschema=1\nschema=1\n[strings]\n");
    REJECT("[pack]\nschema=1\n[strings]\n[strings]\n");
    REJECT("[pack]\nschema=1\n[strings]\nquit.prompt=valid\nfault=bad\\q\n");
    REJECT("[pack]\nschema=1\n[strings]\nquit.prompt=\n");
    REJECT("[pack]\nschema=1\n[strings]\nquit.prompt=bad\rvalue\n");
    REJECT("[pack]\nschema=1\n[strings]\nquit.prompt=bad\0value\n");
    REJECT("[pack]\nschema=1\n[strings]\nquit.prompt=\xc0\xaf\n");
    REJECT("[pack]\nschema=1\n[strings]\nquit.prompt=\xed\xa0\x80\n");
    REJECT("[pack]\nschema=1\n[strings]\nquit.prompt=\xf4\x90\x80\x80\n");
    REJECT("[pack]\nschema=1\n[strings]\nquit.prompt=\xe2\x82");
    REJECT("[pack]\nschema=1\n[strings]\n../unsafe=bad\n");
    large = malloc(LANG_MAX_FILE + 2);
    assert(large);
    strcpy(large, "[pack]\nschema=1\n[strings]\nquit.prompt=");
    n = strlen(large);
    memset(large + n, 'a', LANG_MAX_VALUE);
    large[n + LANG_MAX_VALUE] = 0;
    catalog = parse(large);
    assert(strlen(Lang_Lookup(catalog, "quit.prompt")) == LANG_MAX_VALUE);
    Lang_Free(catalog);
    large[n + LANG_MAX_VALUE] = 'a';
    large[n + LANG_MAX_VALUE + 1] = 0;
    reject(large, strlen(large));
    reject(large, LANG_MAX_FILE + 1);
    strcpy(large, "[pack]\nschema=1\n[strings]\n");
    n = strlen(large);
    {
        int i;
        for (i = 0; i <= LANG_MAX_ENTRIES; ++i)
            n += (size_t)sprintf(large + n, "text.%d=entry\n", i);
    }
    reject(large, n);
    free(large);
    Lang_MenuText("one two 中文\nnext", menu, sizeof(menu), 8, 3);
    assert(!strcmp(menu, "one two \n??\nnext"));
    Lang_MenuText("abcdefghi", small, sizeof(small), 30, 2);
    assert(!strcmp(small, "abc"));
    Lang_MenuText("abcdefghi", menu, sizeof(menu), 3, 2);
    assert(!strcmp(menu, "abc\ndef"));
    Lang_MenuText("ignored", menu, 0, 3, 2);
    Lang_QuitMessage(menu, sizeof(menu));
    assert(strstr(menu, "are you sure you want to"));
    assert(strstr(menu, "(press y to quit)"));
    puts("language public-interface tests passed");
    return 0;
}
