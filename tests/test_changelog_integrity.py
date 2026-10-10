"""hooks.csv and re/analysis/CHANGELOG.md must agree: every C3/C4 row has a C2->C3 line.

re_classify.py writes both, but twice a promotion wrote hooks.csv and no CHANGELOG line (registerExit_44b190 in round
22, MappedFile::dtor in round 24: a blank line where c3_verify --apply prints PROMOTED). Both were re-recorded with a
demote + promote; this test catches the next one.
"""
import csv
import re

import pytest

from conftest import ROOT

pytestmark = pytest.mark.static

LINE = re.compile(r'\S+\s+(?:([^\s:]+):)?([0-9a-fA-F]{8})\s+.*?\s(C2->C3)(?:\s|$)')


def test_every_c3_row_has_a_promotion_line():
    have, addr_only = set(), set()
    for line in (ROOT / 're' / 'analysis' / 'CHANGELOG.md').read_text(encoding='utf-8').splitlines():
        m = LINE.match(line)
        if m:
            addr = int(m.group(2), 16)
            have.add(((m.group(1) or 'golf_clean.exe').lower(), addr))
            if not m.group(1):
                addr_only.add(addr)        # lines written before the module prefix existed (2026-10-02)
    missing = []
    for r in csv.DictReader(open(ROOT / 'hooks.csv')):
        if r['confidence'] not in ('C3', 'C4'):
            continue
        mod, addr = (r['module'] or 'golf_clean.exe').lower(), int(r['addr'], 16)
        if (mod, addr) not in have and not (mod != 'golf_clean.exe' and addr in addr_only):
            missing.append(f'{mod}:{addr:08x} {r["name"]}')
    assert not missing, f'C3/C4 rows without a C2->C3 CHANGELOG line: {missing}'
