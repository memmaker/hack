/* Browser frontend (RVIP step 7): web/hack.js draws the screen and the
 * tiles (Module.hk); input waits with Asyncify. The save file is an
 * autosave while playing (written at the command prompt when the page asks)
 * and removed when the game ends unless the player saved with S. */
#include <emscripten.h>
#include <stdio.h>
#include <unistd.h>
#include "vt.h"

extern int rl_at_prompt, rl_saved;
extern char SAVEF[];
void rl_autosave(void);

EM_JS(void, js_frame, (unsigned *scr, int *cell, int y0, int y1, int x0, int x1), { Module.hk.frame(scr, cell, y0, y1, x0, x1); });
EM_JS(void, js_cursor, (int y, int x), { Module.hk.cursor(y, x); });
EM_JS(int, js_key, (void), { return Module.hk.key(); });
EM_JS(int, js_want_save, (void), { return Module.hk.wantSave(); });
EM_ASYNC_JS(void, js_end, (int saved), { await Module.hk.end(saved); });   /* before exit closes IndexedDB */

static int cells[24][80];
int be_menu;

static void at_exit(void)
{
    if (!rl_saved) unlink(SAVEF);     /* died or quit: the game is over */
    js_end(rl_saved);
}

static void cell(int y, int x, int tile, int und, int ch)
{
    cells[y][x] = tile >= 0 ? tile << 12 | (und + 1) : ch == ' ' ? 0 : -2 - ch;
}

void be_init(int c, int r) { atexit(at_exit); }
void be_frame(chtype s[][80])
{
    int b[4];
    vt_map(s, b, cell);
    js_frame(&s[0][0], &cells[0][0], b[0], b[1], b[2], b[3]);
}
void be_cursor(int y, int x) { js_cursor(y, x); }
void be_flush(void) { }
void be_sleep(int ms) { emscripten_sleep(ms); }
void be_end(void) { }

int be_getkey(int wait)
{
    static double last;
    int k;
    for (;;) {
        if (rl_at_prompt && js_want_save()) {
            rl_at_prompt = 0;
            rl_autosave();
            rl_at_prompt = 1;
        }
        if ((k = js_key()) >= 0) {
            if (be_menu) switch (k) {   /* menus: arrows must not look like item letters */
            case 0x101: case 0x102: case 0x103: case 0x104: return k;
            }
            switch (k) {
            case 0x101: return 'k';
            case 0x102: return 'j';
            case 0x103: return 'h';
            case 0x104: return 'l';
            }
            return k;
        }
        if (!wait) {                /* polling (explore): let the page paint */
            if (emscripten_get_now() - last > 50) {
                last = emscripten_get_now();
                emscripten_sleep(0);
            }
            return -1;
        }
        emscripten_sleep(10);
    }
}

int hk_usleep(unsigned us) { emscripten_sleep(us / 1000); return 0; }   /* -Dusleep: animations */
