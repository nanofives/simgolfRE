# S3.9: the wave 7 interaction no step's gate ran (9_notes.md section 4). S3.16's checkpoint-position fix (16_snapshot_edits.py:
# a checkpoint whose POLL rides on a non-tgt clock call is snapshotted at the POLL's position, gfx.checkpoint_ordinal / framegate,
# U-1820) moved framegate's frame for 7 rows, but port/test/test_app_ingame.py's checkpoint_plan keeps its own copy of the old
# rule (status.tgt only), so the app's live-trace snapshot of cov_k1_routing `end` (one of the 7) stayed at the old position and
# test_app_path_equals_framegate_and_its_present failed with 1,955 px. The edit gives checkpoint_plan framegate's rule, with the
# same SG_GFX_CHECKPOINT_AT=tgt control. Format: (path, old text exactly once, new text), as the steps' edit lists.
EDITS = [
    ("port/test/test_app_ingame.py",
     """    summ = json.loads((GX.REFS / scenario / "det_b" / "summary.json").read_text())
    rows = []
    for c in FG.scenario_checkpoints(scenario):
        st = summ["checkpoints"][c]["status"]
        if st["tgt"] in tgt and tgt[st["tgt"]][1] == st["T"]:
            o = tgt[st["tgt"]][0]
            rows.append(dict(FG.classify(tr, o), checkpoint=c, ordinal=o))
""",
     """    summ = json.loads((GX.REFS / scenario / "det_b" / "summary.json").read_text())
    # roadmap S3.9: framegate's checkpoint position (roadmap S3.16, U-1820): a checkpoint whose POLL rode on a clock call that is
    # not a tgt call is taken at the POLL's position (SG_GFX_CHECKPOINT_AT=tgt restores status.tgt's call, as in framegate)
    polls = {}
    for r in tr.records:
        if r.type == TF.POLL and r.f.get("checkpoint") and r.f["checkpoint"] not in polls:
            polls[r.f["checkpoint"]] = r.f["pos"]
    rows = []
    for c in FG.scenario_checkpoints(scenario):
        st = summ["checkpoints"][c]["status"]
        if st["tgt"] in tgt and tgt[st["tgt"]][1] == st["T"]:
            o = tgt[st["tgt"]][0]
            if os.environ.get("SG_GFX_CHECKPOINT_AT", "poll") == "poll" and polls.get(c) is not None and polls[c] < o:
                o = polls[c]
            rows.append(dict(FG.classify(tr, o), checkpoint=c, ordinal=o))
"""),
    # S3.17 adds the body Surface_createFull_fwd1 to the `surface` family (gfx_surface.cpp kBodies: 9 -> 10); S2.3's
    # test_gfx_credits pins the family's count and S3.17 did not run that file (port suite on the combined tree: 1 failed)
    ("port/test/test_gfx_credits.py",
     """        assert st.call("gfx.family.surface", []).eax == 9
""",
     """        assert st.call("gfx.family.surface", []).eax == 10   # 9 + Surface_createFull_fwd1 (roadmap S3.17)
"""),
]
