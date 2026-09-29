# Hack (restoHack) — handover

Port of Hack 1.0.3 via restoHack (all RVIP stages done): web build live at
https://ruzzoli.de/roguelikes/hack/, plus a native X11 tiles build. Procedure:
`~/Games/rvip-tools/RVIP.md` (Hack is the "O-Hack" worked example). Sister
port: `~/Games/nethack13d` (shares the `port/` design).

## Source and repo
- Upstream Critlist/restoHack @ `0bd798b` (full clone, remote `origin`). Repo
  https://github.com/memmaker/hack (remote `memmaker`, branch `master`); README
  header + compare link.
- Case O (termcap, not curses): `port/vt.c` swaps stdin/stdout for `funopen()`
  streams and runs stdout through a small VT100 interpreter into an 80×24
  buffer. Needs `TERM=vt100`.

## Build and deploy
- Native first (the web build needs `build/hack.onames.h`):
  `cmake -S . -B build -DHACKDIR_OVERRIDE=$PWD/save && cmake --build build`
  (`HACK_X11` option, default ON). Run `./play.sh [nethack]` (seeds `save/`
  from `build/hackdir`); Desktop shortcut `~/Desktop/Games/Roguelikes/Hack.app`.
  X11 env knobs: `HACK_TILESET=nethack|dawn`, `HACK_TILES`, `HACK_CELL`,
  `HACK_TEXT`, `HACK_POS`, `HACK_AUTOSAVE=1` (autosave at every prompt).
- ASan: `-DCMAKE_C_FLAGS=-fsanitize=address`.
- Web: `sh web/build.sh` → `web/dist`; `sh web/deploy.sh`. Shared page code
  from the parent folder: `../rvip-wm.js`, `../rvip-app.js`.

## File map
- `port/rl.c`: `x` = explore, `<`/`>` off stairs walk to known ones (stops on
  arrival), Enter = `cmd_menu()` (parses the `Commands:` lines of `help`),
  `i` = `inv_menu()`/`item_menu()`/`act()`. Hook: `rhack()` (src/hack.cmd.c)
  calls `rl_parse()` instead of `parse()`.
- `port/tiles.c` `tile_for()` (game state decides the tile), `port/mktiles.py` →
  `port/tiles-dawn*.png/.rgba`, `port/tiles.png/.rgba`, `port/tilemap.h`;
  credits `port/TILES-CREDITS.txt`. DawnLike default (NetHack for gaps),
  NetHack set switchable.
- `port/vt.c`/`vt.h`, `port/be_x11.c`, `port/be_web.c` (`js_key(rl_at_prompt)`,
  `be_page()` pager pop-up), `port/termcap-web.c`, `port/web-inc/` (`hkio.h`
  force-included: fopencookie streams).
- `web/hack.js` (draws, `hk.page` scrollable pager), `web/index.html`,
  `web/make-help.py`. In-game help: root `help`/`hh` (build copies them over the
  stale `hackdir/` copies).

## Facts and gotchas
- Web saves live in this game's own IndexedDB folder (`RvipApp.dir`); player
  name and tile set are stored there too (no localStorage).
- Autosave at the command prompt: `dosave0(1)` + `dorecover`, write the file
  back; reset worn pointers first; `toplin=2` + `redotoplin()`; restore
  moonphase/luck. Save format 3 stores the object shuffle as indices (v2 saves
  still load).
- Rebuilding invalidates saves ("Saved level is out of date"): upstream.
- `S` saves at once and exits the process. Page makes a dummy `/this.program`
  (gethdate).
- `page_more()` (src/hack.pager.c, `__EMSCRIPTEN__`) hands long texts to
  `be_page()`; falls back to the terminal pager before the map exists.
- No sound (upstream has none).

## Open
- Web: end screen not shown before the "Play again" overlay.
- Native: long `--More--` lines wrap to row 1; end screen text wraps oddly.
- Shift+letter drop only for a–z; counts like "d7a" lose 2/5/8 (numpad keys)
  at the list.
