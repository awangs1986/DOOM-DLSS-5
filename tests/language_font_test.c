/* CJK font parser and UTF-8 boundary tests. GPLv2; see LICENSE.TXT. */
#include "language.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const unsigned char *mask = NULL;
    const char *p = "A中Z";
    uint32_t cp;
    int expected_valid;
    if (argc != 3) return 2;
    expected_valid = !strcmp(argv[2], "valid");
    Lang_Startup(argv[1], "zh-cn");
    if (expected_valid) {
        assert(!strcmp(Lang_Selected(), "zh-cn"));
        assert(Lang_CjkFontReady());
        assert(Lang_CjkGlyph(0x4e2d, &mask));
        assert(mask != NULL);
        assert(Lang_CjkGlyph(0x4e2d, NULL));
        assert(!Lang_CjkGlyph(0x10ffff, NULL));
        assert(Lang_DecodeUtf8(&p, &cp) == 1 && cp == 'A');
        assert(Lang_DecodeUtf8(&p, &cp) == 1 && cp == 0x4e2d);
        assert(Lang_DecodeUtf8(&p, &cp) == 1 && cp == 'Z');
        assert(Lang_DecodeUtf8(&p, &cp) == 0);
        p = "\xe2(";
        assert(Lang_DecodeUtf8(&p, &cp) == -1 && cp == '?');
        assert(Lang_DecodeUtf8(&p, &cp) == 1 && cp == '(');
    } else {
        assert(!strcmp(Lang_Selected(), "en"));
        assert(!Lang_CjkFontReady());
        assert(!strcmp(Lang_Text("quit.prompt"), "are you sure you want to\nquit this great game?"));
    }
    return 0;
}
