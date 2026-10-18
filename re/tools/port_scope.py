"""Phase 2.0 scope manifest: assigns every golf_clean.exe function (re/functions_ghidra.tsv) to a PORT MODULE and
writes the ordered sim-core list for the phase 3 children. Rules: re/port/scope_rules.md.

    py -3.12 re/tools/port_scope.py [--rebuild-inputs]      # writes re/port/scope.tsv, scope_straddlers.tsv, simcore_order.tsv

Two stages. `load_inputs()` is the slow one (about 45 s: the model_scan reference scan, the mainLoop reach map, tags) and
is cached in log/p2/p20/inputs.json. `build(inputs)` is pure and deterministic: the test feeds it a synthetic graph.
"""
import argparse, bisect, collections, csv, glob, json, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))

CACHE = ROOT / "log" / "p2" / "p20" / "inputs.json"
OUT = ROOT / "re" / "port"
DATA_BASE = 0x4C1000
LOGIC_HI = 0x4A4ED0
NET_RANGE = (0x497B40, 0x49BEC0)
SIM_PHASES = ["PRE", "P1", "P2", "P3", "P4", "P5", "P6", "P7", "H1", "H2"]
SESSION_PHASES = ["SETUP", "SESSION", "QUIT", "MENU"]
PHASE_ORDER = ["SETUP", "SESSION", "QUIT", "MENU", "PRE", "P1", "P2", "P3", "P4", "P5", "P6", "P7", "H1", "H2"]
SIM_TAGS = {"golfer", "sim", "economy", "ai", "course", "terrain", "util", "-", ""}
UI_TAGS = {"ui", "input", "frontend"}
GFX_TAGS = {"render", "video"}
HEAP_CLASSES = {"ui", "pointer", "render", "audio"}
NOT_LEAF_TAGS = {"input", "render", "video", "audio", "net", "crt", "save", "boot"}
CONST_HEAP = re.compile(r"^f_(ptrtab|strpool|tab)_")   # constant string / pointer tables of stream F: reading them is not a heap dereference
DRAW_NAME = re.compile(r"^(draw|show|render|paint|panel|window|menu|blit|Surface|Image|Widget|View|Button|EditBox|ListBox|TextView|"
                       r"scroll)", re.I)
MAINLOOP = 0x40F5C0
GLUE_NAME = re.compile(r"(_adj|vtordisp|dynInit|atexit|exitDtor|gSurfaceDtor|scalarDtor|deletingDtor|scalar_dtor|"
                       r"thunk|fwdDev|callChild|ctorVtbl|^FUN_)")
FMT_NAME = re.compile(r"^(load|decode|parse|openImage|Image::(load|save)|Flic|Bink|jpeg_|jinit_|save|write(Pcx|Bmp))", re.I)
JPEG_NAME = re.compile(r"(jpeg|jinit|jcopy|jround|jdiv|jzero|jpeg_|emit_|encode_mcu|forward_DCT|fdct|quant|huff|sarray|barray)", re.I)

# suggested phase 3 child: name rules first, then subsystem tag, then majority of already assigned neighbours
CHILD_NAME = [
    ("3.4", r"(shot|ball|trial|swing|flight|putt|chooseShot|lie\b|stroke)"),
    ("3.5", r"(story|event|message|news|thought|comment|bulletin|patron|opinion|notice|tutorial|announce|popup)"),
    ("3.6", r"(walker|animal|ambient|habitat|spawn|tourist|wildlife)"),
    ("3.1", r"(tile|terrain|course|cell|height|grid|slope|build|edit|hole|record|object|footprint|coverage|river|flood)"),
    ("3.2", r"(cash|financ|member|price|rating|award|econom|money|year|date|budget|trend|score|fee|salary|employee)"),
    ("3.3", r"(golfer|pair|arriv|ban|mood|skill|handicap|player)"),
    ("3.7", r"(tick|command|frame|pump|pause|speed|handle|dispatch)"),
]
CHILD_TAG = {"golfer": "3.3", "economy": "3.2", "course": "3.1", "terrain": "3.1", "ai": "3.3"}
CHILD_TITLE = {"3.0": "core leaves", "3.1": "world and course editing", "3.2": "economy and membership", "3.3": "golfers",
               "3.4": "shots", "3.5": "events/news/stories", "3.6": "walkers/animals/ambient", "3.7": "tick and commands"}


