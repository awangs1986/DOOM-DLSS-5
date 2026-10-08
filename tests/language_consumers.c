/* Real engine option/pickup/HUD consumers, platform I/O replaced at link seam. */
#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include "s_sound.h"
#include "language.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

player_t players[MAXPLAYERS];
int consoleplayer;
boolean automapactive;
int viewwindowx, viewwindowy, viewheight, viewwidth;
boolean message_dontfuckwithme;
boolean netgame;
boolean deathmatch;
skill_t gameskill;
GameMode_t gamemode = commercial;
patch_t *hu_font[HU_FONTSIZE];
static int removed, sounds, drawn, cjk_drawn, cjk_x[8];
static int erased_rows;
extern int showMessages;
extern char *messageString;
void M_ChangeMessages(int choice);
void M_StartMessage(char *string, void *routine, boolean input);
int M_StringWidth(char *string);
int M_StringHeight(char *string);
void M_WriteText(int x, int y, char *string);
void S_StartSound(void *origin, int sfx_id) { (void)origin; (void)sfx_id; ++sounds; }
void P_RemoveMobj(mobj_t *thing) { (void)thing; ++removed; }
void I_Error(char *fmt, ...) { (void)fmt; abort(); }
void V_DrawPatchDirect(int x, int y, int scrn, patch_t *patch)
{ (void)x; (void)y; (void)scrn; (void)patch; ++drawn; }
void V_DrawCjkGlyph(int x, int y, const unsigned char *mask, patch_t *patch)
{ (void)y; (void)mask; (void)patch; if (cjk_drawn < 8) cjk_x[cjk_drawn] = x; ++drawn; ++cjk_drawn; }
void R_VideoErase(unsigned ofs, int count)
{ (void)count; if (ofs % SCREENWIDTH == 0) ++erased_rows; }

