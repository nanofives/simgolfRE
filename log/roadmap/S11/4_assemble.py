"""S11.4: apply the deferred S10 renames (D1 `P` rows) to a tree, then regenerate the generated files.

    ~/bin/py -3.12 log/roadmap/S11/4_assemble.py --root <dir> [--names] [--no-regen] [--check-only]

Steps, in order:
  1. --names: the tracker part (4_names.py): the 70 name columns in <root>/re/names/*.tsv and `re_classify.py --root <root>
     retag` of exactly those rows (hooks.csv names, one CHANGELOG line each). On the real tree this is a merge step of its own
     (4_merge.md); on a test copy it makes hooks.csv / re/names agree with the port (test_audio_names, soundstate_gen, model_merge).
  2. 4_edits.py: every EDITS anchor must occur exactly once (texts are LF; a CRLF file is matched in LF and written back in CRLF);
     the whole list is checked before any file is written, and a second apply is refused (every new text already there, no old
     text left). NEW_FILES are created and refused when they exist.
  3. regeneration (skipped with --no-regen), each with cwd <root> and <root>'s own tools:
       re/tools/model_merge.py header            re/model/include/simgolf_model.h from the sections
       re/tools/model_ctypes_gen.py              re/model/model_ctypes.py
       re/tools/model_state_gen.py               port/include/gamestate.h, gamestate_meta.inc, gamestate_addresses.inc
       re/tools/soundstate_gen.py                port/include/portsim/sim/soundstate.h (names from hooks.csv: needs step 1)
  4. checks: each generator's --check, and `model_merge.py merge --out <tmp>` equal to re/model/addresses.tsv (line ends ignored).
--check-only reports which anchors are present / missing / already applied and writes nothing (use it after S3 wave 10 merges).
"""
from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
PY = sys.executable


def env_of(path: pathlib.Path) -> dict:
    env: dict = {}
    exec(path.read_text(encoding="utf-8"), env)
    return env


def load(p: pathlib.Path) -> tuple[str, bool]:
    t = p.read_bytes().decode("utf-8")
    crlf = "\r\n" in t
    return (t.replace("\r\n", "\n") if crlf else t), crlf


def plan(root: pathlib.Path, edits, new_files):
    """Check every anchor against the tree with the edits applied in order; return (texts, problems, state)."""
    texts: dict[str, list] = {}
    missing, ambiguous, applied = [], [], 0
    for k, (rel, old, new) in enumerate(edits, 1):
        if rel not in texts:
            p = root / rel
            if not p.exists():
                missing.append((k, rel, "file missing"))
                continue
            texts[rel] = list(load(p))
        t = texts[rel][0]
        c = t.count(old)
        if c == 1:
            texts[rel][0] = t.replace(old, new, 1)
        elif c == 0:
            if new and t.count(new) >= 1:
                applied += 1
            else:
                missing.append((k, rel, old.split("\n")[0][:100]))
        else:
            ambiguous.append((k, rel, c, old.split("\n")[0][:100]))
    exists = [rel for rel in new_files if (root / rel).exists()]
    return texts, missing, ambiguous, applied, exists


def run(root: pathlib.Path, *args: str) -> None:
    print("+", " ".join(args), flush=True)
    subprocess.run([PY, *args], cwd=root, check=True)


def main() -> None:
    a = sys.argv
    root = pathlib.Path(a[a.index("--root") + 1]).resolve()
    assert (root / "port" / "src" / "sim").is_dir(), root
    env = env_of(HERE / "4_edits.py")
    edits, new_files = env["EDITS"], env["NEW_FILES"]
    if "--names" in a and "--check-only" not in a:
        run(root, str(HERE / "4_names.py"), "--root", str(root))
    # a second apply: every edit's new text is already in its file and every new file exists
    cache: dict[str, str] = {}
    for rel, _, _ in edits:
        if rel not in cache and (root / rel).exists():
            cache[rel] = load(root / rel)[0]
    if all(rel in cache and new in cache[rel] for rel, _, new in edits) and all((root / r).exists() for r in new_files):
        raise SystemExit(f"refused: all {len(edits)} edits and {len(new_files)} new files are already in {root} (second apply)")
    texts, missing, ambiguous, applied, exists = plan(root, edits, new_files)
    print(f"edits {len(edits)}: to apply {len(edits) - len(missing) - len(ambiguous) - applied}, already applied {applied}, "
          f"missing {len(missing)}, ambiguous {len(ambiguous)}; new files {len(new_files)}, existing {len(exists)}")
    for k, rel, why in missing:
        print(f"  MISSING edit {k} {rel}: {why}")
    for k, rel, c, first in ambiguous:
        print(f"  AMBIGUOUS edit {k} {rel} ({c}x): {first}")
    for rel in exists:
        print(f"  EXISTS {rel}")
    if "--check-only" in a:
        sys.exit(0 if not (missing or ambiguous) else 1)
    if applied == len(edits) and len(exists) == len(new_files):
        raise SystemExit("refused: every edit is already applied (second apply)")
    if missing or ambiguous or exists or applied:
        raise SystemExit("refused: the tree does not match the edits' base (nothing written)")
    for rel, (t, crlf) in texts.items():
        data = t.replace("\n", "\r\n") if crlf else t
        (root / rel).write_bytes(data.encode("utf-8"))
    for rel, content in new_files.items():
        p = root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(content.encode("utf-8"))
    print(f"wrote {len(texts)} files, created {len(new_files)}")
    if "--no-regen" in a:
        return
    run(root, "re/tools/model_merge.py", "header")
    run(root, "re/tools/model_ctypes_gen.py")
    run(root, "re/tools/model_state_gen.py")
    run(root, "re/tools/soundstate_gen.py")
    for args in (("re/tools/model_merge.py", "header", "--check"), ("re/tools/model_ctypes_gen.py", "--check"),
                 ("re/tools/model_state_gen.py", "--check"), ("re/tools/soundstate_gen.py", "--check")):
        run(root, *args)
    with tempfile.TemporaryDirectory() as td:
        out = pathlib.Path(td) / "addresses.tsv"
        run(root, "re/tools/model_merge.py", "merge", "--out", str(out))
        same = out.read_text(encoding="utf-8").replace("\r\n", "\n") == load(root / "re/model/addresses.tsv")[0]
        print("model_merge merge reproduces re/model/addresses.tsv:", same)
        if not same:
            raise SystemExit("model_merge merge does not reproduce re/model/addresses.tsv (run 4_names.py first?)")


if __name__ == "__main__":
    main()
