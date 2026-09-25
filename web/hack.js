/*
 * Hack in the browser: draws the screen port/be_web.c sends (Module.hk):
 * text rows 0 and 23, the map rows as 16x16 tiles (DawnLike or NetHack,
 * switchable), text over the map in a box, as the X11 version does.
 * Keyboard, saves in IndexedDB (IDBFS, /hack). Loaded before hack-core.js.
 * Structure copied from ~/Games/omega/web/omega.js.
 */
(function () {
	'use strict';

	var DIR = '/hack', SAVES = DIR + '/save', SEED = '/seed';
	var KEEP = { help: 1, hh: 1, data: 1, rumors: 1, news: 1, perm: 1, record: 1, save: 1 };
	var FONT = '"DejaVu Sans Mono", Menlo, Consolas, "Liberation Mono", monospace';
	var FG = '#d7d7d7', A_STANDOUT = 0x10000, MAP0 = 1, MAP1 = 22;
	var SETS = { dawn: ['tiles-dawn.png', 'DawnLike'], nethack: ['tiles.png', 'NetHack'] };
	/* arrows: 0x101.. (be_web.c makes them hjkl, or cursor keys in menus) */
	var KEYS = { ArrowUp: 0x101, ArrowDown: 0x102, ArrowLeft: 0x103, ArrowRight: 0x104, Home: 121, PageUp: 117,
		End: 98, PageDown: 110, Clear: 46, Enter: 10, Escape: 27, Backspace: 8, Delete: 8, Tab: 9 };

	var events = [], running = false, lastSave = 0;
	var wantSaveFlag = true;   /* restoring deletes the save: write it back at the first prompt */
	var scr = null, cells = null, box = [99, -1, 99, -1], cur = { y: 0, x: 0 };
	var cv, ctx, cell = 18, tw = 9, th = 16, fpx = 14, auto = true;
	var dpr = Math.max(1, Math.min(3, window.devicePixelRatio || 1));
	var set = 'dawn', sheets = {};

	function $(id) { return document.getElementById(id); }
	function status(msg, isError) {
		var s = $('status');
		s.textContent = msg; s.hidden = !msg; s.classList.toggle('error', !!isError);
	}
	function store(k, v) { try { if (v === undefined) return localStorage.getItem(k); localStorage.setItem(k, v); } catch (e) { } return null; }

	/* ---------- drawing ---------- */
	function rowy(y) { return y < MAP0 ? 0 : y <= MAP1 ? th + (y - MAP0) * cell : th + (MAP1 - MAP0 + 1) * cell + (y - MAP1 - 1) * th; }
	function metrics(c) {
		var p = Math.min(22, Math.floor(c / 1.25));   /* a text line fits a map cell: boxes over the map fit */
		ctx.font = p + 'px ' + FONT;
		return { p: p, tw: Math.ceil(ctx.measureText('M').width), th: Math.ceil(p * 1.25) };
	}
	function measure() {
		var m = metrics(cell);
		fpx = m.p; tw = m.tw; th = m.th;
		var w = 80 * cell, h = rowy(24);
		cv.width = w * dpr; cv.height = h * dpr;
		cv.style.width = w + 'px'; cv.style.height = h + 'px';
		ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
		ctx.imageSmoothingEnabled = false;     /* nearest-neighbour tiles */
		ctx.textBaseline = 'middle'; ctx.textAlign = 'center';
	}
	function fit() {
		var g = $('game'), best = 8;
		for (var c = 8; c <= 64; c++) {
			var m = metrics(c);
			if (80 * c <= g.clientWidth && 22 * c + 2 * m.th <= g.clientHeight) best = c;
		}
		return best;
	}
	function glyph(c, px, py, w, h, inv, size) {
		ctx.fillStyle = inv ? FG : '#000'; ctx.fillRect(px, py, w, h);
		if (c <= 32) return;
		ctx.font = size + 'px ' + FONT;
		ctx.fillStyle = inv ? '#000' : FG;
		ctx.fillText(String.fromCharCode(c), px + w / 2, py + h / 2 + 1);
	}
	function tile(t, px, py) {
		var img = sheets[set];
		if (img && img.complete) ctx.drawImage(img, (t % 32) * 16, (t >> 5) * 16, 16, 16, px, py, cell, cell);
	}
	function draw() {
		if (!scr) return;
		var y, x, v, k;
		ctx.fillStyle = '#000'; ctx.fillRect(0, 0, 80 * cell, rowy(24));
		for (y = 0; y < 24; y++) if (y < MAP0 || y > MAP1)
			for (x = 0; x < 80; x++) { v = scr[y * 80 + x]; glyph(v & 255, x * tw, rowy(y), tw, th, v & A_STANDOUT, fpx); }
		for (y = MAP0; y <= MAP1; y++)
			for (x = 0; x < 80; x++) {
				k = cells[y * 80 + x];
				if (k > 0) {
					var t = k >> 12, u = (k & 0xfff) - 1;
					if (u >= 0) tile(u, x * cell, rowy(y));
					tile(t, x * cell, rowy(y));
				} else if (k < 0) glyph(-2 - k, x * cell, rowy(y), cell, cell, 0, Math.round(cell * 0.78));
			}
		var inbox = box[1] >= 0 && cur.y >= MAP0 && cur.y <= MAP1;
		if (box[1] >= 0) {     /* text over the map, one text cell of padding */
			var bx = box[2] * cell, by = rowy(box[0]), w = (box[3] - box[2] + 3) * tw, h = (box[1] - box[0] + 1) * th + tw;
			if (bx + w > 80 * cell) bx = 80 * cell - w;
			ctx.fillStyle = '#000'; ctx.fillRect(bx, by, w, h);
			ctx.strokeStyle = FG; ctx.lineWidth = 1; ctx.strokeRect(bx + 0.5, by + 0.5, w - 1, h - 1);
			for (y = box[0]; y <= box[1]; y++)
				for (x = box[2]; x <= box[3]; x++) {
					v = scr[y * 80 + x];
					glyph(v & 255, bx + (x - box[2] + 1) * tw, by + tw / 2 + (y - box[0]) * th, tw, th, v & A_STANDOUT, fpx);
				}
		}
		ctx.fillStyle = FG; ctx.strokeStyle = FG;
		if (cur.y < MAP0 || cur.y > MAP1) ctx.fillRect(cur.x * tw, rowy(cur.y) + th - 2, tw, 2);
		else if (!inbox) ctx.strokeRect(cur.x * cell + 0.5, rowy(cur.y) + 0.5, cell - 1, cell - 1);
	}
	function zoom(d) {
		auto = false;
		cell = Math.max(8, Math.min(64, cell + d));
		store('hack-cell', cell);
		measure(); draw();
	}
	function setTiles(s) {
		set = SETS[s] ? s : 'dawn';
		store('hack-tileset', set);
		$('btn-tiles').textContent = 'Tiles: ' + SETS[set][1];
		if (!sheets[set]) { sheets[set] = new Image(); sheets[set].onload = draw; sheets[set].src = SETS[set][0]; }
		draw();
	}

	var hk = {
		frame: function (sp, cp, y0, y1, x0, x1) {
			scr = Module.HEAPU32.slice(sp >> 2, (sp >> 2) + 1920);
			cells = Module.HEAP32.slice(cp >> 2, (cp >> 2) + 1920);
			box = [y0, y1, x0, x1];
			if ($('game').hidden) {
				$('game').hidden = false;
				measure();
				if (auto) { cell = fit(); measure(); }
			}
			draw();
		},
		cursor: function (y, x) { cur.y = y; cur.x = x; draw(); },
		key: function () { return events.length ? events.shift() : -1; },
		/* autosave at most every 2 s, and when the page is hidden */
		wantSave: function () {
			var now = performance.now();
			if (!wantSaveFlag || (now - lastSave < 2000 && !document.hidden)) return 0;
			wantSaveFlag = false; lastSave = now;
			setTimeout(syncFiles, 0);
			return 1;
		},
		end: function (saved) {
			running = false;
			return new Promise(function (done) {
				syncFiles(function () {
					$('overlay-msg').textContent = saved ? 'Your game has been saved. Play again to continue it.' : 'The game is over.';
					$('overlay').hidden = false;
					done();
				});
			});
		}
	};

	/* ---------- input ---------- */
	function onKey(e) {
		if (!$('help').hidden) {
			if (e.key === 'Escape') { $('help').hidden = true; e.preventDefault(); }
			return;
		}
		if (!running || e.isComposing || e.metaKey) return;
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

	/* ---------- saves: IndexedDB (IDBFS) ---------- */
	var syncing = false, syncAgain = false, pendingCbs = [];
	function syncFiles(cb) {
		if (!Module.FS) { if (cb) cb(); return; }
		if (typeof cb === 'function') pendingCbs.push(cb);
		if (syncing) { syncAgain = true; return; }
		syncing = true;
		var cbs = pendingCbs; pendingCbs = [];
		Module.FS.syncfs(false, function (err) {
			syncing = false;
			if (err) status('Saving to browser storage (IndexedDB) failed: ' + err + '. Use "Export save" to keep a copy.', true);
			cbs.forEach(function (f) { f(err); });
			if (syncAgain) { syncAgain = false; syncFiles(); }
		});
	}
	/* the save file is save/<uid><name>; the page passes -u <name> to find it again */
	function saveFile() {
		try {
			return Module.FS.readdir(SAVES).filter(function (f) { return f[0] !== '.' && !/\.tmp$/.test(f); })[0] || null;
		} catch (e) { return null; }
	}
	function exportSave() {
		var f = saveFile();
		if (!f) { status('There is no saved game yet.', true); setTimeout(function () { status(''); }, 2000); return; }
		var a = document.createElement('a');
		a.href = URL.createObjectURL(new Blob([Module.FS.readFile(SAVES + '/' + f)], { type: 'application/octet-stream' }));
		a.download = f;
		document.body.appendChild(a); a.click();
		setTimeout(function () { URL.revokeObjectURL(a.href); a.remove(); }, 1000);
	}
	function clearSaves() { var f; while ((f = saveFile())) Module.FS.unlink(SAVES + '/' + f); }
	function importSave(file) {
		var name = file.name.replace(/^\d+/, '').replace(/[^\w-]/g, '');
		if (!name) { status('A Hack save file is named like 501Name (user number, then the character name).', true); return; }
		var r = new FileReader();
		r.onload = function () {
			if (!confirm('Replace the current game with "' + file.name + '"?')) return;
			running = false;
			clearSaves();
			Module.FS.writeFile(SAVES + '/0' + name, new Uint8Array(r.result));
			syncFiles(function (err) { if (!err) location.reload(); });
		};
		r.readAsArrayBuffer(file);
	}
	function newGame() {
		if (!confirm('Delete the saved game in this browser and start a new one?')) return;
		running = false;
		clearSaves();
		syncFiles(function (err) { if (!err) location.reload(); });
	}

	/* ---------- help ---------- */
	var helpLoaded = false;
	function toggleHelp() {
		var h = $('help');
		h.hidden = !h.hidden;
		if (!h.hidden && !helpLoaded) {
			helpLoaded = true;
			fetch('help.html').then(function (r) { if (!r.ok) throw new Error(r.status); return r.text(); })
				.then(function (t) { $('help-body').innerHTML = t; })
				.catch(function (err) { helpLoaded = false; $('help-body').textContent = 'Could not load the guide (' + err + '). Press ? in the game for its own help.'; });
		}
		if (!h.hidden) $('help-body').focus();
	}

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
				if (err) status('Could not read saved games from IndexedDB (' + err + '). Saving may not work in this browser mode.', true);
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
		onRuntimeInitialized: function () { running = true; status(''); },
		print: function (s) { console.log(s); },
		printErr: function (s) { console.warn(s); },
		setStatus: function (s) { if (s && !running) status(s.replace(/\(\d+\/\d+\)/, '').trim() || 'Loading…'); },
		onAbort: function (what) { crashed(what); }
	};
	function crashed(err) {
		if (!running) return;
		running = false;
		var msg = (err && (err.message || err.reason && err.reason.message)) || String(err);
		console.error('[hack] crash:', err);
		status('The game crashed (' + msg + '). Reload the page to continue from the last autosave.', true);
	}
	window.addEventListener('unhandledrejection', function (e) {
		if (e.reason && e.reason.name === 'ExitStatus') return;   /* exit() is the normal end */
		crashed(e.reason);
	});
	window.addEventListener('error', function (e) {
		if (e.error && e.error.name === 'ExitStatus') return;
		if (e.error instanceof WebAssembly.RuntimeError || /hack-core/.test(e.filename || '')) crashed(e.error || e.message);
	});
	document.addEventListener('visibilitychange', function () { if (document.hidden) wantSaveFlag = true; });

	window.addEventListener('resize', function () { if (auto && scr) { cell = fit(); measure(); draw(); } });
	document.addEventListener('keydown', onKey);
	document.addEventListener('DOMContentLoaded', function () {
		cv = document.querySelector('#game canvas');
		ctx = cv.getContext('2d');
		var c = +store('hack-cell');
		if (c >= 8 && c <= 64) { cell = c; auto = false; }
		setTiles(store('hack-tileset'));
		$('btn-tiles').onclick = function () { setTiles(set === 'dawn' ? 'nethack' : 'dawn'); };
		$('btn-export').onclick = exportSave;
		$('btn-import').onclick = function () { $('import-file').click(); };
		$('import-file').onchange = function () { if (this.files[0]) importSave(this.files[0]); this.value = ''; };
		$('btn-new').onclick = newGame;
		$('btn-help').onclick = toggleHelp;
		$('help-close').onclick = toggleHelp;
		$('btn-zoom-in').onclick = function () { zoom(2); };
		$('btn-zoom-out').onclick = function () { zoom(-2); };
		$('btn-restart').onclick = function () { location.reload(); };
		document.querySelectorAll('button').forEach(function (b) {
			b.addEventListener('mousedown', function (e) { e.preventDefault(); });
		});
	});
})();
