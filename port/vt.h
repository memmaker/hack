/* Screen buffer between Hack's termcap output and the X11 window. */
#include <stdio.h>
typedef unsigned int chtype;
#define A_CHARTEXT 0xff
#define A_COLOR 0x7f00
#define A_STANDOUT 0x10000
void be_init(int c, int r);
void be_put(int y, int x, chtype ch);
void be_cursor(int y, int x);
void be_flush(void);
int be_getkey(int wait);
void be_sleep(int ms);
void be_end(void);
