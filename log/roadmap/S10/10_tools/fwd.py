"""S10.8: resolve the exe's NodeList / NodeListB / NodeListC::fwdChildN forwarders to the sound.dll device method they reach.
child at this+0x14 = the device the create_device export wrote (openHookA/B/C); vtables from the device constructors."""
import csv, glob, pathlib, struct, sys
import pefile, capstone
R = pathlib.Path(__file__).resolve().parents[4]
VT = {"NodeList": ("Wave_Device", 0x1005b304), "NodeListB": ("Midi_Device", 0x1005b28c),
      "NodeListC": ("Wave_In_Device", 0x1005b3d4)}
exe = pefile.PE(str(R / "original/golf_clean.exe"), fast_load=True)
snd = pefile.PE(str(R / "original/sound.dll"), fast_load=True)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
names = {}
for f in glob.glob(str(R / "re/names/*.tsv")):
    if f.endswith("_globals.tsv"):
        continue
    for l in open(f, encoding="utf-8").read().splitlines()[1:]:
        c = l.split("\t")
        if len(c) == 6:
            names[(c[0], int(c[1], 16))] = c[2]
def sdw(va): return struct.unpack("<I", snd.get_data(va - 0x10000000, 4))[0]
def body(va):
    b = snd.get_data(va - 0x10000000, 5)
    return va + 5 + struct.unpack("<i", b[1:])[0] if b[0] == 0xE9 else va
out = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
for (mod, va), n in sorted(names.items()):
    cls = n.split("::")[0]
    if mod != "golf_clean.exe" or cls not in VT or "fwdChild" not in n:
        continue
    ins = list(md.disasm(exe.get_data(va - 0x400000, 40), va))
    head = ins[0]
    slot = None
    for i in ins:
        if i.mnemonic in ("jmp", "call") and "[eax + " in i.op_str:
            slot = int(i.op_str.split("+ ")[1].rstrip("]"), 16); site = i.address; break
        if i.mnemonic == "ret":
            break
    if slot is None or head.op_str != "ecx, dword ptr [ecx + 0x14]":
        out.writerow([hex(va), n, "UNRESOLVED", head.op_str]); continue
    dev, vt = VT[cls]
    tgt = sdw(vt + slot); b = body(tgt)
    out.writerow([hex(va), n, dev, hex(slot), hex(site), hex(tgt), hex(b), names.get(("sound.dll", b), "")])
