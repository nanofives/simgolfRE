"""Static checks: binary anchors, SafeDisc removal, portability, shim surface. No process launched."""
import hashlib
import math
import pathlib
from collections import Counter

import pefile
import pytest

from conftest import GAME_DIR, ROOT

pytestmark = pytest.mark.static

GAME_MODULES = ["golf_clean.exe", "jgld.dll", "jgl.dll", "Terrain.dll", "sound.dll", "binkw32.dll"]


def _pe(name):
    return pefile.PE(str(GAME_DIR / name))


def _imports(pe):
    out = {}
    for e in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
        out.setdefault(e.dll.decode().lower(), set()).update(i.name.decode() for i in e.imports if i.name)
    return out


def _entropy(data: bytes) -> float:
    if not data:
        return 0.0
    n = len(data)
    return -sum(c / n * math.log2(c / n) for c in Counter(data).values())


# ---------------------------------------------------------------- anchors
@pytest.mark.parametrize("name", ["golf.exe", "golf_clean.exe", "jgld.dll", "jgl.dll", "Terrain.dll", "sound.dll", "binkw32.dll"])
def test_binary_anchor(anchors, name):
    """Every RVA in this project is only valid against these exact bytes."""
    want = anchors["files"][name]
    data = (GAME_DIR / name).read_bytes()
    assert len(data) == want["size"], f"{name} size changed"
    assert hashlib.sha256(data).hexdigest().upper() == want["sha256"], (
        f"{name} does not match its anchor. If it was patched in place, restore it: patches belong in the shim.")


# ---------------------------------------------------------------- SafeDisc removal
def test_protected_exe_is_safedisc_2_60_52():
    data = (GAME_DIR / "golf.exe").read_bytes()
    i = data.find(b"BoG_ *90.0&!!  Yy>")
    assert i >= 0, "SafeDisc signature missing from the protected anchor"
    import struct
    assert struct.unpack_from("<III", data, i + 32) == (2, 60, 52)


def test_clean_exe_has_no_safedisc_sections():
    pe = _pe("golf_clean.exe")
    names = [s.Name.rstrip(b"\0").decode() for s in pe.sections]
    assert names == [".text", ".rdata", ".data", ".rsrc"], names


def test_clean_exe_text_is_decrypted():
    pe = _pe("golf_clean.exe")
    text = next(s for s in pe.sections if s.Name.startswith(b".text"))
    assert _entropy(text.get_data()) < 7.0, "encrypted .text has entropy ~7.98"


def test_clean_exe_entry_point_is_crt_startup():
    pe = _pe("golf_clean.exe")
    assert pe.OPTIONAL_HEADER.ImageBase == 0x400000
    assert pe.OPTIONAL_HEADER.AddressOfEntryPoint == 0xA682F  # VA 0x004a682f


def test_clean_exe_imports_rebuilt():
    imps = _imports(_pe("golf_clean.exe"))
    assert set(imps) == {"binkw32.dll", "terrain.dll", "kernel32.dll", "user32.dll", "winmm.dll"}
    assert sum(len(v) for v in imps.values()) == 149


# ---------------------------------------------------------------- portability
@pytest.mark.parametrize("name", GAME_MODULES)
def test_no_registry_dependency(name):
    """The installer writes registry keys; nothing the game loads reads them."""
    assert "advapi32.dll" not in _imports(_pe(name))


def test_fonts_are_loaded_privately_by_the_game():
    """Jackal calls AddFontResourceA at runtime, so the installer's font registration is not needed."""
    imps = _imports(_pe("jgld.dll"))
    assert "AddFontResourceA" in imps["gdi32.dll"]
    for f in ("KLEPTO__.TTF", "manu3_.TTF", "klepto__.fot", "manu3_.fot"):
        assert (GAME_DIR / f).exists()


def test_cd_root_files_present():
    for f in ("jackal.txt", "AutoSaveBase.pcx", "AutoSaveButtons.pcx"):
        assert (GAME_DIR / f).exists(), f"{f} comes from the CD root and is read at boot"


def test_launcher_present():
    bat = (ROOT / "Play SimGolf.bat").read_text()
    assert "golf_clean.exe" in bat


# ---------------------------------------------------------------- shim
def test_shim_deployed_and_current():
    built = ROOT / "shim" / "build" / "winmm.dll"
    deployed = GAME_DIR / "winmm.dll"
    assert deployed.exists(), "run shim\\build.bat"
    assert built.read_bytes() == deployed.read_bytes(), "original\\winmm.dll is stale; run shim\\build.bat"


def test_shim_forwards_every_winmm_import_the_game_uses():
    shim = {e.name.decode() for e in _pe("winmm.dll").DIRECTORY_ENTRY_EXPORT.symbols if e.name}
    needed = set()
    for m in GAME_MODULES:
        needed |= _imports(_pe(m)).get("winmm.dll", set())
    assert needed, "expected the game to import winmm"
    assert needed <= shim, f"missing forwards: {sorted(needed - shim)}"


def test_shim_test_exports():
    shim = {e.name.decode() for e in _pe("winmm.dll").DIRECTORY_ENTRY_EXPORT.symbols if e.name}
    assert {"SimGolfShim_SetVirtualCursor", "SimGolfShim_ClearVirtualCursor", "SimGolfShim_GetInfo"} <= shim


def test_shim_ini_defaults():
    ini = (GAME_DIR / "simgolf_shim.ini").read_text()
    for line in ("windowed=1", "bink_dib=1", "skip_intro=0"):
        assert line in ini
