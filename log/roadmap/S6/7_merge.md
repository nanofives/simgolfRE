# S6.7 merge lines (D42c, loadGolfPro.negativeWindowBound)

## re/port/defect_profiles.tsv: append after the loadGolfPro.divideAndRejectGuard row (tab separated)
```
loadGolfPro.negativeWindowBound	0	D42c	difficulty 5 or 6 (4 - difficulty < 0): once the falling acceptance bound tries / (4 - difficulty) + rep + cash is under every value the roster can give, the draw ends with pick 0 instead of never ending
```

## port/test/integrator.py: the switch -> evidence map (next to "loadGolfPro.divideAndRejectGuard", around line 872)
```
                 "loadGolfPro.negativeWindowBound": "port/test/test_defects_golfers_w2.py::test_probe_course_season_hangs_on_returns_off, "
                                                    "test_synthetic_hang_on_returns_off (state)",
```

## UNCERTAINTIES.md: rows U-1591..U-1592 from log/roadmap/S6/7_uncertain.tsv
