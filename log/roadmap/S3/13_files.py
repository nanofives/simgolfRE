"""S3.13: write log/roadmap/S3/files_w5b.txt = files_w5.txt (S3.11's list) + the files 13_edits.py writes + MERGE_w5b.md /
files_w5b.txt + every log/roadmap/S3/13_* path (directories with a trailing /).

    ~/bin/py -3.12 log/roadmap/S3/13_files.py"""
import pathlib

S3 = pathlib.Path(__file__).resolve().parent


def main():
    seen, out = set(), []

    def add(p):
        if p and p not in seen:
            seen.add(p)
            out.append(p)

    for l in (S3 / "files_w5.txt").read_text(encoding="utf-8").splitlines():
        add(l.strip())
    env = {}
    exec((S3 / "13_edits.py").read_text(encoding="utf-8"), env)
    for rel, _, _ in env["EDITS"]:
        add(rel)
    add("CLAUDE.md")
    for p in ("log/roadmap/S3/MERGE_w5b.md", "log/roadmap/S3/files_w5b.txt"):
        add(p)
    for p in sorted(S3.glob("13_*")):
        add("log/roadmap/S3/" + p.name + ("/" if p.is_dir() else ""))
    (S3 / "files_w5b.txt").write_text("\n".join(out) + "\n", encoding="utf-8")
    print(len(out), "lines ->", S3 / "files_w5b.txt")


if __name__ == "__main__":
    main()
