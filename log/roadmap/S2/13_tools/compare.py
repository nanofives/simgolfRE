"""S2.13 comparisons.

  segs <baseline.tsv> <new.tsv>         segment gate rows: status and digest per name (0 differ = unchanged)
  census <new.tsv> <step.tsv>...        framegate census: each row of new against every step run; a row is WORSE than a step
                                        run when it left PASS, or its total_px grew, or its status changed otherwise
                                        (3_compare_fg.py's rule); prints per step and the rows worse than the BEST step value
  boot <baseline.tsv> <new.tsv>         boot gate rows: status, records, expected per scenario
"""
import csv
import sys


def load(p, key):
    with open(p, encoding="utf-8") as f:
        return {key(r): r for r in csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t")}


def segs(a, b):
    A, B = load(a, lambda r: r["name"]), load(b, lambda r: r["name"])
    diff = [n for n in sorted(set(A) & set(B)) if (A[n]["status"], A[n]["digest"]) != (B[n]["status"], B[n]["digest"])]
    st = {}
    for r in B.values():
        st[r["status"]] = st.get(r["status"], 0) + 1
    print(f"{len(set(A) & set(B))} rows in both, {len(diff)} differ; new statuses {st}; only base {len(set(A) - set(B))}, "
          f"only new {len(set(B) - set(A))}")
    for n in diff:
        print(f"DIFF\t{n}\t{A[n]['status']} {A[n]['digest']} -> {B[n]['status']} {B[n]['digest']}")
    return 1 if diff else 0


def px(r):
    try:
        return int(r["total_px"])
    except (ValueError, TypeError, KeyError):
        return None


def rank(r):
    """smaller is better: PASS first, then fewer px"""
    p = px(r)
    return (0 if r["status"] == "PASS" else 1, p if p is not None else 10 ** 9)


def census(new, *steps):
    N = load(new, lambda r: (r["scenario"], r["checkpoint"]))
    S = [(s, load(s, lambda r: (r["scenario"], r["checkpoint"]))) for s in steps]
    for name, T in S:
        worse = better = same = 0
        for k in set(N) & set(T):
            a, b = rank(T[k]), rank(N[k])
            if b < a:
                better += 1
            elif b > a or T[k]["status"] != N[k]["status"]:
                worse += 1
            else:
                same += 1
        print(f"against {name}: same {same}, better {better}, worse {worse}, missing {len(set(T) - set(N))}")
    worst = []
    for k in sorted(N):
        best = min((T[k] for _, T in S if k in T), key=rank, default=None)
        if best is None:
            continue
        if rank(N[k]) > rank(best) or (rank(N[k]) == rank(best) and N[k]["status"] != best["status"]):
            worst.append((k, best["status"], px(best), N[k]["status"], px(N[k]), N[k].get("first_unsupported", "")))
    st = {}
    for r in N.values():
        st[r["status"]] = st.get(r["status"], 0) + 1
    print(f"new: {len(N)} rows, statuses {st}; rows worse than the best step value: {len(worst)}")
    for k, sa, pa, sb, pb, fu in worst:
        print(f"WORSE_THAN_BEST\t{k[0]}\t{k[1]}\t{sa} {pa} -> {sb} {pb}\t{fu}")
    return 0


def boot(a, b):
    A, B = load(a, lambda r: r["scenario"]), load(b, lambda r: r["scenario"])
    diff = []
    for n in sorted(set(A) & set(B)):
        if (A[n]["status"], A[n]["records"], A[n]["expected"]) != (B[n]["status"], B[n]["records"], B[n]["expected"]):
            diff.append(n)
    st = {}
    for r in B.values():
        st[r["status"]] = st.get(r["status"], 0) + 1
    print(f"{len(set(A) & set(B))} rows in both, {len(diff)} differ; new statuses {st}; only base {sorted(set(A) - set(B))}, "
          f"only new {sorted(set(B) - set(A))}")
    for n in diff:
        print(f"DIFF\t{n}\t{A[n]['status']} {A[n]['records']}/{A[n]['expected']} -> {B[n]['status']} {B[n]['records']}/"
              f"{B[n]['expected']}\t{B[n].get('detail', '')[:200]}")
    return 0


if __name__ == "__main__":
    cmd, *rest = sys.argv[1:]
    sys.exit({"segs": segs, "census": census, "boot": boot}[cmd](*rest))
