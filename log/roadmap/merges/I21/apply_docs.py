"""I21: the shared-file text of MERGE_w8.md applied to the mirror's own CLAUDE.md and UNCERTAINTIES.md (port/build/I21).

UNCERTAINTIES.md: the 13 rows of log/roadmap/S3/w8_uncertain.md appended as they are (U-1834 is new at 2291a342); U-1370 set
`resolved` with 24_merge.md's sentence at the end of its path-to-resolution cell; U-1392 stays open with a pointer to U-1309.
CLAUDE.md: MERGE_w8.md section 4's five paragraphs after the S3.20 paragraph, in its order.

    ~/bin/py -3.12 log/roadmap/merges/I21/apply_docs.py      (refuses a second run: every anchor exactly once)
"""
import pathlib
import re
import sys
import textwrap

ROOT = pathlib.Path(__file__).resolve().parents[4]
M = ROOT / "port" / "build" / "I21"
S3 = ROOT / "log" / "roadmap" / "S3"


def load(p):
    t = p.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in t else "\n"
    return t.split(nl), nl


def save(p, lines, nl):
    p.write_bytes(nl.join(lines).encode("utf-8"))


def section(text, head):
    """the text after the line `head` up to the next `## ` heading"""
    parts = text.split(head, 1)
    assert len(parts) == 2, head
    return parts[1].split("\n## ", 1)[0]


def fenced(text):
    m = re.findall(r"```\n(.*?)\n```", text, re.S)
    return m


def uncertainties():
    u = M / "UNCERTAINTIES.md"
    lines, nl = load(u)
    rows = [l for l in (S3 / "w8_uncertain.md").read_text(encoding="utf-8").splitlines() if l.startswith("| U-")]
    assert len(rows) == 13, len(rows)
    for r in rows:
        rid = r.split("|")[1].strip()
        assert len(r.split("|")) == 8, rid
        assert not any(x.startswith("| " + rid + " ") for x in lines), rid
    # U-1370 resolved (24_merge.md)
    i = [k for k, x in enumerate(lines) if x.startswith("| U-1370 ")]
    assert len(i) == 1, i
    c = re.split(r"(?<!\\)\|", lines[i[0]])
    assert len(c) == 8 and c[6].strip() == "open", c[6]
    c[5] = (c[5].rstrip() + " [roadmap S3.24] the footprint is the per-column span of N = floor(w * L / \\|major\\|) + 2 pixels"
            " (log/roadmap/S3/24_notes.md); the end-cap and tie remainders are U-1826 / U-1827. ")
    c[6] = " resolved "
    lines[i[0]] = "|".join(c)
    # U-1392 stays open, narrowed to U-1309 (21_merge.md)
    i = [k for k, x in enumerate(lines) if x.startswith("| U-1392 ")]
    assert len(i) == 1, i
    c = re.split(r"(?<!\\)\|", lines[i[0]])
    assert len(c) == 8 and c[6].strip() == "open", c[6]
    c[5] = (c[5].rstrip() + " [roadmap S3.21] the vertex rule and clipping are modelled since S3.21; what is left is U-1309"
            " (the attribute interpolation). ")
    lines[i[0]] = "|".join(c)
    while lines and lines[-1] == "":
        lines.pop()
    lines += rows + [""]
    save(u, lines, nl)
    print("UNCERTAINTIES.md: +", len(rows), "rows, U-1370 resolved, U-1392 narrowed")


def claude():
    mw = (S3 / "MERGE_w8.md").read_text(encoding="utf-8")
    sec4 = section(mw, "## 4. CLAUDE.md paragraphs (in this order, after the S3.20 paragraph)")
    f = fenced(sec4)
    assert len(f) == 2 and f[0].startswith("Roadmap S3.23,") and f[1].startswith("S3 wave 8 heavy-run lock"), [x[:30] for x in f]
    s323 = f[0].split("\n")
    lead = f[1].split("\n")
    # item 1: S3.21 part 1 + the two sentences
    i1 = sec4.split("1. S3.21 part 1:", 1)[1].split("\n2. S3.24:", 1)[0]
    q = re.findall(r"`([^`]*)`", " ".join(x.strip() for x in i1.split("\n")))
    q = [x for x in q if x.startswith(("S3.21b", "Both part 1"))]
    assert len(q) == 2, q
    p21 = section((S3 / "21_merge.md").read_text(encoding="utf-8"),
                  "## CLAUDE.md (suggested paragraph, after S3.20's)").strip().split("\n")
    assert p21[0].startswith("Roadmap S3.21, U-1392 part 1") and p21[-1].endswith("not yet in GlRaster."), p21[-1][-40:]
    p21 = p21 + textwrap.wrap(q[0] + " " + q[1], 135, break_on_hyphens=False)
    p24 = section((S3 / "24_merge.md").read_text(encoding="utf-8"),
                  "## CLAUDE.md (after the S3.20 paragraph)").strip().split("\n")
    assert p24[0].startswith("Roadmap S3.24,") and p24[-1].endswith("(U-1827)."), p24[-1][-40:]
    m26 = (S3 / "26_merge.md").read_text(encoding="utf-8")
    f26 = fenced(m26.split("### 4. CLAUDE.md paragraph", 1)[1])
    assert len(f26) == 1 and f26[0].startswith("Roadmap S3.26,"), f26
    p26 = f26[0]
    old = "storing one needs an exe .rdata address: the lead's decision"
    assert p26.count(old) == 1
    p26 = p26.replace(old, "kept as an explained frame divergence on cov_k3_camera y_inj3 (orchestrator, 2026-10-31)").split("\n")
    i5 = sec4.split("5. The lead's process paragraph", 1)[1].split("\n\nS3.23:", 1)[0]
    end = re.findall(r"`([^`]*)`", " ".join(x.strip() for x in i5.split("\n")))
    assert len(end) == 1 and end[0].startswith("Merged-tree census on 2291a342"), end
    lead = lead + textwrap.wrap(end[0], 135, break_on_hyphens=False)
    paras = [p21, p24, s323, p26, lead]
    for p in paras:
        for x in p[1:]:
            assert not x.startswith(("Roadmap ", "Section ", "Phase ", "### ", "## ")), x[:40]
    c = M / "CLAUDE.md"
    lines, nl = load(c)
    st = [k for k, x in enumerate(lines) if x.startswith("Roadmap S3.20,")]
    assert len(st) == 1, st
    for p in paras:
        assert not any(x.startswith(p[0][:40]) for x in lines), p[0][:40]
    j = st[0] + 1
    while not lines[j].startswith(("Roadmap ", "Section ", "Phase ", "Function bodies are not always contiguous", "### ", "## ")):
        j += 1
    assert lines[j].startswith("Section S4, audio engine model"), lines[j][:60]
    lines[j:j] = [x for p in paras for x in p]
    save(c, lines, nl)
    print("CLAUDE.md: +", len(paras), "paragraphs after line", j)


if __name__ == "__main__":
    uncertainties()
    claude()
    sys.exit(0)
