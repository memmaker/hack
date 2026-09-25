/* Screen buffer between Hack's termcap output and the X11 window. */
#include <stdio.h>
typedef unsigned int chtype;
#define A_CHARTEXT 0xff
#define A_COLOR 0x7f00
#define A_STANDOUT 0x10000
void be_init(int c, int r);
void be_frame(chtype s[][80]);                   /* whole 80x24 screen */
int tile_for(int sy, int sx, int ch, int *under); /* port/tiles.c */
int map_char(int sy, int sx);
void be_cursor(int y, int x);
void be_flush(void);
int be_getkey(int wait);
void be_sleep(int ms);
void be_end(void);
extern int vt_msgs;   /* printable chars written on the message line */
extern int be_menu;    /* set while a menu reads keys: arrows -> BE_* */
enum { BE_UP = 0x101, BE_DOWN, BE_LEFT, BE_RIGHT };
void vt_push(const char *keys);
int vt_queued(void);                  /* keys the game reads next */
int vt_menu(const char **item, int n, int cur);  /* box; returns cursor, key in vt_menukey */
extern int vt_menukey;                           /* key that ended vt_menu */
