# Hack (restoHack) — RVIP

## RVIP progress
- Stage 1 done (2026-09-25).
- Stage 3 done (2026-09-25). Next: stage 4 (tiles).
  - All in `port/rl.c` (+ `vt_menu`/`vt_push` in `port/vt.c`). Enter menu
    `cmd_menu()` parses the `Commands:` lines of `help` ("\t<key>\t<text>",
    `^X` = Ctrl); chosen key is returned from `rl_parse` as the command.
  - `i` = `inv_menu()`, item menu `item_menu()`, action `act()`: sets
    `rl_obj` (getobj() in src/hack.invent.c returns it once, cleared at the
    next `rl_parse`), `R` with two rings queues l/r via `vt_push`. List
    reopens (`reopen`) unless `threat()`. Item prompts: getobj's first
    `readchar()` → `rl_pick(lets)` (cursor list, skipped when keys queued).
  - `vt_menu(items,n,cur)`: box sized to content, scrolls past 22 rows,
    returns cursor, key in `vt_menukey`; `be_menu` makes arrows BE_UP..
    so j/k stay item letters. Numpad 8/2/5/+/-/*/0/4/6.
  - Ceilings: Shift+letter drop only for a–z; counts in "d7a" lose 2/5/8
    (numpad keys) at the list; no floor/equipment lists (Hack has none).
  - Tested live + 500 random keys under ASan: clean.
- Stage 2 done (2026-09-25).
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
