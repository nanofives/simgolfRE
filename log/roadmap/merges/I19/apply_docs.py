"""I19: apply S3 wave 7's documents (log/roadmap/S3/MERGE_w7.md sections C and D) to a tree's CLAUDE.md and UNCERTAINTIES.md.
Usage: ~/bin/py -3.12 apply_docs.py <tree root>. Every insert refuses to run twice or when its anchor is not found exactly once.

CLAUDE.md: the paragraphs of S3.17, S3.18, S3.19, S3.16, S3.15 (Appendix 1 sections 1, 3, 4, 5, 7) after S3.12's paragraph, each
after the previous one, then S3.20's (Appendix 2 section 5) after S3.15's.
UNCERTAINTIES.md: the rows of Appendix 1 sections 1, 3, 4, 5, 7 and 10, then S3.20's 20_uncertain.tsv. U-1320 / U-1321 go in as
resolved: P1 (c7412150) fixed them and I19's port suite runs both files GREEN on the merged tree."""
import pathlib
import re
import sys

T = pathlib.Path(sys.argv[1])
S3 = pathlib.Path(__file__).resolve().parents[2] / "S3"
MD = (S3 / "MERGE_w7.md").read_text(encoding="utf-8").splitlines()
RESOLVED = {
    "U-1320": "resolved: P1 (c7412150) regenerated soundstate.h; test_audio_core passes on I19 (log/roadmap/merges/I19.md)",
    "U-1321": "resolved: P1 (c7412150) renamed the 43 snd.FUN_ keys; test_audio_names passes on I19 (log/roadmap/merges/I19.md)",
}


def block_after(line_prefix, start=0):
    """The fenced block that follows the first line starting with line_prefix at or after index start."""
    i = next(k for k in range(start, len(MD)) if MD[k].startswith(line_prefix))
    a = next(k for k in range(i + 1, len(MD)) if MD[k].startswith("```"))
    b = next(k for k in range(a + 1, len(MD)) if MD[k].startswith("```"))
    return MD[a + 1:b], b


def section(title):
    return next(k for k, s in enumerate(MD) if s.startswith(title))


def read(rel):
    raw = (T / rel).read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in raw else "\n"
    return raw.replace("\r\n", "\n"), nl


def write(rel, s, nl):
    (T / rel).write_bytes(s.replace("\n", nl).encode("utf-8"))
    print("wrote", rel)


# CLAUDE.md
paras = []
for title in ("## 1. S3.17", "## 3. S3.18", "## 4. S3.19", "## 5. S3.16", "## 7. S3.15"):
    lines, _ = block_after("CLAUDE.md paragraph", section(title))
    assert lines[0].startswith("Roadmap " + title.split()[2].rstrip(":")), (title, lines[0][:60])
    paras.append("\n".join(lines) + "\n")
lines20, _ = block_after("## 5. CLAUDE.md paragraph")
assert lines20[0].startswith("Roadmap S3.20"), lines20[0][:60]
paras.append("\n".join(lines20) + "\n")

s, nl = read("CLAUDE.md")
anchor = "about 1 run of 6 without workers too (U-1399).\n"
assert s.count(anchor) == 1, s.count(anchor)
for p in paras:
    assert p.split(",")[0] not in s, ("already applied", p[:40])
s = s.replace(anchor, anchor + "".join(paras))
write("CLAUDE.md", s, nl)

# UNCERTAINTIES.md
rows = []
for title in ("## 1. S3.17", "## 3. S3.18", "## 4. S3.19", "## 5. S3.16", "## 7. S3.15"):
    lines, _ = block_after("UNCERTAINTIES.md rows", section(title))
    rows += lines
lines, _ = block_after("## 10. S3.9's UNCERTAINTIES.md rows")
rows += lines
for line in (S3 / "20_uncertain.tsv").read_text(encoding="utf-8").splitlines()[1:]:
    c = line.split("\t")
    assert len(c) == 6 and "|" not in line, line[:40]
    assert c[1] == "" or re.fullmatch(r"0x[0-9a-f]+", c[1]), c[1]
    rows.append(f"| {c[0]} | {c[1]} | {c[2]} | {c[3]} | {c[4]} | {c[5]} |")
out = []
for r in rows:
    assert r.startswith("| U-") and r.endswith("|"), r[:60]
    rid = r.split("|")[1].strip()
    if rid in RESOLVED:
        cells = r.split(" | ")
        cells[-1] = RESOLVED[rid] + " |"
        r = " | ".join(cells)
    out.append(r)
ids = [r.split("|")[1].strip() for r in out]
assert len(ids) == len(set(ids)), "duplicate id"
s, nl = read("UNCERTAINTIES.md")
for i in ids:
    assert f"| {i} |" not in s, ("already present", i)
s = s.rstrip("\n") + "\n" + "\n".join(out) + "\n"
write("UNCERTAINTIES.md", s, nl)
print(len(paras), "paragraphs,", len(out), "rows:", " ".join(ids))
