/* X11 frontend: one window. Screen rows 0 (messages) and 23 (status) are
 * text; rows 1-22 are the map, tiled (square cells, nearest-neighbour,
 * RVIP step 4). Text the game or vt_menu writes over the map goes into a
 * box in the normal font on top of the tiles.
 * Env: HACK_TILESET (dawn | nethack, default dawn), HACK_TILES (sheet
 * path, overrides), HACK_CELL (map cell px, 18), HACK_XFT (font, Menlo),
 * HACK_TEXT (text px, 14), HACK_POS "x,y". */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "vt.h"
#include <time.h>

static Display *dpy;
static Window win;
static Pixmap pix;
static GC gc;
static XftDraw *xd;
static XftFont *fnt, *mapfnt;
static XftColor xfg, xbg;
static unsigned long fg, bg;
static int cols, rows, tw, th, cell = 18, cy, cx;
static unsigned char *sheet;
static int sheet_w, sheet_h;
static XImage *img;
static int drawn[24][80];    /* what each map cell shows now (tile/glyph key) */
static int bx0, by0, bx1, by1 = -1;   /* text box over the map, in screen cells */

static int rowy(int y) { return y < MAP0 ? 0 : y <= MAP1 ? th + (y - MAP0) * cell : th + (MAP1 - MAP0 + 1) * cell + (y - MAP1 - 1) * th; }

static void load_sheet(void)
{
    const char *p = getenv("HACK_TILES"), *s = getenv("HACK_TILESET");
    FILE *f = fopen(p ? p : s && !strcmp(s, "nethack") ? "port/tiles.rgba" : "port/tiles-dawn.rgba", "rb");
    unsigned char h[8];
    if (!f || fread(h, 1, 8, f) != 8) { if (f) fclose(f); return; }
    sheet_w = h[0] | h[1] << 8 | h[2] << 16 | h[3] << 24;
    sheet_h = h[4] | h[5] << 8 | h[6] << 16 | h[7] << 24;
    sheet = malloc((size_t)sheet_w * sheet_h * 4);
    if (fread(sheet, 4, (size_t)sheet_w * sheet_h, f) != (size_t)sheet_w * sheet_h) { free(sheet); sheet = NULL; }
    fclose(f);
}

static XftFont *font(double px, double stretch)
{
    const char *fam = getenv("HACK_XFT");
    FcMatrix m;
    FcMatrixInit(&m);
    m.xx = stretch;
    return XftFontOpen(dpy, DefaultScreen(dpy), XFT_FAMILY, XftTypeString, fam ? fam : "Menlo",
                       XFT_PIXEL_SIZE, XftTypeDouble, px, XFT_MATRIX, XftTypeMatrix, &m, NULL);
}

static unsigned long rgb(int r, int g, int b)
{
    XColor c = { 0, r * 257, g * 257, b * 257, 0, 0 };
    XAllocColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &c);
    return c.pixel;
}

void be_init(int c, int r)
{
    XSizeHints h;
    XGlyphInfo gi;
    XRenderColor c1 = { 0xd7d7, 0xd7d7, 0xd7d7, 0xffff }, c0 = { 0, 0, 0, 0xffff };
    const char *e;
    int scr, x = 0, y = 0, w, ht;
    Visual *vis;
    Colormap cm;

    if (!(dpy = XOpenDisplay(NULL))) { fprintf(stderr, "hack: no X display\n"); exit(1); }
    scr = DefaultScreen(dpy); vis = DefaultVisual(dpy, scr); cm = DefaultColormap(dpy, scr);
    if ((e = getenv("HACK_CELL"))) cell = atoi(e);
    fnt = font((e = getenv("HACK_TEXT")) ? atof(e) : 14.0, 1);
    XftTextExtents8(dpy, fnt, (FcChar8 *)"M", 1, &gi);
    tw = gi.xOff; th = fnt->ascent + fnt->descent;
    mapfnt = font(cell * 0.78, 1);     /* stray glyphs on the map (rays, thrown things) */
    XftColorAllocValue(dpy, vis, cm, &c1, &xfg);
    XftColorAllocValue(dpy, vis, cm, &c0, &xbg);
    fg = rgb(215, 215, 215); bg = rgb(0, 0, 0);
    cols = c; rows = r;
    w = c * cell; ht = rowy(r);
    if ((e = getenv("HACK_POS"))) sscanf(e, "%d,%d", &x, &y);
    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), x, y, w, ht, 0, 0, 0);
    h.flags = PPosition | USPosition | PMinSize | PMaxSize;
    h.x = x; h.y = y;
    h.min_width = h.max_width = w;
    h.min_height = h.max_height = ht;
    XSetWMNormalHints(dpy, win, &h);
    XStoreName(dpy, win, "Hack");
    XSelectInput(dpy, win, KeyPressMask | ExposureMask);
    pix = XCreatePixmap(dpy, win, w, ht, DefaultDepth(dpy, scr));
    xd = XftDrawCreate(dpy, pix, vis, cm);
    gc = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc, bg);
    XFillRectangle(dpy, pix, gc, 0, 0, w, ht);
    img = XCreateImage(dpy, vis, DefaultDepth(dpy, scr), ZPixmap, 0, malloc(cell * cell * 4), cell, cell, 32, 0);
    load_sheet();
    memset(drawn, -1, sizeof drawn);
    XMapWindow(dpy, win);
    XFlush(dpy);
}

