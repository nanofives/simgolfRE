"""S10.10: resolve the exe forwarders of 10_facts_exe.json to the sound.dll body they reach (extends 8_tools/fwd.py).
Shape: `mov ecx, [ecx + O]` ... `mov eax, [ecx]` ... `jmp|call [eax + S]`. Device class per O and exe table:
 O = 0x14: NodeList / NodeListB / NodeListC -> Wave_Device / Midi_Device / Wave_In_Device vtables (S10.8 fwd.py);
 O = 0x40: the object create_sound (sound.dll 0x100093e0) builds for the exe's createSound4840e0 type argument:
   every exe caller passes type 1 (0x00484319, 0x00484c33, 0x00484c96, 0x00484d7d, 0x00484e88) -> case 0 -> MultiSound
   vtable 0x1005c7e8 (ctor 0x10039790, store 0x100397ca), except VoiceRx::start (push 6 at 0x00485460) -> case 5 ->
   vtable 0x1005c450 (ctor 0x10030080, store 0x100300b9). Exe tables 0x004badf8 / 0x004bbc3c are VoiceRx's.
~/bin/py -3.12 log/roadmap/S10/10_tools/fwd2.py  -> log/roadmap/S10/10_fwd.tsv"""
import glob, json, pathlib, re, struct
import pefile
R = pathlib.Path(__file__).resolve().parents[4]
snd = pefile.PE(str(R / "original/sound.dll"), fast_load=True)
NODE = {"NodeList": 0x1005b304, "NodeListB": 0x1005b28c, "NodeListC": 0x1005b3d4}
VOICERX_TABLES = {"0x004badf8", "0x004bbc3c"}
MULTI, VRX = 0x1005c7e8, 0x1005c450
names = {}
for f in glob.glob(str(R / "re/names/*.tsv")):
    if not f.endswith("_globals.tsv"):
        for l in open(f, encoding="utf-8").read().splitlines()[1:]:
            c = l.split("\t")
            if len(c) == 6 and c[0] == "sound.dll":
                names[int(c[1], 16)] = c[2]
sp = {}
for l in (R / "log/roadmap/S10/pdb/sound.procs.tsv").read_text(encoding="utf-8").splitlines():
    c = l.split("\t")
    if len(c) >= 3 and c[0].startswith("0x"):
        sp.setdefault(int(c[0], 16) + 0x10000000, set()).add(c[2])


def sdw(va): return struct.unpack("<I", snd.get_data(va - 0x10000000, 4))[0]


def body(va):
    b = snd.get_data(va - 0x10000000, 5)
    return va + 5 + struct.unpack("<i", b[1:])[0] if b[0] == 0xE9 else va


d = json.loads((R / "log/roadmap/S10/10_facts_exe.json").read_text(encoding="utf-8"))
out = ["addr\tours\ttheirs\toffset\tslot\tsite\tvtables\ttargets\tour_target_names\tsound_pdb_names"]
for a, v in d.items():
    ins = [b.split(" ", 2) for b in v["body"]]
    head = next((x for x in ins if len(x) == 3 and x[1] == "mov" and re.match(r"ecx, dword ptr \[ecx \+ 0x(14|40)\]$", x[2])), None)
    call = next((x for x in ins if x[1] in ("jmp", "call") and len(x) == 3 and re.match(r"dword ptr \[eax \+ 0x[0-9a-f]+\]$", x[2])), None)
    if not head or not call or ins.index(head) > 1:
        continue
    off = int(head[2].split("+ ")[1].rstrip("]"), 16)
    s = int(call[2].split("+ ")[1].rstrip("]"), 16)
    tabs = {re.search(r"table at (0x[0-9a-f]+)", r["detail"]).group(1) for r in v["refs"] if "table at" in r["detail"]}
    cls = v["ours"].split("::")[0]
    if off == 0x14:
        vts = [NODE[cls]] if cls in NODE else []
    else:
        vts = sorted({VRX if t in VOICERX_TABLES else MULTI for t in tabs})
    tg = [body(sdw(vt + s)) for vt in vts]
    out.append("\t".join([a, v["ours"], v["theirs"], hex(off), hex(s), "0x" + call[0], ",".join(map(hex, vts)),
                          ",".join(map(hex, tg)), ",".join(names.get(t, "-") for t in tg),
                          ",".join("|".join(sorted(sp.get(t, {"-"}))) for t in tg)]))
(R / "log/roadmap/S10/10_fwd.tsv").write_text("\n".join(out) + "\n", encoding="utf-8")
print(len(out) - 1, "forwarders resolved")
