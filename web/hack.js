/*
 * Hack in the browser: draws the screen port/be_web.c sends (Module.hk):
 * the map rows as 16x16 tiles (DawnLike or NetHack, switchable) in a map
 * window, messages and status in text windows, text over the map in a pop-up.
 * Keyboard, saves in IndexedDB (IDBFS, /hack). Loaded before hack-core.js.
 * Structure copied from ~/Games/omega/web/omega.js.
 */
(function () {
	'use strict';

	var DIR = '/hack', SAVES = DIR + '/save', SEED = '/seed';
	var KEEP = { 'web-layout.json': 1, help: 1, hh: 1, data: 1, rumors: 1, news: 1, perm: 1, record: 1, save: 1 };
	var FONT = '"DejaVu Sans Mono", Menlo, Consolas, "Liberation Mono", monospace';
	var FG = '#d7d7d7', A_STANDOUT = 0x10000, MAP0 = 1, MAP1 = 22;
	/* Tiles button cycles NetHack -> DawnLike -> DawnLike|a (animated) -> None (text) */
	var SETS = { nethack: ['tiles.png', 'NetHack'], dawn: ['tiles-dawn.png', 'DawnLike'],
		dawna: ['tiles-dawn.png', 'DawnLike|a', 'tiles-dawn-1.png'], none: [null, 'None'] }, ORDER = ['nethack', 'dawn', 'dawna', 'none'];
	/* arrows: 0x101.. (be_web.c makes them hjkl, or cursor keys in menus) */
	var KEYS = { ArrowUp: 0x101, ArrowDown: 0x102, ArrowLeft: 0x103, ArrowRight: 0x104, Home: 121, PageUp: 117,
		End: 98, PageDown: 110, Clear: 46, Enter: 10, Escape: 27, Backspace: 8, Delete: 8, Tab: 9 };

	var events = [], lastSave = 0;
	var wantSaveFlag = true;   /* restoring deletes the save: write it back at the first prompt */
	var scr = null, cells = null, box = [99, -1, 99, -1], cur = { y: 0, x: 0 };
	var cv, ctx, cell = 18, auto = true;
	var dpr = Math.max(1, Math.min(3, window.devicePixelRatio || 1));
	var set = 'dawn', sheets = {}, rowfg = [], invCache = '';
	function tilesReady() { var i = sheets[SETS[set][0]]; return !!(SETS[set][0] && i && i.complete && i.naturalWidth); }

	function $(id) { return document.getElementById(id); }
	function store(k, v) { try { if (v === undefined) return localStorage.getItem(k); localStorage.setItem(k, v); } catch (e) { } return null; }

	/* ---------- drawing ---------- */
	/* Windows (as ~/Games/rogue3.6/web): the map rows as tiles in their own
	 * window, scrolled to keep the hero in view; row 0 plus the message
	 * history in Messages, row 23 in Status, text over the map in a pop-up. */
	var ROWS = MAP1 - MAP0 + 1, GUT = 6, TITLE = 22, log = [], hero = { y: 0, x: 0, lev: -1 }, off = { x: 0, y: 0 };
	var L = { cell: 0, wm: null }, LAYOUT = DIR + '/web-layout.json', wm = null;
	function esc(t) { return t.replace(/[&<>]/g, function (c) { return '&' + (c === '&' ? 'amp' : c === '<' ? 'lt' : 'gt') + ';'; }); }
	/* screen row y, columns x0..x1 as HTML (standout, cursor) */
	function rowHtml(y, x0, x1, cursor) {
		var h = rowHtml1(y, x0, x1, cursor);
		return rowfg[y] ? '<span style="color:' + rowfg[y] + '">' + h + '</span>' : h;     /* the game's colour for a menu row */
	}
	function rowHtml1(y, x0, x1, cursor) {
		var h = '', so = false, x, v, c, on;
		for (x = x0; x <= x1; x++) {
			v = scr[y * 80 + x]; c = String.fromCharCode((v & 255) || 32);
			on = !!(v & A_STANDOUT) !== (cursor && cur.y === y && cur.x === x);
			if (on !== so) { h += on ? '<span class="so">' : '</span>'; so = on; }
			h += esc(c);
		}
		return (so ? h + '</span>' : h).replace(/\s+$/, '');
	}
	function rowText(y) { var s = '', x; for (x = 0; x < 80; x++) s += String.fromCharCode((scr[y * 80 + x] & 255) || 32); return s; }
	function measure() {
		var w = 80 * cell, h = ROWS * cell;
		cv.width = w * dpr; cv.height = h * dpr;
		cv.style.width = w + 'px'; cv.style.height = h + 'px';
		ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
		ctx.imageSmoothingEnabled = false;     /* nearest-neighbour tiles */
		ctx.textBaseline = 'middle'; ctx.textAlign = 'center';
	}
	function fit() {       /* biggest cell that shows the whole map in its window */
		var b = $('map'), best = 8;
		for (var c = 8; c <= 64; c++) if (80 * c <= b.clientWidth && ROWS * c <= b.clientHeight) best = c;
		return best;
	}
	function face(map) { var n = map ? L.mapFace : L.face; return n ? '"' + n + '", ' + FONT : FONT; }
	function glyph(c, px, py, w, h, size) {
		ctx.fillStyle = '#000'; ctx.fillRect(px, py, w, h);
		if (c <= 32) return;
		ctx.font = (L.mapFace ? '' : 'bold ') + size + 'px ' + face(true);
		ctx.fillStyle = FG;
		ctx.fillText(String.fromCharCode(c), px + w / 2, py + h / 2 + 1);
	}
	function tile(t, px, py) {
		var img = frame && anim && sheets[SETS[set][2]] || sheets[SETS[set][0]];
		if (tilesReady()) ctx.drawImage(img, (t % 32) * 16, (t >> 5) * 16, 16, 16, px, py, cell, cell);
	}
	function draw() {
		if (!scr) return;
		var y, x, k, py, lines = [];
		ctx.fillStyle = '#000'; ctx.fillRect(0, 0, 80 * cell, ROWS * cell);
		for (y = MAP0; y <= MAP1; y++)
			for (x = 0; x < 80; x++) drawCell(y, x);
		var pop = $('pop'), inbox = box[1] >= 0 && cur.y >= box[0] && cur.y <= box[1];
		if (page !== null) placePage();
		else if (box[1] >= 0) {
			for (y = box[0]; y <= box[1]; y++) lines.push(rowHtml(y, box[2], box[3], inbox));
			pop.innerHTML = lines.join('\n');
			pop.hidden = false;
			RvipWM.popup(pop, { x: box[2] * cell + Math.min(0, -off.x) });
		} else pop.hidden = true;
		drawCursor();
		/* messages: history (dim), then the live top line with the cursor */
		var ml = $('msg'), body = ml.parentNode;
		ml.innerHTML = log.map(function (t) { return '<span class="old">' + esc(t) + '</span>'; }).join('\n') +
			(log.length ? '\n' : '') + rowHtml(0, 0, 79, true);
		body.scrollTop = body.scrollHeight;     /* the newest message stays in view */
		$('stat').innerHTML = rowHtml(23, 0, 79, true);
		RvipWM.prompt.text(rowText(0));      /* the prompt line over the map */
		scrollMap(false);
	}
	/* pager text (be_page: long "More info?" entries, help) in the pop-up, scrolled by keys or the wheel */
	var page = null;
	function placePage() {     /* RvipWM.popup keeps it inside the map body (max-height: overflow scrolls) */
		var pop = $('pop'), st = pop.hidden ? 0 : pop.scrollTop;
		pop.hidden = false;
		RvipWM.popup(pop, { x: 0 });
		pop.scrollTop = st;
	}
	function showPage(t) {
		var pop = $('pop');
		pop.innerHTML = esc(t.replace(/\s+$/, '')) +
			'\n\n<span class="old">↑↓ j k scroll · PgUp PgDn · Space next page · Esc q Enter close</span>';
		pop.hidden = true;
		placePage();
	}
	function pageKey(k) {
		var pop = $('pop'), line = Math.ceil(parseFloat(getComputedStyle(pop).lineHeight) || 18), step = Math.max(line, pop.clientHeight - 2 * line);
		var atEnd = pop.scrollTop + pop.clientHeight >= pop.scrollHeight - 2;
		switch (k) {
		case 27: case 113: case 10: case 13: return 1;                      /* Esc q Enter */
		case 0x101: case 107: pop.scrollTop -= line; break;                 /* up, k */
		case 0x102: case 106: pop.scrollTop += line; break;                 /* down, j */
		case 117: case 45: pop.scrollTop -= step; break;                    /* PgUp (u), - */
		case 110: pop.scrollTop += step; break;                             /* PgDn (n) */
		case 32: if (atEnd) return 1; pop.scrollTop += step; break;         /* Space: next page, closes at the end */
		case 121: case 60: pop.scrollTop = 0; break;                        /* Home (y), < */
		case 98: case 62: pop.scrollTop = pop.scrollHeight; break;          /* End (b), > */
		}
		return 0;
	}
	function drawCell(y, x) {
		var k = cells[y * 80 + x], py = (y - MAP0) * cell;
		if (!tilesReady()) glyph(scr[y * 80 + x] & 255, x * cell, py, cell, cell, Math.round(cell * 0.78));   /* None: text */
		else if (k > 0) {
			var t = k >> 12, u = (k & 0xfff) - 1;
			ctx.fillStyle = '#000'; ctx.fillRect(x * cell, py, cell, cell);
			if (u >= 0) tile(u, x * cell, py);
			tile(t, x * cell, py);
		} else if (k < 0) glyph(-2 - k, x * cell, py, cell, cell, Math.round(cell * 0.78));
	}
	function drawCursor() {
		var inbox = box[1] >= 0 && cur.y >= box[0] && cur.y <= box[1];
		if (cur.y >= MAP0 && cur.y <= MAP1 && !inbox && !(cur.y === hero.y && cur.x === hero.x)) {   /* no cursor on the hero */
			ctx.strokeStyle = FG; ctx.lineWidth = 1;
			ctx.strokeRect(cur.x * cell + 0.5, (cur.y - MAP0) * cell + 0.5, cell - 1, cell - 1);
		}
	}
	/* animation ("DawnLike|a"): every 500 ms the map draws from the frame-1
	 * sheet and back; only cells whose sprite (or floor under it) differs
	 * between the two sheets (anim[slot], found once) are redrawn */
	var frame = 0, anim = null;
	function findAnim() {
		var a = sheets[SETS.dawna[0]], b = sheets[SETS.dawna[2]];
		if (!a || !b || !a.naturalWidth || !b.naturalWidth) return;
		var W = a.naturalWidth, H = a.naturalHeight, c = document.createElement('canvas'), g;
		c.width = W; c.height = H; g = c.getContext('2d');
		g.drawImage(a, 0, 0); var da = g.getImageData(0, 0, W, H).data;
		g.clearRect(0, 0, W, H); g.drawImage(b, 0, 0); var db = g.getImageData(0, 0, W, H).data;
		var n = (W / 16) * (H / 16);
		anim = new Uint8Array(n);
		for (var s = 0; s < n; s++)
			for (var y = 0, x0 = (s % (W / 16)) * 16, y0 = ((s / (W / 16)) | 0) * 16; y < 16 && !anim[s]; y++)
				for (var i = ((y0 + y) * W + x0) * 4, e = i + 64; i < e; i++) if (da[i] !== db[i]) { anim[s] = 1; break; }
	}
	setInterval(function () {
		if (!cells || set !== 'dawna' || !tilesReady() || document.hidden) return;
		if (!anim) findAnim();
		if (!anim) return;
		frame ^= 1;
		for (var i = MAP0 * 80; i < (MAP1 + 1) * 80; i++) {
			var k = cells[i];
			if (k > 0 && (anim[k >> 12] || anim[(k & 0xfff) - 1])) drawCell((i / 80) | 0, i % 80);
		}
		drawCursor();
	}, 500);
	/* keep the hero in the middle half of the map window; recentre when it leaves it */
	function scrollMap() {
		off = RvipWM.center(cv, (hero.x + 0.5) * cell, (hero.y - MAP0 + 0.5) * cell, 80 * cell, ROWS * cell);
	}
	var rects = {};
	function saveLayout() {
		try { Module.FS.writeFile(LAYOUT, JSON.stringify(L)); app.sync(); } catch (e) { console.warn('layout not saved', e); }
	}
	function fonts() {
		['msg', 'stat', 'inv', 'vis'].forEach(function (id) { $(id).style.fontFamily = id !== 'vis' || L.face ? face(false) : ''; });
		$('pop').style.fontSize = RvipWM.fontSize('msg') + 'px'; $('pop').style.fontFamily = face(false);   /* pop-up text = the message font */
	}
	/* windows: the shared tiling window manager (rvip-wm.js, RVIP.md 5b) */
	function makeWM() {
		try { var s = JSON.parse(Module.FS.readFile(LAYOUT, { encoding: 'utf8' })); if (s) L = { cell: s.cell | 0, wm: s.wm, face: s.face || '', mapFace: s.mapFace || '' }; } catch (e) { }
		if (s && L.wm && !L.wm.fs) L.wm.fs = s.fs || (s.font ? { msg: s.font, stat: s.font, inv: s.font, vis: s.vis } : undefined);   /* old layout: sizes move to the WM */
		if (L.cell >= 8 && L.cell <= 64) { cell = L.cell; auto = false; }
		var H = $('game').clientHeight || 600, line = Math.ceil(RvipWM.fontSize('msg') * 1.4) + 6;
		wm = RvipWM({
			area: $('game'), menu: $('btn-layout'),
			wins: [{ id: 'map', title: 'Map' }, { id: 'msg', title: 'Messages' }, { id: 'stat', title: 'Status' }, { id: 'inv', title: 'Inventory' }, { id: 'vis', title: 'Visible' }],
			multi: { d: 'v', r: 0.7, a: 'map', b: { d: 'h', r: 0.4, a: { d: 'v', r: 0.7, a: 'msg', b: 'stat' }, b: { d: 'h', r: 0.5, a: 'inv', b: 'vis' } } },
			single: { d: 'v', r: line / H, a: 'msg', b: { d: 'v', r: 1 - line / (H - line), a: 'map', b: 'stat' } },
			state: L.wm,
			save: function (st) { L.wm = st; saveLayout(); },
			layout: function (r) { rects = r; fonts(); if (auto) { cell = fit(); measure(); } scrollMap(true); draw(); },
			zoom: { map: function (size, d) { zoom(2 * d); }, msg: function () { fonts(); } },   /* A- / A+ on the map zooms the map */
			onReset: function () { auto = true; L.cell = 0; L.wm = wm.state(); fonts(); cell = fit(); measure(); scrollMap(true); draw(); saveLayout(); }
		});
		wm.apply();
		renderMapSel();
		$('sel-font').value = L.face || '';
		loadFace(L.face); loadFace(L.mapFace);
	}
	/* fonts: the index page's fonts/*.woff (web/build.sh lists them in fonts.json) */
	var mapSel = document.createElement('select');
	mapSel.title = 'Map font (text mode)';
	mapSel.innerHTML = '<option value="">Default font</option>';
	mapSel.addEventListener('pointerdown', function (e) { e.stopPropagation(); });   /* not a window drag */
	function renderMapSel() {
		var bs = document.querySelector('#t-map .wm-btns');
		if (bs && mapSel.parentNode !== bs) bs.insertBefore(mapSel, bs.firstChild);
		mapSel.hidden = !!SETS[set][0];
		mapSel.value = L.mapFace || '';
	}
	function loadFace(n, now) {
		var redraw = function () { fonts(); draw(); };
		if (!n) { if (now) redraw(); return; }
		var ff = new FontFace(n, 'url(../fonts/' + n + '.woff)');
		ff.load().then(function () { document.fonts.add(ff); redraw(); }).catch(function () { app.status('Could not load the font ' + n + '.', true); });
	}
	/* a tile as a 16px CSS sprite (Inventory and Visible icons) */
	function icon(t) {
		if (!tilesReady() || !(t >= 0)) return null;
		var s = document.createElement('i');
		s.className = 'wm-ic';
		s.style.cssText = 'image-rendering:pixelated;background:url(' + SETS[set][0] + ') -' + (t % 32) * 16 + 'px -' + ((t / 32) | 0) * 16 + 'px';
		return s;
	}
	function renderInv() {
		var pre = $('inv');
		pre.innerHTML = '';
		invCache.split('\n').forEach(function (l, i, a) {
			if (!l) return;
			var f = l.split('\t'), c = f[0], t = +f[1], g = f[2], txt = f.slice(3).join('\t');
			var d = document.createElement('div'), ic = icon(t);
			d.className = 'inv-row';
			if (c) d.style.color = c;
			/* "a) <icon> name" with tiles, "a) ! name" in text mode */
			d.appendChild(document.createTextNode(txt.slice(0, 2) + ' '));
			if (ic) d.appendChild(ic); else d.appendChild(document.createTextNode(g));
			d.appendChild(document.createTextNode(' ' + txt.slice(4)));
			pre.appendChild(d);
		});
	}
	function zoom(d) {
		auto = false;
		cell = Math.max(8, Math.min(64, cell + d));
		L.cell = cell; saveLayout();
		measure(); scrollMap(true); draw();
	}
	function setTiles(s) {
		set = SETS[s] ? s : 'dawn';
		store('hack-tileset', set);
		$('btn-tiles').textContent = 'Tiles: ' + SETS[set][1];
		frame = 0;
		[SETS[set][0], SETS[set][2]].forEach(function (src) {
			if (!src || sheets[src]) return;
			var img = sheets[src] = new Image(), mine = set;
			img.onload = function () { if (set === mine) retile(); };   /* a late sheet can't turn tiles back on after None */
			img.src = src;
		});
		retile();
	}
	function retile() {        /* the tile set changed: the map and both lists at once */
		draw();
		renderInv();
		var v = $('vis'), c = v._vis; v._vis = null; if (c != null) RvipWM.visible(v, c, icon);
		if (L.wm) renderMapSel();
	}

	var hk = {
		frame: function (sp, cp, y0, y1, x0, x1, hy, hx, lev) {
			scr = Module.HEAPU32.slice(sp >> 2, (sp >> 2) + 1920);
			cells = Module.HEAP32.slice(cp >> 2, (cp >> 2) + 1920);
			box = [y0, y1, x0, x1];
			if ($('game').hidden) {
				$('game').hidden = false;
				measure(); makeWM();
			}
			var moved = hy !== hero.y || hx !== hero.x, lv = lev !== hero.lev;
			hero.y = hy; hero.x = hx; hero.lev = lev;
			if (moved || lv) scrollMap(lv);
			draw();
		},
		/* inventory lines "<colour>\t<text>", coloured by the game */
		/* inventory lines "<colour>\t<tile>\t<glyph>\t<text>", from the game */
		inv: function (t) {
			if (t === invCache) return;
			invCache = t;
			renderInv();
		},
		vis: function (s) { RvipWM.visible($('vis'), s, icon); },
		rowfg: function (s) { rowfg = s.split('\n'); },
		/* fold: the game folded a repeat into "message (xN)", replacing the last line */
		msg: function (t, fold) {
			t = t.replace(/\s*\n\s*/g, ' ').trim();
			if (!t) return;
			if (fold && log.length) log[log.length - 1] = t;
			else { log.push(t); if (log.length > 200) log.shift(); }
		},
		/* be_page: "" = can the pop-up be used (a map window exists), text = show it, null = close */
		page: function (t) {
			if (t === null) { page = null; $('pop').hidden = true; draw(); return 1; }
			if ($('game').hidden || !scr) return 0;
			if (t) { page = t; showPage(t); }
			return 1;
		},
		pageKey: pageKey,
		cursor: function (y, x) { cur.y = y; cur.x = x; draw(); },
		key: function (atCmd) { RvipWM.prompt.wait(atCmd); return events.length ? events.shift() : -1; },
		/* autosave at most every 2 s, and when the page is hidden */
		wantSave: function () {
			var now = performance.now();
			if (!wantSaveFlag || (now - lastSave < 2000 && !document.hidden)) return 0;
			wantSaveFlag = false; lastSave = now;
			setTimeout(app.sync, 0);
			return 1;
		},
		end: function (saved) {
			app.running = false;
			return new Promise(function (done) {
				app.sync(function () {
					$('overlay-msg').textContent = saved ? 'Your game has been saved. Play again to continue it.' : 'The game is over.';
					$('overlay').hidden = false;
					done();
				});
			});
		}
	};

	/* ---------- input ---------- */
	function onKey(e) {
		if (!app.running || e.isComposing || e.metaKey) return;
		var k = e.key, c;
		if (e.code === 'NumpadEnter') c = 10;
		else if (KEYS[k] !== undefined) c = KEYS[k];
		else if (k.length === 1) {
			c = k.charCodeAt(0);
			if (e.ctrlKey && !e.altKey) {
				var u = k.toUpperCase().charCodeAt(0);
				if (u >= 65 && u <= 90) c = u & 0x1f; else return;
			}
			if (c > 126) return;
		}
		else return;
		events.push(c);
		wantSaveFlag = true;
		e.preventDefault();
	}

	/* ---------- saves: IndexedDB (IDBFS), Export / Import / New game in rvip-app.js ---------- */
	/* the save file is save/<uid><name>; the page passes -u <name> to find it again */
	function saveFile() {
		try {
			return Module.FS.readdir(SAVES).filter(function (f) { return f[0] !== '.' && !/\.tmp$/.test(f); })[0] || null;
		} catch (e) { return null; }
	}
	function clearSaves() { var f; while ((f = saveFile())) Module.FS.unlink(SAVES + '/' + f); }
	var app = RvipApp({
		name: 'hack',
		save: function () { var f = saveFile(); return f ? SAVES + '/' + f : null; },
		clear: clearSaves,
		put: function (file, data) {
			var name = file.name.replace(/^\d+/, '').replace(/[^\w-]/g, '');
			if (!name) return 'A Hack save file is named like 501Name (user number, then the character name).';
			Module.FS.writeFile(SAVES + '/0' + name, data);
		},
		helpText: 'Press ? in the game for its own help.'
	});

	/* ---------- startup ---------- */
	window.Module = {
		hk: hk,
		arguments: [],
		preRun: [function () {
			var FS = Module.FS;
			Module.ENV.TERM = 'vt100';
			Module.ENV.HOME = DIR;
			Module.ENV.USER = 'player';
			/* gethdate() stats argv[0]; saves older than it count as outdated */
			FS.writeFile('/this.program', ''); FS.utime('/this.program', 0, 0);
			FS.mkdirTree(DIR);
			FS.mount(Module.IDBFS, {}, DIR);
			Module.addRunDependency('idbfs');
			FS.syncfs(true, function (err) {
				if (err) app.status('Could not read saved games from IndexedDB (' + err + '). Saving may not work in this browser mode.', true);
				/* game files from the build; level files a closed page left behind go */
				FS.readdir(DIR).forEach(function (f) {
					if (f[0] === '.' || KEEP[f] || /^bones/.test(f)) return;
					try { FS.unlink(DIR + '/' + f); } catch (e) { }
				});
				FS.readdir(SEED).forEach(function (f) { if (f[0] !== '.') FS.writeFile(DIR + '/' + f, FS.readFile(SEED + '/' + f)); });
				['perm', 'record'].forEach(function (f) { try { FS.stat(DIR + '/' + f); } catch (e) { FS.writeFile(DIR + '/' + f, ''); } });
				try { FS.mkdir(SAVES); } catch (e) { }
				var s = saveFile();
				if (s) Module.arguments.push('-u', s.replace(/^\d+/, ''));
				Module.removeRunDependency('idbfs');
			});
		}],
		onRuntimeInitialized: function () { app.running = true; app.status(''); },
		print: function (s) { console.log(s); },
		printErr: function (s) { console.warn(s); },
		setStatus: function (s) { if (s && !app.running) app.status(s.replace(/\(\d+\/\d+\)/, '').trim() || 'Loading…'); },
		onAbort: function (what) { app.crashed(what); }
	};
	document.addEventListener('visibilitychange', function () { if (document.hidden) wantSaveFlag = true; });

	window.addEventListener('resize', function () { if (wm) wm.apply(); });
	document.addEventListener('keydown', onKey);
	document.addEventListener('DOMContentLoaded', function () {
		cv = document.querySelector('#game canvas');
		ctx = cv.getContext('2d');
		setTiles(store('hack-tileset'));
		$('btn-tiles').onclick = function () { setTiles(ORDER[(ORDER.indexOf(set) + 1) % ORDER.length]); };
		RvipWM.dropdown($('btn-file'), $('menu-file'));
		fetch('fonts.json').then(function (r) { return r.json(); }).then(function (list) {
			[[$('sel-font'), 'face'], [mapSel, 'mapFace']].forEach(function (a) {
				list.forEach(function (n) { var o = document.createElement('option'); o.value = n; o.textContent = n.replace(/^Web(Plus|437)_/, '').replace(/_/g, ' '); a[0].appendChild(o); });
				a[0].value = L[a[1]] || '';
			});
		}).catch(function () { });
		[[$('sel-font'), 'face'], [mapSel, 'mapFace']].forEach(function (a) {
			a[0].onchange = function () { L[a[1]] = this.value; saveLayout(); loadFace(this.value, true); this.blur(); };
		});
		$('btn-restart').onclick = function () { location.reload(); };
		document.querySelectorAll('button').forEach(function (b) {
			b.addEventListener('mousedown', function (e) { e.preventDefault(); });
		});
	});
})();
