"""S3.14: run one pytest command (argv after `--`) while N busy-loop processes load the CPU at normal priority, then stop
them. Measures a test's sensitivity to load (I12 ran the app files next to other gates).

    ~/bin/py -3.12 log/roadmap/S3/14_load.py <N> -- <command ...>
"""
import multiprocessing as mp
import subprocess
import sys
import time


def hog():
    x = 0
    while True:
        x = (x * 1103515245 + 12345) & 0xFFFFFFFF


if __name__ == "__main__":
    n = int(sys.argv[1])
    cmd = sys.argv[sys.argv.index("--") + 1:]
    procs = [mp.Process(target=hog, daemon=True) for _ in range(n)]
    for p in procs:
        p.start()
    t0 = time.time()
    try:
        rc = subprocess.call(cmd)
    finally:
        for p in procs:
            p.terminate()
        for p in procs:
            p.join(5)
    print(f"load {n} procs, command rc {rc}, {time.time() - t0:.1f} s", flush=True)
    sys.exit(rc)
