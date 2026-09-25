/* Hack writes VT100 sequences (TERM=vt100) to stdout and reads stdin.
 * Both are swapped for funopen() streams before main(): output goes
 * through a small VT100 interpreter into an 80x24 buffer, input comes
 * from the X11 window. Game sources stay untouched. */
#include "vt.h"
#include <stdlib.h>
#include <string.h>

#define COLS 80
#define ROWS 24
static chtype scr[ROWS][COLS];
int vt_msgs;
static int cy, cx, so, esc, np, par[4];

static void put(int c)
{
    if (esc == 1) { esc = c == '[' ? 2 : 0; np = 0; par[0] = par[1] = 0; return; }
    if (esc == 2) {
        if (c >= '0' && c <= '9') { par[np] = par[np] * 10 + c - '0'; return; }
        if (c == ';') { if (np < 3) par[++np] = 0; return; }
        if (c == '?') return;
        esc = 0;
        switch (c) {
        case 'H': case 'f': cy = par[0] ? par[0] - 1 : 0; cx = np && par[1] ? par[1] - 1 : 0; break;
        case 'A': cy -= par[0] ? par[0] : 1; break;
        case 'B': cy += par[0] ? par[0] : 1; break;
        case 'C': cx += par[0] ? par[0] : 1; break;
        case 'D': cx -= par[0] ? par[0] : 1; break;
        case 'K': for (int x = cx; x < COLS; x++) scr[cy][x] = ' '; break;
        case 'J':
            if (par[0] == 2) { cy = cx = 0; }
            for (int y = cy; y < ROWS; y++)
                for (int x = y == cy ? cx : 0; x < COLS; x++) scr[y][x] = ' ';
            break;
        case 'm': so = par[0] == 7 || par[1] == 7; break;
        }
        goto clip;
    }
    switch (c) {
    case 033: esc = 1; return;
    case '\r': cx = 0; break;
    case '\n': cx = 0;   /* tty ONLCR: newline is CR LF */
        if (cy < ROWS - 1) cy++; else { memmove(scr, scr[1], sizeof scr[0] * (ROWS - 1)); for (int x = 0; x < COLS; x++) scr[ROWS - 1][x] = ' '; } break;
    case '\b': if (cx) cx--; break;
    case '\t': cx = (cx + 8) & ~7; break;
    case 7: case 0: case 016: case 017: break;
    default:
        if (c < ' ') break;
        if (cx >= COLS) { cx = 0; put('\n'); }
        if (!cy) vt_msgs++;
        scr[cy][cx++] = (unsigned char)c | (so ? A_STANDOUT : 0);
        return;
    }
clip:
    if (cy < 0) cy = 0; if (cy >= ROWS) cy = ROWS - 1;
    if (cx < 0) cx = 0; if (cx > COLS) cx = COLS;
}

static void present(void)
{
    be_frame(scr);
    be_cursor(cy, cx < COLS ? cx : COLS - 1);
    be_flush();
}

static int out(void *c, const char *b, int n) { for (int i = 0; i < n; i++) put((unsigned char)b[i]); return n; }

static char queue[64];

int vt_queued(void) { return *queue; }
void vt_push(const char *k) { strncat(queue, k, sizeof queue - strlen(queue) - 1); }

/* Pop-up box sized to its items over the screen, cursor on item cur.
 * Up/down/8/2 move, any other key returns the cursor index and leaves
 * the key in vt_menukey. */
int vt_menukey;
int vt_menu(const char **item, int n, int cur)
{
    int w = 0, h = n < ROWS - 2 ? n : ROWS - 2, top = 0, y0, x0, k;
    chtype save[ROWS][COLS];

    for (int i = 0; i < n; i++) if ((int)strlen(item[i]) > w) w = strlen(item[i]);
    if (w > COLS - 4) w = COLS - 4;
    y0 = (ROWS - h - 2) / 2, x0 = (COLS - w - 4) / 2;
    fflush(stdout);
    memcpy(save, scr, sizeof scr);
    be_menu = 1;
    for (;;) {
        if (cur < top) top = cur;
        if (cur >= top + h) top = cur - h + 1;
        for (int y = 0; y < h + 2; y++)
            for (int x = 0; x < w + 4; x++) {
                int c = y == 0 || y == h + 1 ? (x == 0 || x == w + 3 ? '+' : '-') : x == 0 || x == w + 3 ? '|' : ' ';
                const char *t = y && y <= h ? item[top + y - 1] : "";
                if (y && y <= h && x >= 2 && x - 2 < (int)strlen(t) && x - 2 < w) c = (unsigned char)t[x - 2];
                scr[y0 + y][x0 + x] = c | (y && y <= h && top + y - 1 == cur && x && x < w + 3 ? A_STANDOUT : 0);
            }
        if (top) scr[y0][x0 + w + 2] = '^';
        if (top + h < n) scr[y0 + h + 1][x0 + w + 2] = 'v';
        present();
        be_cursor(y0 + 1 + cur - top, x0 + 1);
        be_flush();
        while ((k = be_getkey(1)) < 0) ;
        if (k == BE_UP || k == '8') cur = (cur + n - 1) % n;
        else if (k == BE_DOWN || k == '2') cur = (cur + 1) % n;
        else break;
    }
    be_menu = 0;
    memcpy(scr, save, sizeof scr);
    present();
    vt_menukey = k;
    return cur;
}

static int in(void *c, char *b, int n)
{
    int k;
    if (*queue) { b[0] = queue[0]; memmove(queue, queue + 1, strlen(queue)); return 1; }
    fflush(stdout);
    present();
    while ((k = be_getkey(1)) < 0) ;
    b[0] = k;
    return 1;
}

__attribute__((constructor)) static void vt_start(void)
{
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++) scr[y][x] = ' ';
    be_init(COLS, ROWS);
    stdout = funopen(NULL, NULL, out, NULL, NULL);
    stdin = funopen(NULL, in, NULL, NULL, NULL);
    setvbuf(stdout, NULL, _IOFBF, 8192);
    setvbuf(stdin, NULL, _IONBF, 0);
}
