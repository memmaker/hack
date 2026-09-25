/* RVIP: auto-explore (x) and walk-to-stairs (< >) for Hack.
 * rhack() calls rl_parse() instead of parse(). */
#include "hack.h"
#include "vt.h"

extern char *parse(void), sdir[];
extern schar xdir[], ydir[];
extern int canseemon(struct monst *);

static char mode;                   /* 0, 'x', '<' or '>' */
static char known[COLNO][ROWNO], stood[COLNO][ROWNO];
static int lvl = -1, lastx, lasty, lastmsg;

static int ok(int x, int y) { return x >= 0 && x < COLNO && y >= 0 && y < ROWNO; }

static int pass(int x, int y)
{
    struct trap *t;
    return ok(x, y) && known[x][y] && ACCESSIBLE(levl[x][y].typ) &&
           !((t = t_at(x, y)) && t->tseen);
}

static int target(int x, int y)
{
    int i;
    if (mode == '<') return x == xupstair && y == yupstair;
    if (mode == '>') return x == xdnstair && y == ydnstair;
    if (stood[x][y]) return 0;
    if (o_at(x, y) || g_at(x, y)) return 1;
    for (i = 0; i < 8; i++)
        if (ok(x + xdir[i], y + ydir[i]) && !known[x + xdir[i]][y + ydir[i]]) return 1;
    return 0;
}

/* BFS from the player; returns the key for the first step, 0 if none. */
static int path(void)
{
    static short q[COLNO * ROWNO];
    static schar from[COLNO][ROWNO];
    int h = 0, t = 0, i, x, y, nx, ny;

    memset(from, -1, sizeof from);
    from[u.ux][u.uy] = 8;
    q[t++] = u.ux * ROWNO + u.uy;
    while (h < t) {
        x = q[h] / ROWNO, y = q[h++] % ROWNO;
        if ((x != u.ux || y != u.uy) && target(x, y)) {
            while (from[x][y] != 8) {   /* walk back to the first step */
                i = from[x][y];
                nx = x - xdir[i], ny = y - ydir[i];
                if (nx == u.ux && ny == u.uy) return sdir[i];
                x = nx, y = ny;
            }
        }
        for (i = 0; i < 8; i++) {
            nx = x + xdir[i], ny = y + ydir[i];
            if (!pass(nx, ny) || from[nx][ny] != -1) continue;
            if (xdir[i] && ydir[i] && (levl[x][y].typ == DOOR || levl[nx][ny].typ == DOOR)) continue;
            from[nx][ny] = i;
            q[t++] = nx * ROWNO + ny;
        }
    }
    return 0;
}

static int step(void)
{
    struct monst *m;
    int x, y;

    if (dlevel != lvl) return 0;
    for (x = 0; x < COLNO; x++)
        for (y = 0; y < ROWNO; y++) known[x][y] |= levl[x][y].seen;
    known[u.ux][u.uy] = stood[u.ux][u.uy] = 1;
    if (mode == '<' && u.ux == xupstair && u.uy == yupstair) return mode = 0, '<';
    if (mode == '>' && u.ux == xdnstair && u.uy == ydnstair) return mode = 0, '>';
    if (vt_msgs != lastmsg || be_getkey(0) >= 0) return 0;
    if (lastx == u.ux && lasty == u.uy) return 0;   /* last step did not move */
    for (m = mode == 'x' ? fmon : 0; m; m = m->nmon)   /* stairs walk: messages stop it */
        if (!m->mtame && !m->mimic && canseemon(m)) return 0;
    lastx = u.ux, lasty = u.uy;
    if (!(x = path()) && mode == 'x') pline("Nothing left to explore. Search (s) for hidden doors.");
    return x;
}

static void start(char c)
{
    mode = c, lastx = lasty = -1, lastmsg = vt_msgs;
    if (dlevel != lvl) memset(known, 0, sizeof known), memset(stood, 0, sizeof stood), lvl = dlevel;
}

char *rl_parse(void)
{
    static char b[2];
    char *cmd;

    for (;;) {
        if (mode) {
            if ((b[0] = step())) {
                lastmsg = vt_msgs;
                flags.move = 1;
                return b;
            }
            mode = 0;
        }
        cmd = parse();
        if (cmd[1] || multi) return cmd;
        if (cmd[0] == 'x') start('x');
        else if ((cmd[0] == '<' && (u.ux != xupstair || u.uy != yupstair) && levl[xupstair][yupstair].seen) ||
                 (cmd[0] == '>' && (u.ux != xdnstair || u.uy != ydnstair) && levl[xdnstair][ydnstair].seen))
            start(cmd[0]);
        else return cmd;
    }
}
