"""Static inventory of the movies and FLIC animations the game can open (roadmap step S8.1, no game run).

Derives from the anchored binaries (original/golf_clean.exe, jgld.dll, Terrain.dll, sound.dll):
  * the binkw32 imports of every module and every instruction operand that names an import slot (a call site inside a
    function, or a slot in the SafeDisc padding, re/analysis/data/safedisc_padding.md),
  * every call of playAudioFile 0x0043cce0 (the only caller of BinkPlayer::play 0x00487090) with its path argument,
  * every string naming a movie file in the four modules and in the install's text / data files,
  * every call of loadFlic 0x0043d740 with its (slot, name, frames, flag, shadow) arguments and the FLIC directory it names.

    ~/bin/py -3.12 re/tools/movie_inventory.py            # rewrite re/port/movies.tsv and re/port/flics_census.tsv
    ~/bin/py -3.12 re/tools/movie_inventory.py --check    # compare the committed TSVs with a fresh derivation
"""
import argparse, csv, hashlib, pathlib, re, struct, sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
ORIG = ROOT / "original"
MODULES = ["golf_clean.exe", "jgld.dll", "Terrain.dll", "sound.dll"]
PLAY_AUDIO_FILE = 0x0043CCE0   # the only caller of BinkPlayer::play
BINK_PLAY = 0x00487090
LOAD_FLIC = 0x0043D740
SERVICE_MOVIE = 0x0049C8E0
MOVIE_TABLE, MOVIE_COUNT = 0x00840698, 0x008406E4
MOVIE_RE = re.compile(rb"[\x20-\x7e]{0,60}(?:\.bik|\.bk2|\.smk|\.avi|\.mov|\.mpg|\.mpeg|\.flc|\.fli)\b[\x20-\x7e]{0,20}", re.I)
BINK_RE = re.compile(rb"[\x20-\x7e]{0,40}bink[\x20-\x7e]{0,40}", re.I)
INSTALL_TEXT_EXT = {".txt", ".ini", ".srf", ".cse", ".scc", ".md", ".bat", ".ion", ".axt", ".pro", ".sve", ".chr", ".glf", ".dta", ".fot"}


def functions():
    """(lo, hi, entry, name) for every range of the exe's function list."""
    out = []
    for r in csv.DictReader(open(ROOT / "re" / "functions_ghidra.tsv"), delimiter="\t"):
        for rg in (r.get("ranges") or "").split(";"):
            if "-" in rg:
                lo, hi = (int(x, 16) for x in rg.split("-"))
                out.append((lo, hi, int(r["entry"], 16), r["name"]))
    return sorted(out)


class Exe:
    def __init__(self, path=None):
        self.path = pathlib.Path(path or ORIG / "golf_clean.exe")
        self.pe = pefile.PE(str(self.path))
        self.pe.parse_data_directories()
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.text = [s for s in self.pe.sections if s.Name.startswith(b".text")][0]
        self.tdata = self.text.get_data()
        self.tva = self.base + self.text.VirtualAddress
        self.fns = functions()
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = False
        self._ins = {}

    def containing(self, va):
        for lo, hi, entry, name in self.fns:
            if lo <= va <= hi:
                return entry, name
        return None, None

    def cstring(self, va):
        d = self.pe.get_data(va - self.base, 160)
        return d.split(b"\0")[0].decode("latin-1")

    def imports(self, dll):
        out = []
        for e in getattr(self.pe, "DIRECTORY_ENTRY_IMPORT", []):
            if e.dll.decode().lower() == dll.lower():
                out += [(i.address, i.name.decode()) for i in e.imports]
        return out

    def instructions(self, entry):
        """address -> (mnemonic, op_str, size) for the function whose entry is `entry`, linear sweep of each range."""
        if entry in self._ins:
            return self._ins[entry]
        d = {}
        for lo, hi, e, _ in self.fns:
            if e == entry:
                off = lo - self.tva
                for i in self.md.disasm(self.tdata[off:off + (hi - lo + 1)], lo):
                    d[i.address] = (i.mnemonic, i.op_str, i.size)
        self._ins[entry] = d
        return d

    def dword_sites(self, value):
        """Every .text offset whose dword equals `value`, as (va, preceding two bytes)."""
        b = struct.pack("<I", value)
        out, i = [], self.tdata.find(b)
        while i >= 0:
            out.append((self.tva + i, bytes(self.tdata[max(0, i - 2):i])))
            i = self.tdata.find(b, i + 1)
        return out

    def calls_to(self, target):
        """E8 rel32 sites whose target is `target`, inside .text."""
        out, d = [], self.tdata
        i = d.find(b"\xe8")
        while 0 <= i <= len(d) - 5:
            rel = struct.unpack_from("<i", d, i + 1)[0]
            va = self.tva + i
            if (va + 5 + rel) & 0xFFFFFFFF == target:
                out.append(va)
            i = d.find(b"\xe8", i + 1)
        return out

    def pushes_before(self, call_va, n):
        """The n instructions before `call_va` in its function; each as (addr, mnemonic, op_str)."""
        entry, _ = self.containing(call_va)
        if entry is None:
            return None
        ins = self.instructions(entry)
        addrs = sorted(a for a in ins if a < call_va)
        got = []
        for a in reversed(addrs):
            m, o, _ = ins[a]
            got.append((a, m, o))
            if len(got) == n:
                break
        return list(reversed(got))


