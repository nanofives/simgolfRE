"""S7.12: the decision row D54 (sound.golfSfxAlias, Mariano's G15 of 2026-10-30) in the S6 fragment format.

Writes log/roadmap/S7/12_decisions.tsv (the header of log/roadmap/S6/s3_4b_decisions.tsv, one row) and, with --apply, puts the
same row with verified / decision / profile_fixed into re/port/defect_decisions.tsv after D53 (as log/roadmap/S6/merge_decisions.py
would once 12_merge.md's patch adds this fragment to its inputs). Idempotent.

    ~/bin/py -3.12 log/roadmap/S7/12_decisions.py [--apply]
"""
import csv
import io
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
FRAG = ROOT / "log" / "roadmap" / "S7" / "12_decisions.tsv"
TABLE = ROOT / "re" / "port" / "defect_decisions.tsv"
BS = chr(92)
ROW = {
    "id": "D54", "source": "roadmap S7.10 / S7.11 / S7.12 (U-1499, G15)",
    "function": "loadSoundTable (the 18 `sounds" + BS + "Golf sfx" + BS + "` strings 0x004d1da0 .. 0x004d2020 and 0x004d104c), "
                "sound.dll mmioOpenA through the course-effect slots",
    "summary": "the exe names the course effects sounds" + BS + "Golf sfx" + BS + "<file> (17 strings, one more as golf sfx); the "
               "CD's InstallShield header iso" + BS + "data1.hdr names the directory Golf Sfx, the extracted install (unshield, "
               "spaces in directory names written as underscores) holds sounds" + BS + "Golf_Sfx (26 files): there every "
               "mmioOpenA of those names fails and the 18 effects never sound",
    "class": "wrong-value", "changes_save": "no", "changes_balance": "no", "changes_sim": "no", "changes_frame": "no",
    "hang": "no", "fault": "no",
    "effect_note": "audio only: the sim never opens a sound file (every segment start image holds g_soundReady 0); on a device "
                   "the switch makes 229 mmioOpenA find their file instead of 211 (audio_stream device run, 18 more names)",
    "recommendation": "fix", "switch": "sound.golfSfxAlias", "switch_state": "implemented",
    "evidence": "device run at switch 1 = golden $device 4b6ed61a (13,020 lines); at 0 the pinned digest of "
                "port/test/test_defects_golfsfx.py, every line equal to S7.11's renamed-install run "
                "(log/roadmap/S7/11_sfx_install.py) but the alias notes; segments / boot unchanged at 1 and 0 (S7.12 heavy gate)",
    "uncertain": "U-1499",
}
EXTRA = {"verified": "device-sink run (S7.12)", "decision": "G15 fix (S7.12)", "profile_fixed": "yes"}


def main() -> int:
    hdr = (ROOT / "log" / "roadmap" / "S6" / "s3_4b_decisions.tsv").read_text(encoding="utf-8").splitlines()[0].split("\t")
    buf = io.StringIO()
    w = csv.writer(buf, delimiter="\t", lineterminator="\n")
    w.writerow(hdr)
    w.writerow([ROW[c] for c in hdr])
    FRAG.write_text(buf.getvalue(), encoding="utf-8", newline="\n")
    if "--apply" in sys.argv:
        lines = TABLE.read_text(encoding="utf-8").splitlines()
        cols = lines[1].split("\t")
        row = dict(ROW, **EXTRA)
        out = io.StringIO()
        csv.writer(out, delimiter="\t", lineterminator="").writerow([row.get(c, "") for c in cols])
        lines = [ln for ln in lines if not ln.startswith("D54\t")]
        at = next(i for i, ln in enumerate(lines) if ln.startswith("D53\t")) + 1
        lines.insert(at, out.getvalue())
        TABLE.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print("wrote", FRAG, "and" if "--apply" in sys.argv else "", TABLE if "--apply" in sys.argv else "")
    return 0


if __name__ == "__main__":
    sys.exit(main())
