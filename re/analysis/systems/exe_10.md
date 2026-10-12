# exe_10: the game date (2026-10-11)

`g_date` 0x00834170 is the game calendar. appendDate 0x0040d7b0 turns a date into `<month> <year>`: year = date /
8192 (shown as year + 2001) and month = date / 1024 % 8 (an 8-month year), and drawEmployeeDetailPanel 0x00436e50 passes
this dword to it. logTick 0x0040c6f0 writes it. Under SIMGOLF_TIMEWARP=32 it rose from 0x0e97 (2001, month 3) to 0x2b0a
(2002, month 2) in 45 s of the course_season steps; at x1 the same steps moved it by 493. tests/test_scenarios.py's
time-warp test reads it instead of comparing the date line's pixels.