def imm(op):
    try:
        return int(op, 16) if op.startswith("0x") else int(op)
    except ValueError:
        return None


def push_value(exe, entry, at, op):
    """Value pushed at `at`: an immediate, or a register last written by `xor reg, reg` (0) in the same function."""
    v = imm(op)
    if v is not None:
        return v
    ins = exe.instructions(entry)
    for a in sorted((x for x in ins if x < at), reverse=True):
        m, o, _ = ins[a]
        if o.split(",")[0].strip() == op:
            parts = [x.strip() for x in o.split(",")]
            return 0 if m == "xor" and parts[0] == parts[1] else None
        if m in ("call", "ret", "jmp"):
            return None
    return None


def bink_imports():
    exe = Exe()
    res = {}
    for m in MODULES:
        pe = pefile.PE(str(ORIG / m))
        pe.parse_data_directories()
        lst = []
        for e in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
            if "bink" in e.dll.decode().lower():
                lst += [(i.address, i.name.decode()) for i in e.imports]
        res[m] = lst
    return exe, res


def slot_sites(exe, imports):
    """Every dword operand naming a binkw32 slot in .text: in a function or in padding."""
    rows = []
    for slot, name in imports:
        for va, prev in exe.dword_sites(slot):
            entry, fn = exe.containing(va)
            opcode = prev.hex() if len(prev) == 2 else "?"
            rows.append(dict(slot=slot, name=name, va=va - 2, opcode=opcode, fn=fn, entry=entry))
    return rows


def play_calls(exe):
    """Each call of playAudioFile with its two stack arguments (path, immediate)."""
    out = []
    for va in exe.calls_to(PLAY_AUDIO_FILE):
        entry, fn = exe.containing(va)
        if entry is None:
            out.append(dict(call=va, fn=None, entry=None, path_va=None, immediate=None))
            continue
        pre = exe.pushes_before(va, 2)
        # playAudioFile(path, immediate): path pushed last
        (a1, m1, o1), (a2, m2, o2) = pre[0], pre[1]
        assert m1 == "push" and m2 == "push", (hex(va), pre)
        out.append(dict(call=va, fn=fn, entry=entry, path_push=a2, path_va=imm(o2), immediate=push_value(exe, entry, a1, o1)))
    return out


def movie_strings(exe_only=False):
    """(module, va or file offset, text) for every movie-looking string of the four modules."""
    out = []
    for m in MODULES:
        data = (ORIG / m).read_bytes()
        pe = pefile.PE(str(ORIG / m))
        base = pe.OPTIONAL_HEADER.ImageBase
        for rx, kind in ((MOVIE_RE, "ext"), (BINK_RE, "bink")):
            for mt in rx.finditer(data):
                try:
                    va = base + pe.get_rva_from_offset(mt.start())
                except Exception:
                    va = None
                out.append((m, va, mt.group().decode("latin-1"), kind))
    return out


def install_text_hits():
    """Install files other than the four modules, the movies themselves and our own log that mention a movie."""
    hits = []
    rx = re.compile(rb"\.bik\b|\.bk2\b|\.smk\b|\.avi\b|smsg_|introfinal|closingfinal", re.I)
    for p in sorted(ORIG.rglob("*")):
        if not p.is_file() or p.name in MODULES or p.suffix.lower() in (".bik", ".flc"):
            continue
        if p.suffix.lower() not in INSTALL_TEXT_EXT and p.suffix.lower() not in (".dll", ".exe", ".log"):
            continue
        if rx.search(p.read_bytes()):
            hits.append(p.relative_to(ORIG).as_posix())
    return hits


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def find_ci(root, rel):
    """Case-insensitive lookup of a relative path under `root` (the game runs on Windows file names)."""
    cur = root
    for part in rel.replace("\\", "/").split("/"):
        if part in ("", "."):
            continue
        nxt = [c for c in cur.iterdir() if c.name.lower() == part.lower()] if cur.is_dir() else []
        if not nxt:
            return None
        cur = nxt[0]
    return cur


