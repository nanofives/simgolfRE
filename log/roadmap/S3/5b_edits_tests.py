"""S3.5b: S2's test expectations that S3.5's hand-over makes obsolete (exact (path, old, new) pairs, applied to the mirror by
`~/bin/py -3.12 log/roadmap/S3/5b_edits_tests.py` and written out in 5b_merge.md section 6). Each asserted an `unsupported` report for
a draw the original makes and the scene family now draws (judged by test_gfx_scene's emulator oracle on the original's jgld bodies).
"""
import pathlib

EDITS_TESTS = [
    ("port/test/test_gfx_routing.py",
     "def test_image_draw_at_another_scale_is_unsupported(glib):\n"
     "    img, log = _scaled_draw(glib, (1, 1, 5))\n"
     "    assert \"Image_draw: a display scale other than (1, 1, 1)\" in log and not img.any()\n",
     "def test_image_draw_at_another_scale_is_drawn_by_the_scene_family(glib):\n"
     "    # S3.5: a scale other than (1, 1, 1) is drawB under that scale (gfxs::image_draw), no longer reported; at (1, 1, 5) the\n"
     "    # 4 x 2 image scales to (int)(4 * 0.2) = 0 columns, so the magnifying (2, 2, 1) is the one that leaves pixels\n"
     "    img, log = _scaled_draw(glib, (2, 2, 1))\n"
     "    assert \"Image_draw: a display scale other than (1, 1, 1)\" not in log and img.any()\n"),
    ("port/test/test_gfx_dialogs.py",
     "    assert \"Surface_drawSprite: an alpha outside drawAlpha's quarter band\" in log\n",
     "    # S3.5: another alpha is drawn by gfxs::surface_draw_sprite (drawAlpha's window bodies), no longer reported\n"
     "    assert \"Surface_drawSprite: an alpha outside drawAlpha's quarter band\" not in log\n"),
    ("port/test/test_gfx_reports.py",
     "        assert \"an alpha outside drawAlpha's quarter band\" in _log(st)\n"
     "        assert not _root(st).any()\n",
     "        # S3.5: drawn by gfxs::surface_draw_sprite (drawAlpha's window bodies), no longer reported\n"
     "        assert \"an alpha outside drawAlpha's quarter band\" not in _log(st)\n"),
    ("port/test/test_gfx_reports.py",
     "    assert \"no body: addSprite\" in judged[\"$log\"]\n",
     "    assert \"no body: addSprite\" not in judged[\"$log\"]   # S3.5 registers the addSprite body\n"),
    ("port/test/test_gfx_reports.py",
     "    for c in FRAME_CONTROLS[mode]:\n"
     "        assert r[c][\"diff\"] != judged[c][\"diff\"] or r[c][\"judged\"] != judged[c][\"judged\"], (mode, c, r[c])\n",
     "    same = [c for c in FRAME_CONTROLS[mode] if not (r[c][\"diff\"] != judged[c][\"diff\"] or r[c][\"judged\"] != judged[c][\"judged\"])]\n"
     "    # mode 8 (the clip left narrowed): S3.5's sprite draws set their own clip on some frames, so the control keeps its\n"
     "    # effect on at least one of its checkpoints instead of all of them\n"
     "    assert len(same) < len(FRAME_CONTROLS[mode]) if mode == 8 else not same, (mode, same)\n"),
]

if __name__ == "__main__":
    root = pathlib.Path(__file__).resolve().parents[3] / "port" / "build" / "S3_5b_src"
    for rel, old, new in EDITS_TESTS:
        p = root / rel
        b = p.read_bytes().decode()
        if new.replace("\n", "\r\n") in b or new in b:
            continue
        if "\r\n" in b:
            old, new = old.replace("\n", "\r\n"), new.replace("\n", "\r\n")
        assert b.count(old) == 1, rel
        p.write_bytes(b.replace(old, new).encode())
    print("applied")
