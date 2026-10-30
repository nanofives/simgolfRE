"""Static checks of the S9.1 network survey (re/tools/net_survey.py, re/port/net_*.tsv)."""
import shutil
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 're' / 'tools'))
import net_survey as ns  # noqa: E402

pytestmark = pytest.mark.static


@pytest.fixture(scope='module')
def ctx():
    return ns.Image(), ns.load_functions()


def test_tsvs_agree_with_the_binary(ctx):
    assert ns.run_check(None, *ctx) == []


@pytest.mark.parametrize('kind, key, col, value', [
    ('state', '008400b8', 'writers', 'netService'),            # a store to g_netSession that does not exist
    ('functions', '00497d10', 'dp_calls', 'Close x1'),          # a dropped Release
    ('messages', 'A80', 'type', '0x40'),                        # wrong message type at the drop notice
    ('messages', 'S102', 'parse_sites', '0049b918;00497b00'),   # a parse site outside netPoll
])
def test_control_changed_row_reads_red(ctx, tmp_path, kind, key, col, value):
    paths = {}
    for k, p in ns.TSV.items():
        paths[k] = tmp_path / p.name
        shutil.copy(p, paths[k])
    lines = paths[kind].read_text(encoding='utf-8').splitlines()
    head = lines[0].split('\t')
    for i, line in enumerate(lines[1:], 1):
        r = line.split('\t')
        if r[0] == key:
            r[head.index(col)] = value
            lines[i] = '\t'.join(r)
    paths[kind].write_text('\n'.join(lines) + '\n', encoding='utf-8')
    assert ns.run_check(paths, *ctx), 'the changed %s row %s must read RED' % (kind, key)


def test_control_removed_message_row_reads_red(ctx, tmp_path):
    paths = {k: tmp_path / p.name for k, p in ns.TSV.items()}
    for k, p in ns.TSV.items():
        shutil.copy(p, paths[k])
    lines = [l for l in paths['messages'].read_text(encoding='utf-8').splitlines() if not l.startswith('A20\t')]
    paths['messages'].write_text('\n'.join(lines) + '\n', encoding='utf-8')
    assert any('0049ab93' in d for d in ns.run_check(paths, *ctx))


def test_no_directplay_import_and_no_reference(ctx):
    im, funcs = ctx
    f = ns.facts(im, funcs)
    assert f['imports'] == ['binkw32.dll', 'Terrain.dll', 'KERNEL32.dll', 'USER32.dll', 'WINMM.dll']
    assert f['dplayx_string'] == 0x004bf82e and f['dplayx_string_refs'] == 0
    assert set(ns.DPLAY_GUIDS) <= set(f['guids'])
    assert all(refs == 0 for va, refs in f['guids'].values())
    assert [s for _, _, _, s in f['loadlibrary']] == ['.\\sound.dll', None, 'user32.dll']
    assert f['thread_or_event_imports'] == []


def test_session_pointer_is_never_set(ctx):
    im, funcs = ctx
    f = ns.facts(im, funcs)
    assert ns.G_NET_SESSION >= f['data_raw_end'] and ns.G_NET_INTERFACE >= f['data_raw_end']
    refs = ns.global_refs(im, funcs, ns.net_function_set(im, funcs))
    assert [x for x in refs[ns.G_NET_SESSION] if x[2] == 'w'] == []
    stores = [x for x in refs[ns.G_NET_INTERFACE] if x[2] == 'w']
    assert [(x[0], x[3]) for x in stores] == [(0x00497d78, 'ebx')]
    zero = next(im.disasm(0x00497d20, 0x00497d22))
    assert (zero.mnemonic, zero.op_str) == ('xor', 'ebx, ebx')
    # every way into the layer tests g_netSession first
    for gate in (0x0047ee76, 0x00495d20, 0x00497b00, 0x00497b20):
        ins = next(im.disasm(gate, gate + 6))
        assert ins.op_str.endswith('[0x8400b8]'), hex(gate)


def test_interface_slots_follow_dplay_h(ctx):
    im, funcs = ctx
    m = ns.interface_methods()
    assert (m[2], m[4], m[10], m[21], m[26]) == ('Release', 'Close', 'EnumGroupPlayers', 'GetPlayerName', 'Send')
    calls = ns.interface_calls(im, funcs)
    assert sorted({c[3] for c in calls}) == ['Close', 'EnumGroupPlayers', 'GetPlayerName', 'Release', 'Send']
    assert len(calls) == 13
    enum = [c for c in calls if c[3] == 'EnumGroupPlayers'][0]
    assert enum[0] == 0x0049844b and '0x497f60' in enum[4]     # netMarkRecordCb is the callback
