"""Static checks of the .sve parser (re/tools/sve.py) against re/analysis/save/0040afa0_save_serializer.md."""
import pytest

import sve
from conftest import GAME_DIR, ROOT

pytestmark = pytest.mark.static

SAVES = sorted((GAME_DIR / "saved games").glob("*.sve")) + sorted((ROOT / "tests" / "fixtures").rglob("*.sve"))


def test_layout_sizes():
    # 86 unconditional FUN_0040af70 calls; body with only tail A = 296,873 bytes (3/3 saves on disk).
    assert len(sve.FIELDS) == 86
    assert sum(s for _, s in sve.layout(0x40000000)) == 296_873
    assert sum(s for _, s in sve.layout(0x40200000)) == 296_873 + 0xB7C0 + 0x19A28
    assert len({va for va, _ in sve.layout(0x40200000)}) == 89  # no global serialized twice


def test_decode_date():
    assert sve.decode_date(0xC600) == (16, "April", 2007)
    assert sve.decode_date(0xC800) == (1, "May", 2007)


@pytest.mark.parametrize("path", SAVES, ids=lambda p: p.name)
def test_roundtrip_and_header_date(path):
    blob = path.read_bytes()
    s = sve.parse(blob)
    assert s.to_bytes() == blob
    d, m, y = sve.decode_date(s.field(0x00834170).u32())
    assert s.title.endswith(f", {d} {m} {y}")


STORIES5 = ROOT / "tests" / "fixtures" / "stories" / "stories5.sve"


@pytest.mark.skipif(not STORIES5.exists(), reason="stories5.sve fixture not present (game-derived, not committed)")
def test_stories5_summary_matches_load_panel():
    # Load Previous Game panel for stories5.sve (log/ttd/stories_load/last_frame.png, 2026-10-03):
    # Holes 5, Par 15, Yards 430, Cash 1,000,000, Fun Rating 99, Length -0.15, Accuracy 0.99, Imagination 0.20,
    # Theme Standard, Course Record 10.
    s = sve.summary(sve.load(STORIES5))
    assert s == ("Holes=5 Par=15 Yards=430 Type=Park Theme=Standard Record=10 Cash=1000000 FunRating=99 "
                 "SkillRating=1.04 LengthSkill=-0.15 AccuracySkill=0.99 Imagination=0.20")