def flic_calls(exe):
    """Each call of loadFlic with the FLIC names pushed in the 14 instructions before it (stopping at a call or ret).

    The arms of a conditional merge into one call (0x00441b86), so a site can name several files. A pushed immediate
    counts as a name only when "<it>.flc" exists under original/Flics (case-insensitive, as on Windows); the slot is the
    first push of a small value after the name (arg 1) and is recorded only when the call has exactly one name."""
    out = []
    root = ORIG / "Flics"
    topdirs = {p.name.lower() for p in root.iterdir() if p.is_dir()}
    for va in exe.calls_to(LOAD_FLIC):
        entry, fn = exe.containing(va)
        row = dict(call=va, fn=fn, entry=entry, names=[], pushes=[])
        if entry is None:
            out.append(row)
            continue
        ins = exe.instructions(entry)
        for a in sorted((x for x in ins if x < va), reverse=True)[:14]:
            m, o, _ = ins[a]
            if m in ("call", "ret"):
                break
            v = imm(o) if m == "push" else None
            if v is None:
                continue
            row["pushes"].append(v)
            if exe.base <= v < exe.base + 0x500000 and not (exe.tva <= v < exe.tva + len(exe.tdata)):
                try:
                    name = exe.cstring(v)
                except Exception:
                    continue
                if not name:
                    continue
                if find_ci(root, name.replace("\\", "/") + ".flc"):
                    row["names"].append((v, name))
                elif name.replace("/", "\\").split("\\")[0].lower() in topdirs and "\\" in name:
                    row.setdefault("missing", []).append((v, name))
        out.append(row)
    return out


MOVIE_COLS = ["kind", "subject", "movie", "path_string", "string_va", "path_reach", "caller", "caller_entry", "call_site",
              "arg_immediate", "trigger", "skip_input", "missing_file", "bink_open_null", "in_original", "install_media",
              "size", "reason"]
FLIC_COLS = ["directory", "flc_files", "call_sites", "named_files_found", "named_files_missing", "shadow_variants",
             "max_width", "max_height", "max_frames", "not_flc_magic", "unreferenced_files", "classification", "loaders", "note"]


def derive():
    exe, imp = bink_imports()
    d = dict(exe=exe, imports=imp)
    d["sites"] = slot_sites(exe, imp["golf_clean.exe"])
    d["play"] = play_calls(exe)
    d["strings"] = movie_strings()
    d["install_hits"] = install_text_hits()
    d["flic"] = flic_calls(exe)
    d["play_callers_bink"] = exe.calls_to(BINK_PLAY)
    d["flc_dirs"] = sorted(p.name for p in (ORIG / "Flics").iterdir() if p.is_dir())
    d["service_refs"] = [va for va, _ in exe.dword_sites(MOVIE_TABLE)] + [va for va, _ in exe.dword_sites(MOVIE_COUNT)]
    return d


def trigger_of(entry, name):
    return {
        0x0043CD70: "showTitleScreen: first act of the title screen build, called by gameMain 0x0045baf0 at 0x0045bf0f, once per process before the menu",
        0x0040F5C0: "mainLoop QUIT block 0x0040fa29..0x0040fac8: after top10.sve and promptAndSaveFile, before showCreditsScreen, once on leaving the game",
    }.get(entry, name)


