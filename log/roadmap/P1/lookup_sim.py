"""Simulate audio_core lookup_port for every sound.dll rva in two trees and compare the resolved port function.

usage: sim_lookup.py <old_tree> <new_tree> <rename_map.json>
Resolution (audio_core.cpp lookup_port): first kSoundFunctions row with that rva; skip if collides; registry_find("snd."+name)
(break after the first row either way); if none, audio_crt_name(rva): crt.strncpy / crt.ftol (snd_crt constants, unchanged),
audio_crt.cpp kNames, audio_exe.cpp kSndExtra. A function is identified by its registered name mapped through the rename map.
"""
import json, pathlib, re, sys

ROW = re.compile(r'\{0x([0-9a-f]+), "([^"]+)", (true|false)\}')
PAIR = re.compile(r'\{0x([0-9a-f]+), "(snd\.[^"]+)"\}')
REG = re.compile(r'\{\s*"([^"]+)",\s*"(?:default|cdecl|stdcall|thiscall|fastcall|mscdecl)"')
MACRO = re.compile(r'#define\s+(\w+)\(name[^)]*\)\s*\{\s*"([^"]*)"\s*#name')


def load(tree):
    t = pathlib.Path(tree)
    h = (t / 'port/include/portsim/sim/soundstate.h').read_text(encoding='utf-8')
    rows = [(int(a, 16), n, c == 'true') for a, n, c in ROW.findall(h[h.index('kSoundFunctions'):])]
    crt = (t / 'port/src/sim/audio_crt.cpp').read_text(encoding='utf-8')
    exe = (t / 'port/src/sim/audio_exe.cpp').read_text(encoding='utf-8')
    kn = crt[crt.index('const RvaName kNames[]'):]
    kn = {int(a, 16): n for a, n in PAIR.findall(kn[:kn.index('};')])}
    kx = exe[exe.index('const ExeFn kSndExtra[]'):]
    kx = {int(a, 16): n for a, n in PAIR.findall(kx[:kx.index('};')])}
    reg = set()
    for p in sorted((t / 'port/src/sim').glob('*.cpp')) + sorted((t / 'port/src/sim').glob('*.inc')):
        s = p.read_text(encoding='utf-8', errors='replace')
        reg |= set(REG.findall(s))
        for macro, prefix in MACRO.findall(s):
            reg |= {prefix + n for n in re.findall(rf"^\s*{macro}\((\w+),", s, re.M)}
    return rows, kn, kx, reg


def resolve(env, rva):
    rows, kn, kx, reg = env
    for a, n, c in rows:
        if a != rva or c:
            continue
        if 'snd.' + n in reg:
            return 'snd.' + n
        break
    for tab in (kn, kx):
        if rva in tab:
            return tab[rva] if tab[rva] in reg else None
    return None


old, new = load(sys.argv[1]), load(sys.argv[2])
ren = {'snd.' + k: 'snd.' + v for k, v in json.load(open(sys.argv[3])).items()}
rvas = {a for a, _, _ in old[0] + new[0]} | set(old[1]) | set(old[2]) | set(new[1]) | set(new[2])
diff = 0
for rva in sorted(rvas):
    o, n = resolve(old, rva), resolve(new, rva)
    if ren.get(o, o) != n:
        diff += 1
        print(f'{rva:#07x} old={o} new={n}')
print(f'rvas {len(rvas)} resolved_old {sum(resolve(old, r) is not None for r in rvas)} '
      f'resolved_new {sum(resolve(new, r) is not None for r in rvas)} differ {diff}')