static void blend(int t, int first)
{
    int tx = (t % 32) * 16, ty = (t / 32) * 16;
    for (int py = 0; py < cell; py++)
        for (int px = 0; px < cell; px++) {
            unsigned char *s = sheet + ((ty + py * 16 / cell) * sheet_w + tx + px * 16 / cell) * 4;   /* nearest-neighbour */
            unsigned long o = first ? 0 : XGetPixel(img, px, py);
            int a = s[3], r = o >> 16 & 255, g = o >> 8 & 255, b = o & 255;
            r = (s[0] * a + r * (255 - a)) / 255;
            g = (s[1] * a + g * (255 - a)) / 255;
            b = (s[2] * a + b * (255 - a)) / 255;
            XPutPixel(img, px, py, (unsigned long)r << 16 | g << 8 | b);
        }
}

static void glyph(XftFont *f, int px, int py, int w, int h, chtype ch)
{
    FcChar8 c = ch & A_CHARTEXT;
    int inv = !!(ch & A_STANDOUT);
    XGlyphInfo gi;
    XSetForeground(dpy, gc, inv ? fg : bg);
    XFillRectangle(dpy, pix, gc, px, py, w, h);
    if (c == ' ') return;
    XftTextExtents8(dpy, f, &c, 1, &gi);
    XftDrawString8(xd, inv ? &xbg : &xfg, f, px + (w - gi.xOff) / 2, py + (h - f->ascent - f->descent) / 2 + f->ascent, &c, 1);
}

/* One map cell: tile (+ floor under it), a glyph, or blank. key = what it shows. */
static void map_cell(int y, int x, int tile, int und, int ch)
{
    int key = tile >= 0 ? tile << 12 | (und + 1) : ch == ' ' ? 0 : -2 - (int)ch;
    if (drawn[y][x] == key) return;
    drawn[y][x] = key;
    if (tile >= 0 && sheet && (tile / 32 + 1) * 16 <= sheet_h) {
        blend(und >= 0 ? und : tile, 1);
        if (und >= 0) blend(tile, 0);
        XPutImage(dpy, pix, gc, img, 0, 0, x * cell, rowy(y), cell, cell);
    } else glyph(mapfnt, x * cell, rowy(y), cell, cell, ch == ' ' ? ' ' : ch);
}

