# DEFERRED

Work explicitly out of scope for now. Each row has a re-pickup condition.

| ID | item | why deferred | re-pickup when |
|----|------|--------------|----------------|
| D-0001 | Unwrap `golf.v100.safedisc.exe` (retail v1.00) | v1.03 is the target; v1.00 only matters for patch-diffing the 1.01-1.03 changes | a v1.03 behaviour needs its pre-patch counterpart |
| D-0002 | Canonical-scenario harness for C4 (`log/diff/<addr>_*.scenario.csv`) | needs a live, reachable reimplementation first; the first hooked function is dead code | the first reimplementation of a function with runtime callers reaches C3 |
| D-0003 | Multiplayer / DirectPlay paths | not exercised by single-player tests | a patch touches networking |
| D-0004 | ~~Matching decompilation~~ RESOLVED 2026-10-02 | user provided `Visual Studio 6.iso`; cl 12.00.8168 extracted to tools/vc6; `re/tools/match.py` (own matcher; reccmp not needed for per-function checks) | - |
