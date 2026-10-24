"""Defect profiles over the sg_defect compatibility switches (roadmap S6.3).

    from port.test import profiles as P
    P.apply(lib, "original")                  # every switch 1 = the oracle, identical to running with no profile
    P.apply(lib, "fixed")                     # the switches listed in re/port/defect_profiles.tsv
    P.apply(lib, "fixed", path=tmp_tsv)       # a fixture list (tests)
    ~/bin/py -3.12 -m port.test.profiles validate [--dll PATH] [--file TSV]

`original` is fixed by definition (no member list, never editable). `fixed` reads its members from the TSV (switch, value,
decision_id, reason). Everything goes through lib.defect() / lib.defect_reset(), i.e. sg_defect_set / sg_defect_reset: no C ABI
addition is needed (a native sg_profile_set / sg_profile_get is proposed in log/roadmap/S6/3_merge.md).
"""
from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

DEFAULT_TABLE = ROOT / "re" / "port" / "defect_profiles.tsv"
PROFILES = ("original", "fixed")
EDITABLE = ("fixed",)
COLUMNS = ["switch", "value", "decision_id", "reason"]


class ProfileError(ValueError):
    pass


def load_table(path=None) -> list[dict]:
    """The member rows of `fixed`. Header row required; `#` lines and blank lines ignored; value 0 or 1; no duplicate switch;
    decision_id and reason non-empty."""
    p = pathlib.Path(path or DEFAULT_TABLE)
    if not p.exists():
        raise ProfileError(f"{p}: profile table not found")
    lines = [ln.rstrip("\r\n") for ln in p.read_text(encoding="utf-8").splitlines()]
    lines = [ln for ln in lines if ln.strip() and not ln.lstrip().startswith("#")]
    if not lines or lines[0].split("\t") != COLUMNS:
        raise ProfileError(f"{p}: header must be {chr(9).join(COLUMNS)}")
    rows, seen = [], set()
    for n, ln in enumerate(lines[1:], 2):
        f = ln.split("\t")
        if len(f) != len(COLUMNS):
            raise ProfileError(f"{p}: row {n} has {len(f)} columns, expected {len(COLUMNS)}")
        r = dict(zip(COLUMNS, (x.strip() for x in f)))
        if r["value"] not in ("0", "1"):
            raise ProfileError(f"{p}: row {n} ({r['switch']}): value {r['value']!r} is not 0 or 1")
        if not r["switch"] or not r["decision_id"] or not r["reason"]:
            raise ProfileError(f"{p}: row {n}: switch, decision_id and reason are required")
        if r["switch"] in seen:
            raise ProfileError(f"{p}: row {n}: duplicate switch {r['switch']}")
        seen.add(r["switch"])
        r["value"] = int(r["value"])
        rows.append(r)
    return rows


def members(profile: str, path=None) -> dict[str, int]:
    """switch -> value of every switch the profile sets. `original` sets none (all switches stay at their default 1)."""
    if profile not in PROFILES:
        raise ProfileError(f"unknown profile {profile!r} (known: {', '.join(PROFILES)})")
    if profile == "original":
        if path is not None:
            raise ProfileError("profile `original` is not editable: it takes no member list")
        return {}
    return {r["switch"]: r["value"] for r in load_table(path)}


def validate(lib, path=None) -> dict[str, int]:
    """The `fixed` members checked against the library: a name the library does not have is an error (all reported)."""
    m = members("fixed", path)
    known = set(lib.defects())
    bad = sorted(s for s in m if s not in known)
    if bad:
        raise ProfileError(f"switches not present in the library: {', '.join(bad)}")
    return m


def apply(lib, profile: str, path=None) -> dict[str, int]:
    """Reset every switch to 1, then set the profile's members. Returns the members applied. Nothing is changed when the
    profile or its list is invalid."""
    if profile == "original":
        m = members("original", path)
    else:
        m = validate(lib, path) if profile == "fixed" else members(profile, path)
    lib.defect_reset()
    for s, v in m.items():
        lib.defect(s, bool(v))
    return m


def state(lib) -> dict[str, int]:
    return {n: d["on"] for n, d in lib.defects().items()}


def current(lib, path=None) -> str:
    """'original' when every switch is 1, 'fixed' when the state equals the `fixed` list applied to all-ones, else 'custom'."""
    st = state(lib)
    if all(v == 1 for v in st.values()):
        return "original"
    want = {n: 1 for n in st}
    want.update(members("fixed", path))
    return "fixed" if st == want else "custom"


def main(argv=None) -> int:
    import argparse
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("cmd", choices=["validate", "show"])
    ap.add_argument("--dll")
    ap.add_argument("--file")
    a = ap.parse_args(argv)
    from port.test import lib as L
    lib = L.load(a.dll) if a.dll else L.load()
    m = validate(lib, a.file)
    print(f"profile fixed: {len(m)} member(s) of {len(lib.defects())} switches")
    for s, v in m.items():
        print(f"  {s} = {v}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
