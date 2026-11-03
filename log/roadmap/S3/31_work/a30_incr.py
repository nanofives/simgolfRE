"""float32 incremental schemes for the side distance n against the measured pixels of line (42, 15) w 2 (a at the origin)"""
import numpy as np
f32 = np.float32
meas = {(36, 12): (-1.87e-06, -1.60e-06), (33, 12): (1.75e-06, 2.02e-06), (30, 11): (1.31e-06, 1.52e-06)}
dx, dy = 42.0, 15.0
L = np.sqrt(dx * dx + dy * dy)
def exact(x, y):
    px, py = x + 0.5, y + 0.5
    return (-px * dy + py * dx) / L
def schemes():
    Lf = f32(np.sqrt(f32(dx * dx + dy * dy)))
    ndx, ndy = f32(-f32(dy) / Lf), f32(f32(dx) / Lf)   # dn per +x, per +y
    s = dy / dx
    out = {}
    # A: n at the first column's pixel centre on row 0 (0.5, 0.5), stepped by columns then rows (float accumulation)
    def acc(start_x, start_y, x, y, row_first=False):
        n = f32(exact(start_x, start_y))
        if row_first:
            for _ in range(y - start_y): n = f32(n + ndy)
            for _ in range(x - start_x): n = f32(n + ndx)
        else:
            for _ in range(x - start_x): n = f32(n + ndx)
            for _ in range(y - start_y): n = f32(n + ndy)
        return float(n)
    out["cols then rows from (0,0)"] = lambda x, y: acc(0, 0, x, y)
    out["rows then cols from (0,0)"] = lambda x, y: acc(0, 0, x, y, True)
    # B: per column the window's bottom pixel row (o >= -N/2 with N = 4), n stepped along the line's columns at the window bottom
    def colwin(x, y):
        n_bot_prev = None
        ws_prev = None
        n = None
        for c in range(0, x + 1):
            ycen = (c + 0.5) * s
            ws = int(np.ceil(ycen - 2.0 - 0.5))   # first row whose centre y + 0.5 has o >= -2
            if n is None:
                n = f32(exact(c, ws))
            else:
                n = f32(n + ndx)
                for _ in range(ws - ws_prev): n = f32(n + ndy)
            ws_prev = ws
        m = n
        for _ in range(y - ws_prev): m = f32(m + ndy)
        return float(m)
    out["column walk, window bottom"] = colwin
    # C: plane evaluation in float at relative coordinates
    out["plane f32 relative"] = lambda x, y: float(f32(f32(f32(x + 0.5) * ndx) + f32(f32(y + 0.5) * ndy)))
    return out
for name, fn in schemes().items():
    row = []
    for (x, y), (lo, hi) in meas.items():
        e = exact(x, y)
        got = fn(x, y)
        r = abs(got) / abs(e) - 1
        row.append("%s %+.2e %s" % ((x, y), r, "ok" if lo <= r <= hi else "--"))
    print("%-30s %s" % (name, "  ".join(row)))