int main(int argc, char **argv)
{
    mobj_t pickup = {0}, toucher = {0};
    hu_stext_t hud = {0};
    patch_t glyph = {0};
    char long_prompt[2049];
    char long_cjk[1200];
    char translated_prompt[LANG_MAX_VALUE + 260];
    int i;
    assert(argc == 3);
    Lang_Startup(argv[1], argv[2]);
    if (!strcmp(argv[2], "zh-cn")) assert(Lang_CjkFontReady());
    showMessages = 1;
    M_ChangeMessages(0);
    assert(showMessages == 0 && message_dontfuckwithme);
    assert(!strcmp(players[0].message, !strcmp(argv[2], "en") ? "Messages OFF" :
                   !strcmp(argv[2], "zh-cn") ? "消息已关闭" : "Mensajes DESACTIVADOS"));
    M_ChangeMessages(0);
    assert(showMessages == 1);
    assert(!strcmp(players[0].message, !strcmp(argv[2], "en") ? "Messages ON" :
                   !strcmp(argv[2], "zh-cn") ? "消息已开启" : "Mensajes ACTIVADOS"));
    toucher.player = &players[0]; toucher.health = 100; toucher.height = 56 * FRACUNIT;
    pickup.sprite = SPR_ARM1; pickup.flags = MF_COUNTITEM;
    P_TouchSpecialThing(&pickup, &toucher);
    assert(players[0].armorpoints == 100 && players[0].itemcount == 1 && removed == 1 && sounds == 1);
    assert(!strcmp(players[0].message, !strcmp(argv[2], "en") ? "Picked up the armor." :
                   !strcmp(argv[2], "zh-cn") ? "拾取了护甲。" : "Has recogido una armadura."));
    hud.h = 1;
    HUlib_addMessageToSText(&hud, NULL, "A中文B");
    assert(!strcmp(hud.l[0].l, "A??B"));
    if (!strcmp(argv[2], "zh-cn"))
        assert(hud.l[0].len == 4 && hud.l[0].codepoints[1] == 0x4e2d && hud.l[0].codepoints[2] == 0x6587);
    glyph.width = 8; glyph.height = 8;
    for (i = 0; i < HU_FONTSIZE; ++i) hu_font[i] = &glyph;
    assert(M_StringWidth("A中文B") == (!strcmp(argv[2], "zh-cn") ? 36 : 32));
    M_WriteText(0, 0, "A中文B");
    assert(drawn == 4);
    if (!strcmp(argv[2], "zh-cn")) {
        assert(cjk_drawn == 1);
        hud.l[0].f = hu_font; hud.l[0].sc = HU_FONTSTART;
        hud.l[0].x = SCREENWIDTH - 20; hud.l[0].y = 0;
        HUlib_drawTextLine(&hud.l[0], false);
        assert(cjk_drawn == 2 && cjk_x[1] == SCREENWIDTH - 12);
        {
            hu_textline_t erased = {0};
            HUlib_initTextLine(&erased, 0, 10, hu_font, HU_FONTSTART);
            HUlib_addCodepointToTextLine(&erased, 0x4e2d);
            HUlib_drawTextLine(&erased, false);
            HUlib_clearTextLine(&erased); /* contents clear before the next erase */
            erased.needsupdate = 1;
            viewwindowx = 1; viewwindowy = 50; viewheight = 100;
            viewwidth = SCREENWIDTH - 2; automapactive = false;
            erased_rows = 0;
            HUlib_eraseTextLine(&erased);
            assert(erased_rows == 12 && erased.drawn_height == 0);
        }
    }
    if (!strcmp(argv[2], "zh-cn")) {
        Lang_QuitMessage(translated_prompt, sizeof(translated_prompt));
        assert(strstr(translated_prompt, "确定要退出") && strstr(translated_prompt, "按 Y 退出"));
        M_StartMessage(translated_prompt, NULL, true);
        assert(strstr(messageString, "按 Y 退出"));
        Lang_SaveMessage(0, "MY SAVE", translated_prompt, sizeof(translated_prompt), NULL, 0);
        assert(strstr(translated_prompt, "MY SAVE") && strstr(translated_prompt, "按 Y 确认，按 N 取消"));
        M_StartMessage(translated_prompt, NULL, true);
        assert(strstr(messageString, "MY SAVE") && strstr(messageString, "按 Y 确认，按 N 取消"));
        const char phrase[] = "中文语言选项";
        const char *confirm = "\n\n按 Y 确认，按 N 取消";
        size_t used = 0, i;
        int lines = 1;
        for (i = 0; i < 60; ++i) {
            memcpy(long_cjk + used, phrase, sizeof(phrase) - 1);
            used += sizeof(phrase) - 1;
        }
        memcpy(long_cjk + used, confirm, strlen(confirm) + 1);
        M_StartMessage(long_cjk, NULL, true);
        assert(strstr(messageString, "按 Y 确认，按 N 取消"));
        for (i = 0; messageString[i]; ++i) if (messageString[i] == '\n') ++lines;
        assert(lines <= 8);
        {
            char *line = messageString;
            while (*line) {
                char *end = strchr(line, '\n');
                char saved = end ? *end : 0;
                if (end) *end = 0;
                assert(M_StringWidth(line) <= 240);
                if (end) { *end = saved; line = end + 1; }
                else break;
            }
        }
        assert(M_StringHeight(messageString) == (lines - 2) * 12 + 8 + 12);
    }
    memset(long_prompt, 'X', sizeof(long_prompt) - 1); long_prompt[sizeof(long_prompt) - 1] = 0;
    M_StartMessage(long_prompt, NULL, true);
    assert(strlen(messageString) < 256);
    memset(long_prompt, 'Y', sizeof(long_prompt) - 1);
    assert(messageString[0] == 'X'); /* consumer owns stable display copy */
    puts("actual engine option/pickup/HUD/message consumer tests passed");
    return 0;
}
