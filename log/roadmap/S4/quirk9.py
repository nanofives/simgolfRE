"""The emulator's signaling-NaN quirk (S4.6's U-1451, as in 5_run.py): Unicorn's fld / fst of a signaling NaN keeps it signaling; the
CPU (and the live C3 A/B: log/c3/c3fe_notes.txt) sets the quiet bit. The port follows the CPU. For the keys whose original copies
doubles through the x87 (QUIRK_KEYS), a difference that is exactly the quiet bit of a NaN pattern (port set, emulator clear) is
forgiven; every other difference still fails. The golden digest of a forgiven vector pins the port's regions.

    oracle_key_q(R, vm, lib, key, layouts, record, quirk_keys) -> (KeyVerdict, layout A records, regions, {vector: NaNs forgiven})
"""


def _quirk_bytes_ok(bo: bytes, bp: bytes):
    """True when every differing byte is the quiet bit of a NaN (double: bit 51 of a qword, float: bit 22 of a dword) that the
    port set and the emulator left clear; returns (ok, number of forgiven NaNs)"""
    if len(bo) != len(bp):
        return False, 0
    diffs = [k for k in range(len(bo)) if bo[k] != bp[k]]
    done, n = set(), 0
    for k in diffs:
        if k in done:
            continue
        ok = False
        if k + 1 < len(bo) and k >= 6:
            o, q = bo[k - 6:k + 2], bp[k - 6:k + 2]
            if ((o[7] & 0x7F) == 0x7F and (o[6] & 0xF0) == 0xF0 and not (o[6] & 0x08) and q[6] == (o[6] | 0x08)
                    and o[:6] == q[:6] and o[7] == q[7] and (o[6] & 0x07 or any(o[:6]))):
                ok = True
                done.add(k)
        if not ok and k + 1 < len(bo) and k >= 2:
            o, q = bo[k - 2:k + 2], bp[k - 2:k + 2]
            if ((o[3] & 0x7F) == 0x7F and (o[2] & 0x80) and not (o[2] & 0x40) and q[2] == (o[2] | 0x40)
                    and o[:2] == q[:2] and o[3] == q[3] and (o[2] & 0x3F or any(o[:2]))):
                ok = True
                done.add(k)
        if not ok:
            return False, 0
        n += 1
    return True, n


def quirk_equal(orig_hex, port_hex):
    if orig_hex is None or port_hex is None:
        return False, 0
    ro, rp = orig_hex.split("|"), port_hex.split("|")
    if len(ro) != len(rp):
        return False, 0
    total = 0
    for a, b in zip(ro, rp):
        if a == b:
            continue
        ok, n = _quirk_bytes_ok(bytes.fromhex(a), bytes.fromhex(b))
        if not ok:
            return False, 0
        total += n
    return True, total


def oracle_key_q(R, vm, lib, key, layouts=("A", "B", "C"), record=True, quirk_keys=()):
    """R.oracle_key with the quirk filter: (KeyVerdict, layout A records, regions, forgiven {vector: NaNs})"""
    E = R.E
    hooks, batch = E.registry()
    spec = R.spec_of(key)
    derived = key in R.DERIVED
    if derived:
        hooks[key] = spec
    quirk = R.port_name(key) in quirk_keys
    res, ports = {}, {}
    for lay in layouts:
        lp = R.LivePort(lib, key, record and lay == "A")
        try:
            res[lay] = E.KeyRun(vm, key, lay, second_arm=lp).run()
        except E.EmuError as e:
            res[lay] = e
        finally:
            lp.close()
        ports[lay] = lp
    a = res["A"]
    first, match, match_bc, errs = "", 0, 0, 0
    golden, pdig, forgiven = [], [], {}

    def row_ok(row):
        if row.match:
            return True, 0
        if quirk and not row.error and row.orig == row.re:
            ok, n = quirk_equal(row.state_hex, row.state_re_hex)
            if ok:
                return True, n
        return False, 0

    if isinstance(a, Exception) or a.fixture_error:
        return R.KeyVerdict(key, R.port_name(key), len(spec["vectors"]), 0, 0, len(spec["vectors"]), 0, "SETUP_ERR",
                            str(a if isinstance(a, Exception) else a.fixture_error)[:200], [], []), None, [], {}
    for i, row in enumerate(a.rows):
        ok, n = row_ok(row)
        if n:
            forgiven[i] = n
        golden.append(R.digest(row.orig, row.state_re_hex if n else row.state_hex))
        pdig.append(R.digest(row.re, row.state_re_hex))
        if row.error:
            errs += 1
            if not first:
                first = f"v{i} A: {row.error[:160]}"
        elif ok:
            match += 1
        elif not first:
            first = f"v{i} A: ret emu {row.orig} port {row.re}" + ("" if row.state_hex == row.state_re_hex else " (regions differ)")
    for lay in layouts[1:]:
        rb = res[lay]
        if isinstance(rb, Exception):
            first = first or f"{lay}: {rb}"
            continue
        for i, row in enumerate(rb.rows):
            if row_ok(row)[0]:
                match_bc += 1
            elif not first:
                first = f"v{i} {lay}: " + (row.error[:160] if row.error else f"ret emu {row.orig} port {row.re}")
    fb = sum(int(r.report.get("fallback", 0) or 0) for lp in ports.values() for r in lp.records)
    if derived:
        tier = "EMU_ONLY"
    elif not {"B", "C"} <= set(res):
        tier = "A_ONLY"
    else:
        import sound_census as SC
        tier = SC.compare(key, spec, batch.get(key), res, E.read_csv(spec, key))["row"]["tier"]
    v = R.KeyVerdict(key, R.port_name(key), len(a.rows), match, match_bc, errs, fb, tier, first, golden, pdig)
    return v, (ports["A"].records if record else None), [list(x) for x in a.regions], forgiven
