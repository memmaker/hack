/* Web build: the VT100 capabilities port/vt.c understands (no termcap
 * library in Emscripten). */
#include <stdio.h>
#include <string.h>
#include "web-inc/termcap.h"

short ospeed;
static const char *cap[][2] = {
    { "cl", "\033[H\033[2J" }, { "ce", "\033[K" }, { "cd", "\033[J" }, { "cm", "\033[%d;%dH" },
    { "ho", "\033[H" }, { "up", "\033[A" }, { "nd", "\033[C" }, { "so", "\033[7m" }, { "se", "\033[m" },
};

int tgetent(char *bp, const char *name) { return 1; }
int tgetflag(const char *id) { return !strcmp(id, "bs"); }
int tgetnum(const char *id) { return !strcmp(id, "co") ? 80 : !strcmp(id, "li") ? 24 : -1; }

char *tgetstr(const char *id, char **area)
{
    for (int i = 0; i < (int)(sizeof cap / sizeof *cap); i++)
        if (!strcmp(id, cap[i][0])) { char *s = *area; strcpy(s, cap[i][1]); *area += strlen(s) + 1; return s; }
    return NULL;
}

char *tgoto(const char *cm, int col, int row)
{
    static char b[16];
    snprintf(b, sizeof b, "\033[%d;%dH", row + 1, col + 1);
    return b;
}

int tputs(const char *s, int n, int (*pc)(int)) { while (*s) pc((unsigned char)*s++); return 0; }
