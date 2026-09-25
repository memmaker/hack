/* Which tile shows a map cell (RVIP step 4). The screen only has
 * characters; the game's own state (levl[][].scrsym, monsters, objects)
 * says what is really there. Screen (row, col) = map (y + 1, x - 1). */
#include "hack.h"
#include <ctype.h>
#include "vt.h"
#include "tilemap.h"

extern char pl_character[];

static int slot(const char *key)
{
    for (int i = 0; i < (int)(sizeof tile_key / sizeof *tile_key); i++)
        if (!strcmp(tile_key[i], key)) return i;
    return -1;
}

static int keyed(const char *pfx, const char *s)
{
    char k[80];
    snprintf(k, sizeof k, "%s%s", pfx, s);
    return slot(k);
}

#define T(name) (t_##name >= 0 ? t_##name : (t_##name = slot("T:" #name)))
static int t_floor = -1, t_corr = -1, t_hwall = -1, t_vwall = -1, t_tl = -1, t_tr = -1, t_bl = -1, t_br = -1,
           t_hdoor = -1, t_vdoor = -1, t_up = -1, t_down = -1, t_pool = -1, t_trap = -1, t_bear = -1, t_arrow = -1, t_dart = -1,
           t_trapdoor = -1, t_teleport = -1, t_sleep = -1, t_pierc = -1, t_gold = -1;

static int sym(int x, int y) { return x < 1 || x >= COLNO || y < 0 || y >= ROWNO ? ' ' : levl[x][y].scrsym; }

/* What the game shows at map (x, y). */
static int expect(int x, int y)
{
    if (x == u.ux && y == u.uy && !Invisible && !u.uswallow && u.usym) return u.usym;
    struct rm *r = &levl[x][y];
    struct monst *m;
    if (!r->seen && !r->new) {      /* mklev fills scrsym long before it is shown */
        for (m = fmon; m; m = m->nmon)
            if (m->mdispl && m->mdx == x && m->mdy == y) break;
        if (!m) return ' ';
    }
    return r->scrsym ? r->scrsym : ' ';
}

static int door(int x, int y) { return sym(x - 1, y) == '-' || sym(x + 1, y) == '-' ? T(hdoor) : T(vdoor); }

static int under(int x, int y)
{
    switch (levl[x][y].typ) {
    case CORR: return T(corr);
    case DOOR: case LDOOR: return door(x, y);
    }
    return T(floor);
}

static int terrain(int x, int y, int ch)
{
    struct trap *t;
    switch (ch) {
    case '.': return T(floor);
    case CORR_SYM: return T(corr);
    case '|': return T(vwall);
    case '+': return door(x, y);
    case '<': return T(up);
    case '>': return T(down);
    case POOL_SYM: return T(pool);
    case '$': return T(gold);
    case '-':
        if (sym(x, y + 1) == '|' || sym(x, y + 1) == '+') return sym(x + 1, y) == '-' ? T(tl) : T(tr);
        if (sym(x, y - 1) == '|' || sym(x, y - 1) == '+') return sym(x + 1, y) == '-' ? T(bl) : T(br);
        return T(hwall);
    case '^':
        if (!(t = t_at(x, y))) return T(trap);
        switch (t->ttyp) {
        case BEAR_TRAP: return T(bear);
        case ARROW_TRAP: return T(arrow);
        case DART_TRAP: return T(dart);
        case TRAPDOOR: return T(trapdoor);
        case TELEP_TRAP: return T(teleport);
        case SLP_GAS_TRAP: return T(sleep);
        case PIERC: return T(pierc);
        }
        return T(trap);
    }
    return -1;
}

static int obj_tile(struct obj *o)
{
    struct objclass *c = &objects[o->otyp];
    char s[2] = { o->olet, 0 };
    if (c->oc_descr) { char k[64]; snprintf(k, sizeof k, "%s%s", s, c->oc_descr); return keyed("D:", k); }
    return c->oc_name ? keyed("O:", c->oc_name) : -1;
}