void be_frame(chtype s[][80])
{
    int y, x, b[4];

    for (y = 0; y < rows; y++) if (y < MAP0 || y > MAP1)
        for (x = 0; x < cols; x++) glyph(fnt, x * tw, rowy(y), tw, th, s[y][x]);
    vt_map(s, b, map_cell);
    by0 = b[0]; by1 = b[1]; bx0 = b[2]; bx1 = b[3];
    if (by1 >= 0) {     /* the text box, one text cell of padding */
        int px = bx0 * cell, py = rowy(by0), w = (bx1 - bx0 + 3) * tw, h = (by1 - by0 + 1) * th + tw;
        if (px + w > cols * cell) px = cols * cell - w;
        XSetForeground(dpy, gc, bg);
        XFillRectangle(dpy, pix, gc, px, py, w, h);
        XSetForeground(dpy, gc, fg);
        XDrawRectangle(dpy, pix, gc, px, py, w - 1, h - 1);
        for (y = by0; y <= by1; y++)
            for (x = bx0; x <= bx1; x++) glyph(fnt, px + (x - bx0 + 1) * tw, py + tw / 2 + (y - by0) * th, tw, th, s[y][x]);
        for (y = MAP0; y <= MAP1; y++)      /* cells under the box get redrawn when it goes */
            for (x = 0; x < cols; x++)
                if (rowy(y) < py + h && rowy(y) + cell > py && x * cell < px + w && x * cell + cell > px) drawn[y][x] = -1;
    }
}

void be_msg(const char *s, int fold) { }
void be_cursor(int y, int x) { cy = y; cx = x; }

void be_flush(void)
{
    int inbox = by1 >= 0 && cy >= MAP0 && cy <= MAP1;
    XCopyArea(dpy, pix, win, gc, 0, 0, cols * cell, rowy(rows), 0, 0);
    XSetForeground(dpy, gc, fg);
    if (cy < MAP0 || cy > MAP1)
        XFillRectangle(dpy, win, gc, cx * tw, rowy(cy) + th - 2, tw, 2);
    else if (!inbox)
        XDrawRectangle(dpy, win, gc, cx * cell, rowy(cy), cell - 1, cell - 1);
    XFlush(dpy);
}

int be_menu;
static int keycode(XKeyEvent *ev)
{
    char buf[8];
    KeySym ks;
    int n = XLookupString(ev, buf, sizeof buf, &ks, NULL);
    if (be_menu) switch (ks) {   /* menus: arrows must not look like item letters */
    case XK_Up: case XK_KP_Up: return BE_UP;
    case XK_Down: case XK_KP_Down: return BE_DOWN;
    case XK_Left: case XK_KP_Left: return BE_LEFT;
    case XK_Right: case XK_KP_Right: return BE_RIGHT;
    }
    switch (ks) {
    /* arrows = keypad digits: Hack moves with hjkl, so send those */
    case XK_Left: case XK_KP_Left: return 'h';
    case XK_Right: case XK_KP_Right: return 'l';
    case XK_Up: case XK_KP_Up: return 'k';
    case XK_Down: case XK_KP_Down: return 'j';
    case XK_Home: case XK_KP_Home: return 'y';
    case XK_Prior: case XK_KP_Prior: return 'u';
    case XK_End: case XK_KP_End: return 'b';
    case XK_Next: case XK_KP_Next: return 'n';
    case XK_KP_Begin: return '.';
    case XK_KP_Enter: case XK_Return: return '\n';   /* curses nl() mode */
    case XK_BackSpace: case XK_Delete: return '\b';
    case XK_KP_Add: return '+';
    case XK_KP_Subtract: return '-';
    case XK_KP_Multiply: return '*';
    case XK_KP_Divide: return '/';
    case XK_KP_Decimal: case XK_KP_Delete: return '.';
    case XK_KP_Insert: return '0';
    }
        return n == 1 ? (unsigned char)buf[0] : -1;
}

extern int rl_at_prompt;
void rl_autosave(void);
int be_getkey(int wait)
{
    XEvent ev;
    static int test = -1;
    if (test < 0) test = !!getenv("HACK_AUTOSAVE");
    if (test && rl_at_prompt) { rl_at_prompt = 0; rl_autosave(); rl_at_prompt = 1; }
    for (;;) {
        if (!wait && !XPending(dpy)) return -1;
        XNextEvent(dpy, &ev);
        if (ev.type == Expose) be_flush();
        else if (ev.type == KeyPress) {
            int k = keycode(&ev.xkey);
            if (k >= 0) return k;
        }
    }
}

void be_sleep(int ms)
{
    struct timespec t = { ms / 1000, ms % 1000 * 1000000L };
    nanosleep(&t, NULL);
}
void be_end(void) { if (dpy) XCloseDisplay(dpy); dpy = NULL; }
