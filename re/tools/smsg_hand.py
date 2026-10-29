"""Make a hand-fix wrapper (S10.2-4): copy a generated wip wrapper's header, point COMPILE at a hand copy of the source.

usage: smsg_hand.py <wip wrapper> <hand source copy> <out wrapper> <reason> <addr>...
Only the header (up to and including the COMPILE line) is taken from the wrapper; MATCH lines come from the
wrapper lines of the listed addresses with the trailing `// miss ...` comment dropped.
"""
import pathlib
import re
import sys


def main(argv):
    wip, src, out, reason, *addrs = argv
    lines = pathlib.Path(wip).read_text().splitlines()
    head = []
    for ln in lines:
        if ln.startswith("// COMPILE"):
            head.append("// HAND-FIX: " + reason)
            head.append("// COMPILE " + src)
            break
        head.append(ln)
    body = []
    for ln in lines:
        if ln.startswith("// MATCH:") and any(a.lower() in ln.lower() for a in addrs):
            body.append(re.sub(r"\s+//\s*(miss|SMSG).*$", "", ln))
    pathlib.Path(out).write_text("\n".join(head + body) + "\n")


if __name__ == "__main__":
    main(sys.argv[1:])
