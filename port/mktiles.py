#!/usr/bin/env python3
"""Hack tiles (RVIP step 4). One slot list, two sheets with the same layout:
tiles.png (NetHack 3.6 tiles) and tiles-dawn.png (DawnLike, NetHack for gaps),
plus tilemap.h: tile_key[slot] = "M:<monster>", "O:<object>", "D:<sym><look>"
(unidentified appearance), "C:<sym>" (class), "T:<terrain>", "P:<role letter>".
Needs ~/Games/rvip-tools/tilesets (DawnLike + dawnlike_names.tsv) and Pillow."""
import os, re, sys, zlib
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(HERE), 'src')
DL = os.path.expanduser('~/Games/rvip-tools/tilesets')

# ---- NetHack tiles: name -> 16x16 RGBA image -------------------------------
nh, nh_order = {}, []
for f in ('monsters', 'objects', 'other'):
    txt, pal, i = open(os.path.join(HERE, 'nethack', f + '.txt')).read().split('\n'), {}, 0
    while i < len(txt):
        m = re.match(r"^(\S) = \((\d+), *(\d+), *(\d+)\)", txt[i])
        if m: pal[m[1]] = tuple(map(int, m.group(2, 3, 4)))
        m = re.match(r"^# tile \d+ \((.*)\)$", txt[i])
        if m:
            n, rows = m[1], []
            i += 1
            while txt[i].strip() != '{': i += 1
            i += 1
            while not txt[i].startswith('}'): rows.append(txt[i].strip()); i += 1
            im = Image.new('RGBA', (16, 16))
            im.putdata([pal[c] + ((0,) if pal[c] == (71, 108, 108) else (255,)) for r in rows for c in r])
            n = re.sub(r', ?(male|female|nogender)$', '', n)
            for k in [n] + [p.strip() for p in n.split(' / ')]:
                k = ('T:' if f == 'other' else '') + k
                if k not in nh: nh[k] = im; nh_order.append((f, k))
        i += 1

# ---- DawnLike: name -> sprite (frame 0) -------------------------------------
dl, sheets = {}, {}
for l in open(os.path.join(DL, 'dawnlike_names.tsv')).read().split('\n')[1:]:
    f = l.split('\t')
    if len(f) == 5 and f[1] == '0': dl.setdefault(f[0], (f[2], int(f[3]), int(f[4])))
def dsprite(n):
    s, c, r = dl[n]
    if s not in sheets: sheets[s] = Image.open(os.path.join(DL, 'DawnLike', s)).convert('RGBA')
    return sheets[s].crop((c * 16, r * 16, c * 16 + 16, r * 16 + 16))

# ---- Hack's monsters and objects, from the C sources -------------------------
csrc = ''.join(open(os.path.join(SRC, f), encoding='latin-1').read() for f in os.listdir(SRC) if f.endswith('.c'))
mons = re.findall(r'\{"([^"]+)",\s*\'', open(os.path.join(SRC, 'hack.monst.c')).read())
mons += re.findall(r'struct permonst \w+ = \{"([^"]+)"', csrc)
SYM = {'FOOD': '%', 'WEAPON': ')', 'ARMOR': '[', 'POTION': '!', 'SCROLL': '?', 'WAND': '/', 'RING': '=', 'GEM': '*',
       'TOOL_SYM': '(', 'BALL_SYM': '0', 'CHAIN_SYM': '_', 'ROCK_SYM': '`', 'AMULET_SYM': '"', 'ILLOBJ_SYM': '\\'}
objs = []   # (sym, name or None, descr or None)
body = open(os.path.join(SRC, 'def.objects.h')).read()
for m in re.finditer(r'^\s*(FOOD|WEAPON|ARMOR|POTION|SCROLL|WAND|RING|GEM)\((NULL|"[^"]*"),\s*("[^"]*")?|'
                     r'^\s*\{(NULL|"[^"]*"),\s*(NULL|"[^"]*"),\s*NULL,\s*\d+,\s*\d+,\s*(\w+)', body, re.M):
    if m[1]:
        name = None if m[2] == 'NULL' else m[2][1:-1]
        objs.append((SYM[m[1]], name, m[3][1:-1] if m[3] else None))
    else:
        objs.append((SYM[m[6]], None if m[4] == 'NULL' else m[4][1:-1], None if m[5] == 'NULL' else m[5][1:-1]))

# ---- slots: key -> ([NetHack candidates], [DawnLike candidates]) -------------
NHM = {'violet fungi': 'violet fungus', 'zombie': 'human zombie', 'fog cloud': 'fog cloud', 'nymph': 'water nymph',
       'centaur': 'forest centaur', 'mimic': 'large mimic', 'unicorn': 'white unicorn', 'vampire': 'vampire',
       'dragon': 'red dragon', 'xan': 'xan', 'zruty': 'zruty', 'demon': 'hezrou', 'wizard of Yendor': 'Wizard of Yendor',
       'mail daemon': 'mail daemon', 'guard': 'watchman', 'wraith': 'wraith', 'hell hound': 'hell hound',
       'giant rat': 'giant rat', 'piercer': 'rock piercer', 'long worm tail': 'long worm tail', 'ghost': 'ghost'}
