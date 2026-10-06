/* Fresh-process startup consumer; exercises the same API as the game. */
#include "language.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
    char message[256];
    const char *retained;
    if (argc < 2) return 2;
    Lang_Startup(argv[1], argc > 2 ? argv[2] : NULL);
    retained = Lang_Text("quit.prompt");
    Lang_Startup(argv[1], "en"); /* restart is required, retained pointer stable */
    if (retained != Lang_Text("quit.prompt")) return 3;
    Lang_QuitMessage(message, sizeof(message));
    printf("SELECTED=%s\nPROMPT=%s\nCONFIRM=%s\nMESSAGE=%s\n", Lang_Selected(),
           Lang_Text("quit.prompt"), Lang_Text("quit.confirm"), message);
    return 0;
}
