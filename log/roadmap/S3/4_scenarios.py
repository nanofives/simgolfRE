"""Study scenarios of roadmap S3.4 (FLIC sheet cells), registered through SCENARIO_REFS_EXTRA by re/tools/flic_cells_rec.py.

cov_b_s34_hawaii: title menu -> Sandbox Mode -> Hawaii (a tropical location, course type 2) -> in game. The stock game dies
in loadWorldAssets' first `Tree Cerc` load on this install (U-1701); run it with --tree-alias for the tropical cells, and
without it for the failed open.
"""


def register(S):
    import scenario as SCN

    def cov_b_s34_hawaii(c):
        yield from SCN._cov_b_menu(c)
        yield from SCN._cov_b_sandbox(c, "hawaii")
        yield from c.checkpoint("hawaii")

    S.SCENARIOS["cov_b_s34_hawaii"] = cov_b_s34_hawaii