def movies_rows(d):
    exe = d["exe"]
    rows = []
    for p in sorted(d["play"], key=lambda r: r["call"]):
        text = exe.cstring(p["path_va"])
        fn = find_ci(ORIG, text)
        base = pathlib.PurePosixPath(text).name
        media = ROOT / "extracted" / "cab" / "Program_Files_(ENGLISH)" / "Flics" / base
        mrow = find_ci(media.parent, base) if media.parent.exists() else None
        imm_ = p["immediate"]
        rows.append(dict(
            kind="movie", subject=f"playAudioFile call 0x{p['call']:08x}", movie=base,
            path_string=text, string_va=f"0x{p['path_va']:08x}",
            path_reach=(f"push 0x{p['path_va']:08x} at 0x{p['path_push']:08x} -> playAudioFile 0x0043cce0 [esp+8] -> fileExists 0x004378a0 "
                        "(FindFirstFileA 0x004ba090) -> BinkPlayer::play 0x00487090 [esp+4] -> _BinkOpen@8 slot 0x004ba250 at 0x004870cd (flags 0x1000000)"),
            caller=p["fn"], caller_entry=f"0x{p['entry']:08x}", call_site=f"0x{p['call']:08x}",
            arg_immediate=str(imm_),
            trigger=trigger_of(p["entry"], p["fn"]),
            skip_input=("key or click at any time (deadline 0x005aa774 = 0 at 0x0043cd0c: rootWin_onPoint / onUnpoint / onKey clear 0x0083ab9c)" if imm_
                        else "key or click only after 7 s (deadline 0x005aa774 = timeGetTime + 0x1b58 at 0x0043cd00; the three root handlers clear 0x0083ab9c only when timeGetTime is above it)"),
            missing_file="playAudioFile returns at 0x0043ccf0 (fileExists = 0): no deadline, no window change, no Bink call; the caller continues with no movie",
            bink_open_null="BinkPlayer::play answers 1 (0x00487101 / 0x004870d8): playAudioFile still runs BinkPlayer::close (a NULL handle is skipped, 0x0048706b) and clears the deadline; the caller continues with no movie",
            in_original="yes" if fn else "no", install_media="yes" if mrow else "no",
            size=str(fn.stat().st_size) if fn else "", reason=""))
    # excluded rows
    def ex(subject, reason, **kw):
        r = {c: "" for c in MOVIE_COLS}
        r.update(kind="excluded", subject=subject, reason=reason, **kw)
        rows.append(r)
    imp = d["imports"]
    for m in MODULES:
        if m != "golf_clean.exe":
            ex(f"binkw32 imports of {m}", f"{m} has {len(imp[m])} binkw32 imports and no movie string")
    # slot sites
    byslot = {}
    for s in sorted(d["sites"], key=lambda r: r["va"]):
        byslot.setdefault(s["slot"], []).append(s)
    for slot, name in imp["golf_clean.exe"]:
        for s in byslot.get(slot, []):
            if s["entry"] is not None:
                ex(f"import {name} slot 0x{slot:08x} used at 0x{s['va']:08x}",
                   f"inside {s['fn']} 0x{s['entry']:08x}: {'BinkPlayer::play / frame / close, reached only through playAudioFile' if s['fn'].startswith('BinkPlayer') else 'see function'}",
                   call_site=f"0x{s['va']:08x}", caller=s["fn"], caller_entry=f"0x{s['entry']:08x}")
            else:
                ex(f"import {name} slot 0x{slot:08x} used at 0x{s['va']:08x}",
                   f"SafeDisc padding slot (opcode {s['opcode']}) outside every function: re/analysis/data/safedisc_padding.md, nothing reaches it",
                   call_site=f"0x{s['va']:08x}")
    # strings
    seen = set()
    for m, va, text, kind in d["strings"]:
        key = (m, va, text)
        if key in seen:
            continue
        seen.add(key)
        t = text.lower()
        if m == "golf_clean.exe" and va is not None and ("smsg_" in t and t.endswith(".bik")):
            continue  # the two movie rows
        if "flics\\" in t and t.endswith(".flc") or t.endswith(".flc") or "flc" in t and kind == "ext":
            ex(f"{m} string 0x{va:08x} {text!r}", "FLIC sprite-animation name or suffix used by loadFlic 0x0043d740 (see re/port/flics_census.tsv)", string_va=f"0x{va:08x}")
        elif kind == "bink" and va is not None and 0x4BF2EA <= va <= 0x4BF43F:
            ex(f"{m} string 0x{va:08x} {text!r}", "import table name (hint/name entry or DLL name), not a path", string_va=f"0x{va:08x}")
        elif "$bink" in t:
            ex(f"{m} string 0x{va:08x} {text!r}", "credits.txt line marker compared by showCreditsScreen 0x0044b9c0 at 0x0044bc55: draws the logo Surface, no movie", string_va=f"0x{va:08x}")
        elif "bink64.pcx" in t:
            ex(f"{m} string 0x{va:08x} {text!r}", "logo image (PCX) loaded by showCreditsScreen at 0x0044ba7e, not a movie", string_va=f"0x{va:08x}")
        else:
            ex(f"{m} string 0x{va:08x} {text!r}", "UNCLASSIFIED", string_va=f"0x{va:08x}")
    for h in d["install_hits"]:
        reason = {
            "simgolf_shim.log": "our own shim log (names the movie files it saw the game open), not game data",
            "winmm.dll": "our shim (skip_intro): carries the text `introfinal` to recognise the intro movie, not game data",
            "golf_v100_clean.exe": "retail v1.00 exe kept for version diffing: the same two path strings as golf_clean.exe, not one of the four modules",
        }.get(h, "UNCLASSIFIED")
        ex(f"install file {h}", reason)
    ex("movie table 0x00840698 / count 0x008406e4 (serviceMovie 0x0049c8e0)",
       "movie objects of an RIFF / mmio player (not Bink): the table and count have no writer in any module (only serviceMovie reads them) and winmm has no mmioOpen import, so the count stays 0 and serviceMovie returns at 0x0049c8ea; no file name is attached",
       call_site="0x0049c8e0")
    return rows