DLM = {'violet fungi': 'violet fungus', 'zombie': 'human zombie', 'nymph': 'water nymph', 'centaur': 'forest centaur',
       'mimic': 'large mimic', 'unicorn': 'white unicorn', 'dragon': 'red dragon', 'demon': 'hezrou', 'bat': 'giant bat',
       'wizard of Yendor': 'wizard of yendor', 'guard': 'watchman', 'giant eel': 'eel', 'little dog': 'little terrier',
       'dog': 'terrier', 'large dog': 'big terrier', 'jaguar': 'leopard', 'piercer': 'rock piercer', 'trapper': 'lurker below',
       'dragon': 'firedrake', 'chameleon': 'platino'}   # platino: the DawnLike author's easter-egg request
NHO = {'two handed sword': 'two-handed sword', 'pick-axe': 'pick-axe', 'ice box': 'ice box', 'whistle': 'tin whistle',
       'magic whistle': 'magic whistle', 'helmet': 'orcish helm', 'shield': 'small shield', 'pair of gloves': 'leather gloves',
       'elven cloak': 'elven cloak', 'dead lizard': 'lizard corpse', 'tin': 'tin', 'carrot': 'carrot',
       'can opener': 'tin opener', 'worm tooth': 'worm tooth', 'amulet of Yendor': 'Amulet of Yendor',
       'strange object': 'strange object', 'enormous rock': 'boulder', 'tripe ration': 'tripe ration',
       'clove of garlic': 'clove of garlic', 'egg': 'egg', 'sling bullet': 'flint', 'expensive camera': 'expensive camera',
       'studded leather armor': 'studded leather armor', 'large box': 'large box'}
DLO = {'two handed sword': 'two handed sword', 'pick-axe': 'pick axe', 'whistle': 'tin whistle', 'magic whistle': 'magic whistle',
       'helmet': 'helmet', 'shield': 'small shield', 'pair of gloves': 'leather glove', 'elven cloak': 'forest cloak',
       'can opener': 'tin opener', 'expensive camera': 'camera', 'enormous rock': 'boulder', 'heavy iron ball': 'squeaky ball',
       'ring mail': 'chain shirt', 'splint mail': 'iron armor', 'plate mail': 'full plate', 'chain mail': 'chain shirt',
       'scale mail': 'scale armor', 'studded leather armor': 'brass armor', 'leather armor': 'animal hide',
       'sling bullet': 'mid bullets', 'rock': 'rock', 'amulet of Yendor': 'amulet of yendor', 'large box': 'closed chest',
       'ice box': 'closed ice chest', 'tripe ration': 'c ration', 'dead lizard': 'lizard corpse', 'egg': 'egg',
       'clove of garlic': 'sprig of wolfsbane', 'melon': 'pumpkin', 'bow': 'shortbow'}
ROLE = {'T': ('tourist', 'tourist'), 'S': ('archeologist', 'archaeologist'), 'F': ('valkyrie', 'fighter'),
        'K': ('knight', 'knight'), 'C': ('cave dweller', 'caveman'), 'W': ('wizard', 'archmage')}
CLS = {'!': 'potion', '?': 'scroll', '/': 'wand', '=': 'ring', '*': 'gem', ')': 'weapon', '[': 'armor', '(': 'tool',
       '%': 'food', '"': 'amulet'}
GEMNH = {'dark': 'black', 'yellowish brown': 'yellowish brown'}
TERR = {  # key: (NetHack, DawnLike)
 'floor': ('floor of a room', 'night stone floor c'),
 'corr': ('corridor', 'night dirt floor c'), 'hwall': ('main walls horizontal', 'dim brick wall left right'),
 'vwall': ('main walls vertical', 'dim brick wall up down'),
 'tl': ('main walls tlcorn', 'dim brick wall right down'), 'tr': ('main walls trcorn', 'dim brick wall left down'),
 'bl': ('main walls blcorn', 'dim brick wall right up'), 'br': ('main walls brcorn', 'dim brick wall left up'),
 'hdoor': ('horizontal open door', 'broken wooden door'), 'vdoor': ('vertical open door', 'broken wooden door'), 'up': ('staircase up', 'small stairs up'),
 'down': ('staircase down', 'small stairs down'), 'pool': ('water', 'deep water tile'),
 'trap': ('magic trap', 'magic trap tile'), 'bear': ('bear trap', 'bear trap tile'), 'arrow': ('arrow trap', 'arrow trap tile'),
 'dart': ('dart trap', 'dart trap tile'), 'trapdoor': ('trap door', 'trap door tile'),
 'teleport': ('teleportation trap', 'teleportation trap tile'), 'sleep': ('sleeping gas trap', 'sleeping gas trap tile'),
 'pierc': ('falling rock trap', 'falling rock trap tile'), 'gold': ('gold piece', 'pile of gold coins'),
}

