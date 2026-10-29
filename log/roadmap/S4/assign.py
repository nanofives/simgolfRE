"""S4 lead: assign every sound.dll hooks.csv row to an S4 step (re/roadmap/S4_functions.tsv).

Family by name prefix; rows the prefixes do not place take the family of the nearest placed row by RVA (sound.dll's
object files are contiguous in its link order), with explicit overrides below. CRT rows (subsystem crt) are dropped
from the port (re/PORT_PLAN.md: the CRT is not ported) and listed with step '-'. C2 rows are listed with their status
so the step worker decides: `dead` rows are not ported, held / trivial rows are ported only when the engine reaches
them (no path-1 rows exist for them: the emulated original is their only oracle)."""
import bisect
import csv
import collections
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[3]
STEP = {"list": "S4.2", "codec": "S4.3", "mixer": "S4.4", "seq": "S4.5", "chan": "S4.6", "sound": "S4.7",
        "ds": "S4.8", "dev": "S4.8", "wave": "S4.9"}
OVERRIDE = {
    # thread, window, timer and export plumbing: devices
    "init_sound_timer": "dev", "release_sound": "dev", "create_sound": "sound", "registerTimerClient": "dev",
    "postNotification": "dev", "postMidiNotification": "dev", "destroySoundWindowObj": "dev", "pumpWaveMessages": "dev",
    "soundWindowProc": "dev", "soundWindowSubclassProc": "dev", "stopSoundThreadFull": "dev", "startSignalThread": "dev",
    "soundThreadPump": "dev", "postSoundThreadMessage": "dev", "atexitDtor_100b49b8": "dev", "initCtrlVtable": "dev",
    "ctrlScalarDeletingDtor": "dev", "Engine_playSound": "sound", "getPlaybackTime": "sound",
    # DirectSound wrappers
    "mapDsError": "ds", "getDeviceCaps": "ds", "unlinkDsBuffer": "ds", "DsDevice_registerBuffer": "ds",
    "getDsCoopLevel": "ds",
    # wave files, streams, decoders
    "processWaveChunks": "wave", "startStream": "wave", "computeCaptureDuration": "wave", "initStreamBuffer": "wave",
    "writeWaveFile": "wave", "openWaveStreamFile": "wave", "getWaveDeviceInterface": "wave",
    "serviceWaveDevice": "wave", "streamReadByte": "wave", "streamConsumeByte": "wave", "streamWaveSamples": "wave",
    "streamReaderDtor": "wave", "loadWaveResource": "wave", "beginDecodeStreamA": "wave", "beginDecodeStreamB": "wave",
    "closeDecoder": "wave", "fillDecodeRing": "wave", "serviceDecodeRing": "wave", "onWaveInCallbackMessage": "wave",
    "loadMultiChannel": "wave", "enqueueWaveCmdE": "wave", "enqueueWaveCmd10": "wave",
    "writeWaveChunk": "wave", "writeWaveChunkB": "wave", "loadSoundFile": "wave",
    # small generic helpers: core
    "freeDynArray": "list", "resizeDynArray": "list", "pairStore": "list", "pairClear": "list", "getSpanLength": "list",
    "StrPtr_assign": "list", "ChainObject_dtor": "list", "ChainObject_scalarDtor": "list", "byteSwap16": "list",
    "randRange": "list", "nextRandomFloat": "list", "Object_setName": "list", "getField204": "list",
    "MemBlock_deleteDtor": "list", "setField6cReturn0": "list", "clearField6cReturn0": "list",
    # voices / channels
    "initSoundVoice": "chan", "allocVoiceSlot": "chan", "updateVoiceRate": "chan", "rewindChannel": "chan",
    "serviceChannelPlayback": "chan", "startDevicePlayback": "chan", "openSourceType4": "chan",
    "openSourceType6": "chan", "openSourceType7": "chan", "buildVoiceRxName": "chan", "buildVoiceTxName": "chan",
}


def family(a, n):
    if n in OVERRIDE:
        return OVERRIDE[n]
    if 0x3A900 <= a < 0x41A00:
        return "codec"
    low = n.lower()
    if "mix" in low or "render" in low:
        return "mixer"
    if n.startswith(("Seq", "Midi", "Track")) and not n.startswith("MidiReader"):
        return "seq"
    if n.startswith(("DsBuffer", "DirectSound", "Ds3D", "dsEnum")):
        return "ds"
    if n.startswith(("Wave", "Wave_In", "Dll_Wave", "StreamReader", "MidiReader")) or "mmio" in low or "riff" in low:
        return "wave"
    if n.startswith(("Sound", "MultiSound", "SoundGroup", "registerSound", "unregisterSound")):
        return "sound"
    if n.startswith(("Channel", "Voice", "Ch5", "SubChannel", "ChanRecord", "ChannelTable")):
        return "chan"
    if n.startswith(("Device", "Ctrl", "Dll", "CommandQueue", "SoundTimer", "WaveCmd", "SoundWindow", "dynInit")):
        return "dev"
    if any(k in n for k in ("List", "Node", "list", "node")):
        return "list"
    return None


def main():
    rows = [r for r in csv.DictReader((ROOT / "hooks.csv").open(newline="")) if r["module"] == "sound.dll"]
    keys = collections.Counter()
    for p in (ROOT / "log" / "diff").glob("*_sound_*.path1.csv"):
        keys[int(p.name[:8], 16)] += 1
    placed = sorted((int(r["addr"], 16), family(int(r["addr"], 16), r["name"])) for r in rows
                    if family(int(r["addr"], 16), r["name"]) and r["subsystem"] != "crt")
    addrs = [a for a, _ in placed]
    out = []
    for r in rows:
        a = int(r["addr"], 16)
        if r["subsystem"] == "crt":
            fam, step = "crt", "-"
        else:
            fam = family(a, r["name"])
            if fam is None:
                i = bisect.bisect_left(addrs, a)
                cand = [placed[j] for j in (i - 1, i) if 0 <= j < len(placed)]
                fam = min(cand, key=lambda c: abs(c[0] - a))[1] + "~"
            step = STEP[fam.rstrip("~")]
        if r["confidence"] in ("C3", "C4"):
            oracle = "path1+emu"
        elif r["status"] == "dead":
            oracle, step = "dead", "-"
        else:
            oracle = "emu-only"
        out.append((step, fam, f"0x{a:05x}", r["name"], r["confidence"], r["status"], str(keys[a]), oracle,
                    r["file"].split("/")[-1]))
    out.sort(key=lambda t: (t[0], int(t[2], 16)))
    dst = ROOT / "re" / "roadmap" / "S4_functions.tsv"
    with dst.open("w", newline="") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["step", "family", "rva", "name", "level", "status", "path1_csvs", "oracle", "hook_source"])
        w.writerows(out)
    c = collections.Counter((t[0], t[7]) for t in out)
    for k in sorted(c):
        print(k, c[k])


if __name__ == "__main__":
    main()