def flic_rows(d):
    root = ORIG / "Flics"
    dirs = {x.lower(): x for x in d["flc_dirs"]}
    blank = lambda: dict(calls=0, found=set(), shadow=set(), loaders=set(), files=set())
    stats = {x: blank() for x in d["flc_dirs"]}
    for dname in d["flc_dirs"]:
        for p in (root / dname).rglob("*"):
            if p.is_file() and p.suffix.lower() == ".flc":
                stats[dname]["files"].add(p.relative_to(root).as_posix().lower())
    geom = {}
    for dname in d["flc_dirs"]:
        w = h = fr = bad = 0
        for p in (root / dname).rglob("*"):
            if p.is_file() and p.suffix.lower() == ".flc":
                hdr = p.read_bytes()[:12]
                size, magic, frames, width, height = struct.unpack_from("<IHHHH", hdr)
                if magic != 0xAF12:
                    bad += 1
                w, h, fr = max(w, width), max(h, height), max(fr, frames)
        geom[dname] = (w, h, fr, bad)
    unresolved, missing = [], {}
    for c in d["flic"]:
        for _, name in c.get("missing", []):
            top = dirs[name.replace("/", "\\").split("\\")[0].lower()]
            missing.setdefault(top, set()).add(name.lower())
            stats[top]["loaders"].add(c["fn"])
        if not c["names"]:
            if not c.get("missing"):
                unresolved.append(c)
            else:
                stats[dirs[c["missing"][0][1].replace("/", "\\").split("\\")[0].lower()]]["calls"] += 1
            continue
        touched = set()
        for _, name in c["names"]:
            rel = name.replace("\\", "/")
            top = dirs.get(rel.split("/")[0].lower())
            if top is None:
                unresolved.append(c)
                continue
            s = stats[top]
            s["found"].add((rel + ".flc").lower())
            if find_ci(root, rel + "Shadow.flc"):
                s["shadow"].add((rel + "Shadow.flc").lower())
            s["loaders"].add(c["fn"])
            touched.add(top)
        for t in touched:
            stats[t]["calls"] += 1
    rows = []
    for k, s in stats.items():
        named = s["found"] | s["shadow"]
        unref = sorted(s["files"] - named)
        if s["calls"]:
            cls = "S3 sprite animation: named by loadFlic call sites (frames go to the sprite sheet at asset load)"
        else:
            cls = "no loadFlic call site names a file of this directory"
        rows.append(dict(
            directory=k, flc_files=len(s["files"]), call_sites=s["calls"], named_files_found=len(s["found"]),
            named_files_missing=len(missing.get(k, ())), shadow_variants=len(s["shadow"]),
            max_width=geom[k][0], max_height=geom[k][1], max_frames=geom[k][2], not_flc_magic=geom[k][3], unreferenced_files=len(unref), classification=cls,
            loaders=";".join(sorted(s["loaders"])),
            note=("; ".join(filter(None, [("named but absent from the install: " + ", ".join(sorted(missing[k]))) if k in missing else "",
                                          ("first unnamed: " + ", ".join(unref[:3])) if unref else ""])))))
    rows.append(dict(directory="(unresolved call sites)", flc_files=0, call_sites=len(unresolved), named_files_found=0,
                     named_files_missing=0, shadow_variants=0, unreferenced_files=0,
                     classification="loadFlic call sites with no pushed name that resolves to an existing .flc",
                     loaders=";".join(sorted({c["fn"] or "outside" for c in unresolved})),
                     note=";".join(f"0x{c['call']:08x}" for c in unresolved[:12])))
    return rows