# ------------------------------------------------------------------------------------------------ inputs (slow)
def load_inputs(rebuild=False):
    if CACHE.exists() and not rebuild:
        return json.loads(CACHE.read_text())
    import model_inventory as mi
    inv = mi.Inventory()
    objs = []
    for m in re.finditer(r"SG_OBJECT\((\w+), (0x[0-9a-f]+), (0x[0-9a-f]+), (\w+), (\w+), (\w+), (\d)\)",
                         (ROOT / "port/include/gamestate_meta.inc").read_text()):
        objs.append([m.group(1), DATA_BASE + int(m.group(2), 16), int(m.group(3), 16), m.group(4)])
    objs.sort(key=lambda o: o[1])
    starts = [o[1] for o in objs]
    touch = collections.defaultdict(lambda: collections.defaultdict(lambda: [0, 0]))   # fn -> sym -> [reads, writes]
    for r in inv.refs:
        i = bisect.bisect_right(starts, r.addr) - 1
        if i < 0 or not (objs[i][1] <= r.addr < objs[i][1] + objs[i][2]):
            continue
        t = touch[r.fn][objs[i][0]]
        t[1 if "w" in (r.acc or "") else 0] += 1
    fns = []
    for va, name, size, rngs in sorted(__import__("xref").functions("golf_clean.exe")):
        fns.append(dict(addr=va, name=inv.fn_name.get(va, name), size=size, tag=inv.tags.get(va, "-"),
                        tag_src=inv.tag_src.get(va, ""), calls=sorted(inv.calls.get(va, [])),
                        phases=sorted(inv.phases.get(va, []), key=PHASE_ORDER.index) if va in inv.phases else [],
                        depth=inv.depth.get(va, -1), boot=va in inv.boot,
                        touch={s: v for s, v in sorted(touch.get(va, {}).items())}))
    data = dict(objects=objs, fns=fns, hooks=load_hooks(), defects=load_defects(fns))
    CACHE.parent.mkdir(parents=True, exist_ok=True)
    CACHE.write_text(json.dumps(data))
    return json.loads(CACHE.read_text())


def load_hooks():
    out = {}
    with open(ROOT / "hooks.csv", encoding="utf-8", errors="replace") as fh:
        for r in csv.DictReader(fh):
            if r["module"] != "golf_clean.exe":
                continue
            vec = 0
            for p in (r["frida_diff"] or "").split(";"):
                p = p.strip()
                f = ROOT / p
                if p and f.is_file():
                    vec += max(0, sum(1 for _ in open(f, encoding="utf-8", errors="replace")) - 1)
            out[int(r["addr"], 16)] = dict(level=r["confidence"], subsystem=r["subsystem"], file=r["file"], note=r["note"],
                                           vectors=vec, scenario=int(bool(r["scenario"])))
    return out


def load_defects(fns):
    """address -> defect numbers of model_defects.md whose row cites an address inside the function"""
    ents = []
    for ln in (ROOT / "re/analysis/data/model_defects.md").read_text(errors="replace").splitlines():
        m = re.match(r"\|\s*(\d+)\s*\|", ln)
        if m:
            ents.append((int(m.group(1)), [int(x, 16) for x in re.findall(r"0x0?0?([0-9a-fA-F]{6})\b", ln)], ln))
    starts = [f["addr"] for f in fns]
    out = collections.defaultdict(set)
    for n, adrs, ln in ents:
        for a in adrs:
            i = bisect.bisect_right(starts, a) - 1
            if i >= 0 and a < starts[i] + max(fns[i]["size"], 1) and 0x401000 <= a < 0x4BA000:
                out[fns[i]["addr"]].add(n)
        for f in fns:
            if len(f["name"]) > 6 and not f["name"].startswith("FUN_") and re.search(r"\b%s\b" % re.escape(f["name"]), ln):
                out[f["addr"]].add(n)
    return {str(k): sorted(v) for k, v in out.items()}


