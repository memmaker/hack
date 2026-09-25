# Hack (restoHack) — RVIP

## RVIP progress
- Stage 1 done (2026-09-25).
- Stage 2 done (2026-09-25). Next: stage 3 (Enter menu + inventory).
  - `port/rl.c`: `x` = explore, `<`/`>` off stairs walk to the known ones.
    Hook: `rhack()` (src/hack.cmd.c) calls `rl_parse()` instead of `parse()`;
    `rl_parse` returns one step key per turn while a mode runs.
  - Known grid = own `known[][]` OR'd from `levl[x][y].seen` each step (Hack
    un-sees dark room floor when you leave). Reset when a mode starts on a
    new `dlevel`. Frontier cells stay targets until stood on.
  - Stops: new message (`vt_msgs`, counts chars written on row 0 in vt.c),
    any key (`be_getkey(0)`), no movement, level change; explore also on
    any visible non-tame monster. Stairs walk ignores monsters (their
    attacks print messages). Diagonal moves through doors skipped.
  - Help (`help`, `hh`) updated. `<` on level 1 stairs escapes (original).
  - Cosmetic, later: long "--More--" lines wrap to row 1; end screen
    text wraps oddly (vt has no auto-margin fix).
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