def verify(d, mrows, frows):
    """Completeness problems of the two tables against a fresh derivation (empty list = complete)."""
    exe, bad = d["exe"], []
    movies = [r for r in mrows if r["kind"] == "movie"]
    excl = [r for r in mrows if r["kind"] == "excluded"]
    # 1. every playAudioFile call is a movie row with the right call site, path and string address
    calls = {f"0x{p['call']:08x}": p for p in d["play"]}
    rows = {r["call_site"]: r for r in movies}
    if set(calls) != set(rows):
        bad.append(f"movie call sites {sorted(rows)} != derived {sorted(calls)}")
    for cs, p in calls.items():
        r = rows.get(cs)
        if r and (r["string_va"] != f"0x{p['path_va']:08x}" or r["path_string"] != exe.cstring(p["path_va"])):
            bad.append(f"{cs}: string address or text differs")
    # 2. every use of a binkw32 slot is a row of one kind or the other
    seen = {r["call_site"] for r in excl if r["call_site"]}
    for s in d["sites"]:
        if f"0x{s['va']:08x}" not in seen:
            bad.append(f"slot site 0x{s['va']:08x} {s['name']} has no row")
    # 3. the only caller of BinkPlayer::play is playAudioFile; the only callers of playAudioFile are the movie rows
    for va in d["play_callers_bink"]:
        if exe.containing(va)[0] != PLAY_AUDIO_FILE:
            bad.append(f"BinkPlayer::play called from 0x{va:08x}, outside playAudioFile")
    # 4. no other module imports binkw32 or names a movie
    for m in MODULES[1:]:
        if d["imports"][m]:
            bad.append(f"{m} imports binkw32")
    for m, va, text, kind in d["strings"]:
        if m != "golf_clean.exe" and kind == "ext" and not text.lower().endswith(".flc"):
            bad.append(f"{m} names a movie: {text}")
    # 5. nothing is left unclassified
    for r in excl:
        if r["reason"] == "UNCLASSIFIED":
            bad.append(f"unclassified: {r['subject']}")
    # 6. the movie table family has no writer: every dword naming it sits in serviceMovie
    for va in d["service_refs"]:
        if not (SERVICE_MOVIE <= va < SERVICE_MOVIE + 0x29):
            bad.append(f"table reference 0x{va:08x} outside serviceMovie")
    # 7. every FLIC directory has a row; every loadFlic call site has a resolved name
    dirs = {r["directory"] for r in frows}
    for x in d["flc_dirs"]:
        if x not in dirs:
            bad.append(f"flic directory {x} has no row")
    un = [r for r in frows if r["directory"] == "(unresolved call sites)"]
    if un and int(un[0]["call_sites"]):
        bad.append(f"{un[0]['call_sites']} loadFlic call sites without a name")
    # 8. the movie files exist in the install and in the extracted media
    for r in movies:
        if r["in_original"] != "yes" or r["install_media"] != "yes":
            bad.append(f"{r['movie']}: file not in original/ or the install media")
    return bad


def write_tsv(path, cols, rows):
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=cols, delimiter="\t", lineterminator="\n", extrasaction="ignore")
        w.writeheader()
        for r in rows:
            w.writerow({c: r.get(c, "") for c in cols})


def read_tsv(path):
    return list(csv.DictReader(open(path, encoding="utf-8"), delimiter="\t"))


MOVIES_TSV = ROOT / "re" / "port" / "movies.tsv"
FLICS_TSV = ROOT / "re" / "port" / "flics_census.tsv"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    d = derive()
    mr, fr = movies_rows(d), flic_rows(d)
    if a.check:
        same = read_tsv(MOVIES_TSV) == [{c: str(r.get(c, "")) for c in MOVIE_COLS} for r in mr] and \
            read_tsv(FLICS_TSV) == [{c: str(r.get(c, "")) for c in FLIC_COLS} for r in fr]
        print("OK" if same else "DIFFERENT")
        return 0 if same else 1
    write_tsv(MOVIES_TSV, MOVIE_COLS, mr)
    write_tsv(FLICS_TSV, FLIC_COLS, fr)
    print(len(mr), "movie rows,", len(fr), "flic rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
