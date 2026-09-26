/* Browser frontend (RVIP step 7): web/hack.js draws the screen and the
 * tiles (Module.hk); input waits with Asyncify. The save file is an
 * autosave while playing (written at the command prompt when the page asks)
 * and removed when the game ends unless the player saved with S. */
#include <emscripten.h>
#include <stdio.h>
#include <unistd.h>
#include "hack.h"
#include "vt.h"

/* Run report (roguelikes-index/server/CONTRACT.md): fire-and-forget GET,
   never throws, offline just fails silently. Negative ints are omitted. */
EM_JS(void, js_beacon, (const char *g, const char *ev, const char *name, const char *killer, int depth, int score, int turns, int lvl), {
    try {
        var p = [['g', UTF8ToString(g)], ['ev', UTF8ToString(ev)], ['name', name ? UTF8ToString(name) : ''],
                 ['killer', killer ? UTF8ToString(killer) : ''], ['depth', depth], ['score', score], ['turns', turns], ['lvl', lvl]];
        var q = p.filter(function (a) { return a[1] !== '' && !(a[1] < 0); })
                 .map(function (a) { return a[0] + '=' + encodeURIComponent(a[1]); }).join('&');
        if (window.RvipWM && RvipWM.report) RvipWM.report(q); else fetch('/roguelikes/beacon?' + q, { keepalive: true, mode: 'no-cors' }).catch(function () {});
    } catch (e) {}
});
extern char plname[];
void be_run_end(const char *ev, const char *killer)
{
    js_beacon("hack", ev, plname, killer, dlevel, (int)u.urexp, (int)moves, u.ulevel);
}

extern int rl_at_prompt, rl_saved;
extern char SAVEF[];
void rl_autosave(void);

EM_JS(void, js_frame, (unsigned *scr, int *cell, int y0, int y1, int x0, int x1, int hy, int hx, int lev), { Module.hk.frame(scr, cell, y0, y1, x0, x1, hy, hx, lev); });
/* Angband's colour for an object class (RVIP W0: colours come from the game) */
static const char *css(int olet)
{
    switch (olet) {
    case AMULET_SYM: return "#ff9000";
    case FOOD_SYM:   return "#d09050";
    case WEAPON_SYM: return "#b0b0b8";
    case TOOL_SYM:   return "#c0c0c0";
    case ARMOR_SYM:  return "#a07040";
    case POTION_SYM: return "#40a0ff";
    case SCROLL_SYM: return "#ffffff";
    case WAND_SYM:   return "#40d040";
    case RING_SYM:   return "#ff4040";
    case GEM_SYM:    return "#ff60ff";
    }
    return "";
}
EM_JS(void, js_inv, (const char *s), { Module.hk.inv(UTF8ToString(s)); });
EM_JS(void, js_vis, (const char *s), { Module.hk.vis(UTF8ToString(s)); });
EM_JS(void, be_msg, (const char *s, int fold), { Module.hk.msg(UTF8ToString(s), fold); });
EM_JS(void, js_cursor, (int y, int x), { Module.hk.cursor(y, x); });
EM_JS(int, js_key, (int at_cmd), { return Module.hk.key(at_cmd); });
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
    {   /* Inventory window: "<css colour>\ta - item" per line */
        static char inv[52 * 90];
        char *p = inv;
        *p = 0;
        for (struct obj *o = invent; o; o = o->nobj)
            p += snprintf(p, inv + sizeof inv - p, "%s\t%c - %.80s\n", css(o->olet), o->invlet, doname(o));
        js_inv(inv);
    }
    {   /* Visible window: "M<glyph><name>" / "I<glyph><name>" (rvip-wm.js) */
        static char vis[8192];
        char *p = vis, *e = vis + sizeof vis - 100;
        *p = 0;
        for (struct monst *m = fmon; m && p < e; m = m->nmon)
            if (!m->mimic && canseemon(m)) p += sprintf(p, "M%c%.60s\n", m->data->mlet, m->data->mname);
        for (struct obj *o = fobj; o && p < e; o = o->nobj)
            if (cansee(o->ox, o->oy)) p += sprintf(p, "I%c%.80s\t%s\n", o->olet, doname(o), css(o->olet));
        for (struct gold *g = fgold; g && p < e; g = g->ngold)
            if (cansee(g->gx, g->gy)) p += sprintf(p, "I$%ld gold pieces\t#ffe040\n", (long)g->amount);
        js_vis(vis);
    }
    js_frame(&s[0][0], &cells[0][0], b[0], b[1], b[2], b[3], u.uy + MAP0, u.ux, dlevel);
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
        if ((k = js_key(rl_at_prompt)) >= 0) {
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