# ------------------------------------------------------------------------------------------------ rules (pure)
def classify(inp):
    """-> {addr: dict(module, rule, ...)}; every function exactly once"""
    cls = {o[0]: o[3] for o in inp["objects"]}
    F = {f["addr"]: f for f in inp["fns"]}
    res = {}
    for a, f in F.items():
        t = f["touch"]
        f["_sim"] = sorted(s for s in t if cls.get(s) == "sim")
        f["_hid"] = sorted(s for s in t if cls.get(s) == "hidden")
        f["_heap"] = sorted(s for s in t if cls.get(s) in HEAP_CLASSES and not CONST_HEAP.match(s))
        f["_eng"] = sorted(s for s in t if cls.get(s) == "engine")
        f["_audio"] = [s for s in f["_heap"] if cls[s] == "audio"]
        f["_render"] = [s for s in f["_heap"] if cls[s] == "render"]
        ph = set(f["phases"])
        f["_simreach"] = bool(ph & set(SIM_PHASES))
        f["_sesreach"] = bool(ph & set(SESSION_PHASES))
        f["_anyreach"] = bool(ph)
    gfx_callee = {a for a, f in F.items() if f["tag"] in GFX_TAGS}

    def put(a, mod, rule):
        res[a] = dict(module=mod, rule=rule)

    for a, f in sorted(F.items()):
        tag, name = f["tag"], f["name"]
        simt = f["_sim"] or f["_hid"]
        own = f["tag_src"] == "own"
        if a >= LOGIC_HI:
            put(a, "formats-loaders", "R2-LIB") if JPEG_NAME.search(name) and a < 0x4B7000 else put(a, "crt-dropped", "R1-RANGE")
        elif tag == "crt":
            put(a, "crt-dropped", "R1-TAG")
        elif tag == "net" or NET_RANGE[0] <= a < NET_RANGE[1]:
            put(a, "net", "R3-NET")
        elif tag == "audio":
            put(a, "backend-audio", "R4-AUDIO-TAG")
        elif not f["_anyreach"] and f["boot"]:
            put(a, "boot-only", "R5-BOOT-UNREACHED")
        elif tag == "boot" and not f["_simreach"]:
            put(a, "boot-only", "R5-BOOT-TAG")
        elif a == MAINLOOP:
            put(a, "sim-core", "R0-MAINLOOP")
        elif simt and (f["_simreach"] or f["_sesreach"] or not f["_anyreach"]) and tag in SIM_TAGS and not DRAW_NAME.match(name):
            put(a, "sim-core", "R6-SIM" if f["_simreach"] else ("R6-SIM-SESSION" if f["_sesreach"] else "R6-SIM-INDIRECT"))
        elif (simt and f["_simreach"] and tag in UI_TAGS | GFX_TAGS and not f["_heap"] and not DRAW_NAME.match(name)
              and not any(c in gfx_callee for c in f["calls"])):
            put(a, "sim-core", "R6-SIM-RETAG")      # ui-family tag, but no ui/render object, no drawing callee, not a drawing name
        elif tag in GFX_TAGS:
            put(a, "backend-gfx", "R7-GFX-TAG")
        elif tag in SIM_TAGS and DRAW_NAME.match(name) and a != MAINLOOP:
            put(a, "ui", "R8-UI-NAME")                  # a drawing / screen function tagged with a sim subsystem
        elif tag == "terrain" and (f["_heap"] or DRAW_NAME.match(name)):
            put(a, "backend-gfx", "R7-TERRAIN")
        elif tag in UI_TAGS:
            put(a, "ui", "R8-UI-TAG")
        elif tag == "save":
            put(a, "formats-loaders", "R9-SAVE")
        elif FMT_NAME.search(name) and not simt:
            put(a, "formats-loaders", "R9-FMT-NAME")
        elif f["_audio"] and not simt:
            put(a, "backend-audio", "R4-AUDIO-OBJ")
        elif f["_render"] and not simt:
            put(a, "backend-gfx", "R7-GFX-OBJ")
        elif f["_heap"] and not simt:
            put(a, "ui", "R8-UI-OBJ")
        elif GLUE_NAME.search(name):
            put(a, "engine-glue", "R11-GLUE-NAME")
    # R10 sim-leaf-util: callees of the sim set that touch no heap object and carry no non-sim tag, to a fixed point
    changed = True
    while changed:
        changed = False
        for a, r in sorted(res.items()):
            if r["module"] not in ("sim-core", "sim-leaf-util"):
                continue
            for c in F[a]["calls"]:
                f = F.get(c)
                if f is None or c in res and res[c]["module"] != "unknown":
                    continue
                if f["tag"] in NOT_LEAF_TAGS or f["_heap"] or c >= LOGIC_HI or c == MAINLOOP:
                    continue
                if f["tag"] in UI_TAGS and (DRAW_NAME.match(f["name"]) or any(x in gfx_callee for x in f["calls"])):
                    continue
                put(c, "sim-leaf-util", "R10-LEAF")
                changed = True
    for a, f in sorted(F.items()):
        if a in res:
            continue
        if f["tag"] == "terrain":
            put(a, "backend-gfx", "R7-TERRAIN-LATE")     # terrain helper no sim function calls and no sim object: Terrain.dll glue
        elif f["_eng"] or not f["touch"]:
            put(a, "engine-glue", "R11-ENGINE" if f["_eng"] else "R11-NOTOUCH")
        else:
            put(a, "unknown", "R12-UNKNOWN")
    for a, r in res.items():
        f = F[a]
        r["straddler"] = int(bool(f["_sim"] or f["_hid"]) and bool(f["_heap"] or r["module"] in ("ui", "backend-gfx"))
                             and r["module"] != "crt-dropped")
    assert len(res) == len(F)
    return res, F


