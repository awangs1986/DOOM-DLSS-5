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
boolean message_dontfuckwithme;
boolean netgame;
boolean deathmatch;
skill_t gameskill;
GameMode_t gamemode = commercial;
patch_t *hu_font[HU_FONTSIZE];
static int removed, sounds, drawn;
extern int showMessages;
extern char *messageString;
void M_ChangeMessages(int choice);
void M_StartMessage(char *string, void *routine, boolean input);
int M_StringWidth(char *string);
void M_WriteText(int x, int y, char *string);
void S_StartSound(void *origin, int sfx_id) { (void)origin; (void)sfx_id; ++sounds; }
void P_RemoveMobj(mobj_t *thing) { (void)thing; ++removed; }
void I_Error(char *fmt, ...) { (void)fmt; abort(); }
void V_DrawPatchDirect(int x, int y, int scrn, patch_t *patch)
{ (void)x; (void)y; (void)scrn; (void)patch; ++drawn; }

int main(int argc, char **argv)
{
    mobj_t pickup = {0}, toucher = {0};
    hu_stext_t hud = {0};
    patch_t glyph = {0};
    char long_prompt[2049];
    int i;
    assert(argc == 3);
    Lang_Startup(argv[1], argv[2]);
    showMessages = 1;
    M_ChangeMessages(0);
    assert(showMessages == 0 && message_dontfuckwithme);
    assert(!strcmp(players[0].message, !strcmp(argv[2], "en") ? "Messages OFF" : "Mensajes DESACTIVADOS"));
    M_ChangeMessages(0);
    assert(showMessages == 1);
    assert(!strcmp(players[0].message, !strcmp(argv[2], "en") ? "Messages ON" : "Mensajes ACTIVADOS"));
    toucher.player = &players[0]; toucher.health = 100; toucher.height = 56 * FRACUNIT;
    pickup.sprite = SPR_ARM1; pickup.flags = MF_COUNTITEM;
    P_TouchSpecialThing(&pickup, &toucher);
    assert(players[0].armorpoints == 100 && players[0].itemcount == 1 && removed == 1 && sounds == 1);
    assert(!strcmp(players[0].message, !strcmp(argv[2], "en") ? "Picked up the armor." : "Has recogido una armadura."));
    hud.h = 1;
    HUlib_addMessageToSText(&hud, NULL, "A中文B");
    assert(!strcmp(hud.l[0].l, "A??B"));
    glyph.width = 8; glyph.height = 8;
    for (i = 0; i < HU_FONTSIZE; ++i) hu_font[i] = &glyph;
    assert(M_StringWidth("A中文B") == 32);
    M_WriteText(0, 0, "A中文B");
    assert(drawn == 4);
    memset(long_prompt, 'X', sizeof(long_prompt) - 1); long_prompt[sizeof(long_prompt) - 1] = 0;
    M_StartMessage(long_prompt, NULL, true);
    assert(strlen(messageString) < 256);
    memset(long_prompt, 'Y', sizeof(long_prompt) - 1);
    assert(messageString[0] == 'X'); /* consumer owns stable display copy */
    puts("actual engine option/pickup/HUD/message consumer tests passed");
    return 0;
}
