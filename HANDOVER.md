# Hack (restoHack) — RVIP

## RVIP progress
- Stage 1 done (2026-09-25). Next: stage 2 (explore + stairs).
- Folder `~/Games/hack`, upstream Critlist/restoHack @ 0bd798b (full clone).
- Case O (termcap, not curses): `port/vt.c` swaps stdin/stdout for
  `funopen()` streams in a constructor; stdout runs through a small VT100
  interpreter into an 80×24 buffer drawn by `port/be_x11.c` (Omega's). No
  game source edits. Needs `TERM=vt100`. Arrows → hjkl.
- Build: `cmake -S . -B <dir> [-DHACKDIR_OVERRIDE=<abs>] && cmake --build <dir>`
  (`HACK_X11` option, default ON). Hackdir (saves, record) = `<dir>/hackdir`
  unless overridden. Save name = `<uid><name>` in `hackdir/save`.
- ASan run (`-DCMAKE_C_FLAGS=-fsanitize=address`): 150 random keys, save
  (`S` saves at once, no prompt), restore: clean, HP kept.
- Quirks: `S` exits the process (window closes). Upstream termcap warnings.
