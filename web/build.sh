#!/bin/sh
# Build Hack for the browser (Emscripten + Asyncify) into web/dist;
# port/be_web.c + web/hack.js draw the screen and tiles. Deploy: web/deploy.sh.
set -e
cd "$(dirname "$0")/.."
OUT=web/dist SEED=web/seed
rm -rf "$OUT" "$SEED" && mkdir -p "$OUT" "$SEED"
cp help hh data rumors hackdir/news "$SEED/"
[ -f build/hack.onames.h ] || { echo "build natively first (hack.onames.h)"; exit 1; }
SRCS=$(sed -n '/^set(HACK_SOURCES/,/^)/p' CMakeLists.txt | grep -o 'src/[a-zA-Z_.]*\.c')
emcc -O2 $EMFLAGS -std=gnu99 -w -D_GNU_SOURCE -D__linux__ -Dusleep=hk_usleep -DHACKDIR='"/hack"' -DSAVE_VERSION='"1.1.1"' -DHAVE_PRAGMA_PACK=1 \
	-Iport/web-inc -include port/web-inc/hkio.h -Isrc -I. -Ibuild \
	$SRCS port/vt.c port/rl.c port/tiles.c port/termcap-web.c port/be_web.c \
	--preload-file "$SEED@/seed" -o "$OUT/hack-core.js" \
	-sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sSTACK_SIZE=1048576 \
	-sALLOW_MEMORY_GROWTH -sEXIT_RUNTIME=1 -sINITIAL_MEMORY=32MB \
	-sEXPORTED_FUNCTIONS=_main \
	-sEXPORTED_RUNTIME_METHODS=FS,IDBFS,ENV,HEAPU32,HEAP32,addRunDependency,removeRunDependency \
	-sFORCE_FILESYSTEM -lidbfs.js -sENVIRONMENT=web
rm -rf "$SEED"
cp web/index.html web/hack.js web/rvip-wm.js port/tiles-dawn.png port/tiles.png "$OUT/"
python3 web/make-help.py > "$OUT/help.html"
ls -la "$OUT"
