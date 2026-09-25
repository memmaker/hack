/* Web build, force-included (-include): musl's stdin/stdout are const, so
 * the game's terminal I/O goes to port/vt.c's cookie streams by name. */
#include <stdio.h>
extern FILE *hk_out, *hk_in;
#undef stdout
#undef stdin
#undef putchar
#undef getchar
#define stdout hk_out
#define stdin hk_in
#define putchar(c) putc(c, hk_out)
#define getchar() getc(hk_in)
#define printf(...) fprintf(hk_out, __VA_ARGS__)
#define vprintf(f, a) vfprintf(hk_out, f, a)
#define puts(s) (fputs(s, hk_out), putc('\n', hk_out))