def phase3_child(F, res):
    """suggested phase 3 child per sim-core / leaf function"""
    ids = [a for a, r in res.items() if r["module"] in ("sim-core", "sim-leaf-util")]
    child, how = {}, {}
    for a in ids:
        f = F[a]
        for c, rx in CHILD_NAME:
            if re.search(rx, f["name"], re.I):
                child[a], how[a] = c, "name"
                break
        else:
            if f["tag"] in CHILD_TAG:
                child[a], how[a] = CHILD_TAG[f["tag"]], "tag"
    for _ in range(4):                    # majority of assigned callers and callees (call-graph clustering)
        callers = collections.defaultdict(list)
        for a in ids:
            for c in F[a]["calls"]:
                callers[c].append(a)
        new = {}
        for a in ids:
            if a in child:
                continue
            cnt = collections.Counter(child[x] for x in F[a]["calls"] + callers.get(a, []) if x in child)
            if cnt:
                top = cnt.most_common(2)
                if len(top) == 1 or top[0][1] > top[1][1]:
                    new[a] = top[0][0]
        for a, c in new.items():
            child[a], how[a] = c, "graph"
    for a in ids:
        if a not in child and not F[a]["calls"]:
            child[a], how[a] = "3.0", "leaf"
    return child, how


def order(F, res):
    """sim-core + leaf functions leaves-first: level = 1 + max level of callees inside the set (cycles broken by DFS order)"""
    ids = {a for a, r in res.items() if r["module"] in ("sim-core", "sim-leaf-util")}
    level, state = {}, {}

    def go(a):
        if a in level:
            return level[a]
        if state.get(a) == 1:
            return -1
        state[a] = 1
        ls = [go(c) for c in F[a]["calls"] if c in ids and c != a]
        level[a] = 1 + max([x for x in ls if x >= 0], default=-1)
        state[a] = 2
        return level[a]

    for a in sorted(ids):
        go(a)
    return sorted(ids, key=lambda a: (level[a], a)), level