slots = {}
def add(key, nhc, dlc): slots.setdefault(key, ([c for c in nhc if c], [c for c in dlc if c]))
for n in mons + ['long worm tail']:
    add('M:' + n, [NHM.get(n), n], [DLM.get(n), n, n.lower()])
for c, (a, b) in ROLE.items(): add('P:' + c, [a], [b])
for sym, name, descr in objs:
    cls = CLS.get(sym, '')
    if descr and sym not in '(':
        look = GEMNH.get(descr, descr) if sym == '*' else descr.replace('aluminium', 'aluminum')
        add('D:' + sym + descr, ['%s %s' % (look, cls)],
            ['%s %s' % (descr.replace('aluminium', 'aluminum'), cls), 'gleaming %s gem' % descr])
    elif name:
        add('O:' + name, [NHO.get(name), name, 'corpse' if name.startswith('dead ') else None],
            [DLO.get(name), name, 'corpse' if name.startswith('dead ') else None])
    if descr and sym == '(':
        add('D:(' + descr, [NHO.get(descr), descr], [DLO.get(descr), descr])
DLC = {'!': 'clear potion', '?': 'blank scroll', '/': 'balsa wand', '=': 'plain ring', '*': 'dull gray gem', ')': 'long sword',
       '[': 'chain shirt', '(': 'closed chest', '%': 'food ration', '"': 'fuzzy amulet'}
for sym, c in CLS.items(): add('C:' + sym, [c, 'generic ' + c], [DLC[sym]])
for k, (a, b) in TERR.items(): add('T:' + k, ['T:' + a, a], [b])

# NetHack appearance tiles are "<look> / <identity>": find the look inside the class's band
BAND = {'potion': ('ruby / gain ability', 'clear / water'), 'scroll': ('ZELGO MER / enchant armor', 'unlabeled / blank paper'),
        'ring': ('wooden / adornment', 'shiny / protection from shape changers'), 'wand': ('glass / light', 'jeweled'),
        'gem': ('white / dilithium crystal', 'gray / flint')}
objnames = [k for f, k in nh_order if f == 'objects']
def band(cls):
    a, b = BAND[cls]
    return objnames[objnames.index(a):objnames.index(b) + 1]
def h(s, n): return zlib.crc32(s.encode()) % n

missing = []
def nh_img(key, cands):
    for c in cands:
        if c in nh: return nh[c]
        m = re.match(r'(.*) (potion|scroll|ring|wand|gem)$', c)
        if m:
            b = band(m[2])
            for k in b:
                if k.split(' / ')[0] == m[1]: return nh[k]
            missing.append('nh ' + key)
            return nh[b[h(m[1], len(b))]]      # unknown look: a stable pick from the class
    missing.append('nh ' + key)
    return nh['strange object']

def dl_img(key, cands, fallback):
    for c in cands:
        if c in dl: return dsprite(c)
    m = re.match(r'D:(.)(.*)', key)
    if m and m[1] in '!?/=*':
        pool = sorted(n for n in dl if dl[n][0] == {'!': 'Items/Potion.png', '?': 'Items/Scroll.png', '/': 'Items/Wand.png',
                      '=': 'Items/Ring.png', '*': 'Items/Money.png'}[m[1]] and (m[1] != '*' or n.endswith(' gem')))
        return dsprite(pool[h(m[2], len(pool))])
    missing.append('dl ' + key)
    return fallback

keys = list(slots)
PER = 32
H = (len(keys) + PER - 1) // PER * 16
for name, dawn in (('tiles', False), ('tiles-dawn', True)):
    img = Image.new('RGBA', (PER * 16, H))
    for i, k in enumerate(keys):
        t = nh_img(k, slots[k][0])
        if dawn: t = dl_img(k, slots[k][1], t)
        img.paste(t, ((i % PER) * 16, (i // PER) * 16))
    img.save(os.path.join(HERE, name + '.png'))
    open(os.path.join(HERE, name + '.rgba'), 'wb').write(
        img.size[0].to_bytes(4, 'little') + img.size[1].to_bytes(4, 'little') + img.tobytes())
open(os.path.join(HERE, 'tilemap.h'), 'w').write(
    '/* generated by port/mktiles.py - do not edit */\n#define TILES_PER_ROW %d\nstatic const char *tile_key[] = {\n%s};\n'
    % (PER, ''.join('    "%s",\n' % k.replace('\\', '\\\\').replace('"', '\\"') for k in keys)))
print(len(keys), 'slots; fallbacks:', sorted(set(missing)))
