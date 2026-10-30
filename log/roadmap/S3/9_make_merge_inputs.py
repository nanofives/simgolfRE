"""S3.9: write log/roadmap/S3/9_merge_inputs.md (the inputs of MERGE_w7.md) from the steps' merge notes and uncertainty TSVs.
The CLAUDE.md paragraphs are copied verbatim from each step's fenced block; the UNCERTAINTIES.md rows are put in the table's form
`| ID | addr | type | description | path to resolution | status |`: an address cell that is not one hex number moves into the
description as a `[...]` prefix (re_classify parses every addr cell), S3.16's TSV (id status where question evidence next) is mapped
onto the same columns, and U-1844 is marked resolved by S3.19 (19_merge.md section 3).
    ~/bin/py -3.12 log/roadmap/S3/9_make_merge_inputs.py"""
import pathlib
import re

HERE = pathlib.Path(__file__).resolve().parent


def fenced_after(md, heading):
    i = md.index(heading)
    a = md.index("```", i)
    a = md.index("\n", a) + 1
    b = md.index("```", a)
    return md[a:b].rstrip("\n")


def esc(s):
    return s.replace("|", "\\|").strip()


def row(uid, addr, typ, desc, path, status):
    if addr and not re.fullmatch(r"0x[0-9a-fA-F]+", addr):
        desc = f"[{addr}] {desc}"
        addr = ""
    return "| " + " | ".join(esc(c) for c in (uid, addr, typ, desc, path, status)) + " |"


def std_rows(tsv):   # id address kind description needed status
    out = []
    for l in (HERE / tsv).read_text(encoding="utf-8").splitlines():
        if not l.strip() or l.startswith("id\t"):
            continue
        c = l.split("\t")
        assert len(c) == 6, (tsv, l[:50])
        uid, addr, kind, desc, need, status = c
        if uid == "U-1844":
            status = "resolved"
            desc += " RESOLVED by roadmap S3.19: the bio EditBox is drawn (gfx_editbox.cpp, call oracle 300 / 300); the GameState word [+0x57c] continues as U-1846."
        out.append(row(uid, addr, kind, desc, need, status))
    return out


S316_TYPE = {"U-1820": "harness", "U-1821": "residue", "U-1822": "port-gap", "U-1823": "evidence", "U-1824": "residue",
             "U-1825": "port-gap"}


def s316_rows():   # id status where question evidence next
    out = []
    for l in (HERE / "16_uncertain.tsv").read_text(encoding="utf-8").splitlines():
        if not l.strip() or l.startswith("id\t"):
            continue
        c = l.split("\t")
        assert len(c) == 6, l[:50]
        uid, status, where, q, ev, nxt = c
        out.append(row(uid, where, S316_TYPE[uid], f"{q} {ev}", "" if nxt.strip() == "-" else nxt, status))
    return out


def main():
    m = {k: (HERE / f"{k}_merge.md").read_text(encoding="utf-8") for k in (15, 16, 17, 18, 19)}
    para = {17: fenced_after(m[17], "## 4. CLAUDE.md paragraph"), 18: fenced_after(m[18], "## 4. CLAUDE.md paragraph"),
            19: fenced_after(m[19], "## 4. CLAUDE.md paragraph"), 16: fenced_after(m[16], "## 3. CLAUDE.md paragraph"),
            15: fenced_after(m[15], "## 5. CLAUDE.md paragraph")}
    tick = [e for e in (lambda env: (exec((HERE / "17_edits.py").read_text(encoding="utf-8"), env), env)[1])({})["EDITS"]
            if e[0].endswith("tick.cpp")]
    assert len(tick) == 1
    unc = {17: std_rows("17_uncertain.tsv"), 18: std_rows("18_uncertain.tsv"), 19: std_rows("19_uncertain.tsv"),
           16: s316_rows(), 15: std_rows("15_uncertain.tsv"), 9: std_rows("9_uncertain.tsv")}
    tpl = (HERE / "9_merge_inputs.tpl.md").read_text(encoding="utf-8")
    out = tpl
    for k in para:
        out = out.replace(f"@@CLAUDE_{k}@@", "```\n" + para[k] + "\n```")
    for k in unc:
        out = out.replace(f"@@UNC_{k}@@", "```\n" + "\n".join(unc[k]) + "\n```")
    out = out.replace("@@TICK_OLD@@", "```\n" + tick[0][1].rstrip("\n") + "\n```")
    out = out.replace("@@TICK_NEW@@", "```\n" + tick[0][2].rstrip("\n") + "\n```")
    assert "@@" not in out, re.findall(r"@@\w+@@", out)
    (HERE / "9_merge_inputs.md").write_text(out, encoding="utf-8", newline="\n")
    print("wrote", HERE / "9_merge_inputs.md", sum(len(v) for v in unc.values()), "rows")


if __name__ == "__main__":
    main()