# ------------------------------------------------------------------------------------------------ writers
def evidence(inp, a):
    h = inp["hooks"].get(str(a)) or inp["hooks"].get(a)
    if not h:
        return "none"
    s = f"{h['level']} {h['file']} vec={h['vectors']}"
    if h["scenario"]:
        s += " scenario"
    return s


def write_all(inp, out=OUT):
    out.mkdir(parents=True, exist_ok=True)
    res, F = classify(inp)
    hooks = {int(k): v for k, v in inp["hooks"].items()}
    defects = {int(k): v for k, v in inp["defects"].items()}

    def lvl(a):
        return hooks[a]["level"] if a in hooks else "none"

    def hexa(a):
        return "%08x" % a

    rows = []
    for a in sorted(F):
        f, r = F[a], res[a]
        ev = ("%s %s vec=%d%s" % (hooks[a]["level"], hooks[a]["file"], hooks[a]["vectors"], " scenario" if hooks[a]["scenario"] else "")
              if a in hooks else "none")
        rows.append([hexa(a), f["name"], f["size"], lvl(a), f["tag"], r["module"],
                     ",".join(f["phases"]) + ("+boot" if f["boot"] else "") if (f["phases"] or f["boot"]) else "indirect",
                     r["rule"], len(f["_sim"]), len(f["_heap"]), ev, ";".join(f["_sim"]), ";".join(f["_heap"]), r["straddler"]])
    with open(out / "scope.tsv", "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh, delimiter="\t", lineterminator="\n")
        w.writerow(["addr", "name", "size", "hooks_level", "subsystem", "port_module", "mainloop_phase", "reach_reason",
                    "touches_sim_objects", "touches_heap_objects", "evidence", "sim_objects", "heap_objects", "straddler"])
        w.writerows(rows)
    with open(out / "scope_straddlers.tsv", "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh, delimiter="\t", lineterminator="\n")
        w.writerow(["addr", "name", "port_module", "size", "sim_objects", "hidden_objects", "heap_objects"])
        for a in sorted(F):
            if res[a]["straddler"]:
                f = F[a]
                w.writerow([hexa(a), f["name"], res[a]["module"], f["size"], ";".join(f["_sim"]), ";".join(f["_hid"]),
                            ";".join(f["_heap"])])
    ordr, level = order(F, res)
    child, how = phase3_child(F, res)
    with open(out / "simcore_order.tsv", "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh, delimiter="\t", lineterminator="\n")
        w.writerow(["order", "level", "addr", "name", "size", "port_module", "phase3_child", "child_basis", "mainloop_phase",
                    "globals_sim", "globals_hidden", "globals_heap", "hook_source", "evidence", "purpose_note", "defects"])
        for i, a in enumerate(ordr, 1):
            f, h = F[a], hooks.get(a)
            note = h["note"] if h and h["note"].startswith("re/analysis") else ""
            w.writerow([i, level[a], hexa(a), f["name"], f["size"], res[a]["module"], child.get(a, "unassigned"), how.get(a, ""),
                        ",".join(f["phases"]) or "indirect", ";".join(f["_sim"]), ";".join(f["_hid"]), ";".join(f["_heap"]),
                        h["file"] if h else "", evidence(inp, a), note,
                        ";".join("model_defects#%d" % n for n in defects.get(a, []))])
    return res, F, ordr, child


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rebuild-inputs", action="store_true")
    args = ap.parse_args()
    inp = load_inputs(args.rebuild_inputs)
    res, F, ordr, child = write_all(inp)
    c = collections.Counter(r["module"] for r in res.values())
    print("functions", len(F), dict(sorted(c.items())))
    print("straddlers", sum(r["straddler"] for r in res.values()), "simcore_order", len(ordr))
    print("by child", dict(sorted(collections.Counter(child.get(a, "unassigned") for a in ordr).items())))


if __name__ == "__main__":
    main()