/* Tile slot for screen cell (sy, sx) showing ch; *un = floor under it or -1.
   -1: blank, or the screen shows something the game doesn't (text, rays). */
int tile_for(int sy, int sx, int ch, int *un)
{
    int x = sx + 1, y = sy - 1, t;
    struct monst *m;
    struct obj *o;
    char s[2] = { ch, 0 };

    *un = -1;
    if (x < 1 || x >= COLNO || y < 0 || y >= ROWNO || ch == ' ' || ch != expect(x, y)) return -1;
    if (x == u.ux && y == u.uy && ch == '@') { *un = under(x, y); return keyed("P:", (char[]){ pl_character[0], 0 }); }
    if (ch == '~') { *un = under(x, y); return slot("M:long worm tail"); }
    if ((m = m_at(x, y)) && m->mdispl && m->data->mlet == ch) { *un = under(x, y); return keyed("M:", m->data->mname); }
    if ((t = terrain(x, y, ch)) >= 0 && ch != '$') return t;
    *un = under(x, y);
    if (ch == '$') return T(gold);
    for (o = fobj; o; o = o->nobj)
        if (o->ox == x && o->oy == y && o->olet == ch && (t = obj_tile(o)) >= 0) return t;
    switch (ch) {   /* remembered object, or a mimic */
    case '0': return slot("O:heavy iron ball");
    case '_': return slot("O:iron chain");
    case '`': return slot("O:enormous rock");
    }
    if ((t = keyed("C:", s)) >= 0) return t;
    *un = -1;
    return -1;
}

int vt_cooked(void) { return !flags.cbreak; }

/* The character the game shows at screen cell (sy, sx), ' ' outside the map. */
int map_char(int sy, int sx)
{
    int x = sx + 1, y = sy - 1;
    return x < 1 || x >= COLNO || y < 0 || y >= ROWNO ? ' ' : expect(x, y);
}

void vt_map(chtype s[][80], int box[4], void (*cell)(int y, int x, int tile, int und, int ch))
{
    static char diff[24][80];
    int y, x, n, alnum, un, t, text[24] = { 0 }, by0 = 99, by1 = -1, bx0 = 99, bx1 = -1;

    /* tiles for what the game shows; cells where the screen differs are text or rays */
    for (y = MAP0; y <= MAP1; y++) {
        for (x = n = alnum = 0; x < 80; x++) {
            int ch = s[y][x] & A_CHARTEXT, e = map_char(y, x);
            diff[y][x] = ch != e || (s[y][x] & A_STANDOUT);
            if (diff[y][x]) { n++; alnum |= isalnum(ch); }
        }
        text[y] = n >= 3 && alnum;
    }
    for (y = MAP0; y <= MAP1; y++)      /* a box's border rows have no letters */
        if (!text[y] && ((y > MAP0 && text[y - 1] == 1) || (y < MAP1 && text[y + 1] == 1)))
            for (x = 0; x < 80; x++) if (diff[y][x]) { text[y] = 2; break; }
    for (y = MAP0; y <= MAP1; y++) if (text[y])
        for (x = 0; x < 80; x++) if (diff[y][x]) {
            if (y < by0) by0 = y;
            if (y > by1) by1 = y;
            if (x < bx0) bx0 = x;
            if (x > bx1) bx1 = x;
        }
    for (y = MAP0; y <= MAP1; y++)
        for (x = 0; x < 80; x++) {
            int inbox = y >= by0 && y <= by1 && x >= bx0 && x <= bx1, ch = s[y][x] & A_CHARTEXT;
            if (diff[y][x] && !inbox && ch != ' ') { cell(y, x, -1, -1, ch); continue; }
            if (inbox || diff[y][x]) ch = map_char(y, x);
            t = tile_for(y, x, ch, &un);
            cell(y, x, t, un, ch);
        }
    box[0] = by0; box[1] = by1; box[2] = bx0; box[3] = bx1;
}
