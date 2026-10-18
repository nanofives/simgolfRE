"""Merge the phase-1 stream address maps into re/model/addresses.tsv and check them (stream G1, 2026-10-18).

    py -3.12 re/tools/model_merge.py merge      [--out re/model/addresses.tsv]   # merge + overlap scan + coverage
    py -3.12 re/tools/model_merge.py overlaps                                      # overlap scan only (exit 1 on any)
    py -3.12 re/tools/model_merge.py coverage   [--json <file>]                    # cluster / address coverage
    py -3.12 re/tools/model_merge.py checkinput [--out log/p1/g1/check_all.tsv]    # model_check.py input of the map
    py -3.12 re/tools/model_merge.py hooks      [--out log/p1/g1/hooks.tsv]        # hook literal cross-check

Inputs: re/model/{b,c,d,e1,e2,f}_addresses.tsv (one row per typed object, each stream's own map), the decisions
below (RESOLUTIONS: what happens to a stream row another stream also types; ADDITIONS: objects stream G types itself),
re/model/inventory_addresses.tsv / inventory_clusters.tsv / ownership*.tsv (stream A) for coverage.

Overlap rule: two rows whose byte ranges intersect must be nested and the inner row must carry `relation`
member_of / element_of / alias_of with `parent` = the outer row's symbol; any other intersection is an overlap and
`overlaps` exits 1. Every resolution cites the instruction that decides it (NO-GUESSING).
"""
import argparse, collections, csv, json, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
MODEL = ROOT / "re" / "model"
STREAMS = ["b", "c", "d", "e1", "e2", "f"]
COLS = ["symbol", "module", "va", "size", "element_type", "c_type", "count", "stride", "signed", "stream",
        "sve_field", "clusters", "relation", "parent", "evidence", "confidence", "stream_symbol"]

# Header type that the merged header uses for a stream's element type (the same object typed twice: D's opaque
# placeholders and E1's early names are E2's classes; simgolf_model.h asserts the sizes and offsets agree).
CANONICAL_TYPE = {
    "SgSnd": "Snd", "SgSurfaceObj": "SpriteRef", "GfxSurface": "SpriteRef", "SurfaceRef": "SpriteRef",
    "SgFlicObj": "Flic", "SgFlicStream": "FlicFrame", "SgCache58": "PaletteCache", "SgSndGroupDevice": "NodeList",
    "SgCursorObj": "SgCursor", "Surface": "SpriteRef",
    # spellings of the C99 scalar types
    "int8": "int8_t", "uint8": "uint8_t", "int16": "int16_t", "uint16": "uint16_t", "int32": "int32_t",
    "uint32": "uint32_t", "ptr": "void *", "pointer": "void *", "void": "void",
}

# ---------------------------------------------------------------------------------------------------------------
# RESOLUTIONS: (stream, symbol) -> change. action: drop (the bytes are typed by `parent`; the row is kept in the
# merged map as relation member_of / element_of so the stream's symbol still resolves), edit (field values replaced),
# relation (nested row kept with a relation). Each carries the deciding evidence.
RESOLUTIONS = [
    # D's building-name fields are F's F_BuildItem records 7, 9, 12, 14 (+0x00 name) and record 19 +0x12: the table
    # base is 0x4c26b0 with stride 0x14 (buildToolLabel `lea edi, [ecx*4 + 0x4c26b0]` 0x0040a160 with ecx = idx * 5;
    # loadWorldAssets' copies to 0x4c273c / 0x4c2764 / 0x4c27a0 / 0x4c27c8 = 0x4c26b0 + {7, 9, 12, 14} * 0x14),
    # not +4 of a record at 0x4c2738 as d_addresses.tsv said.
    dict(stream="d", symbol="g_buildingNames", action="member", parent="f_tab_buildItems_4c26b0",
         note="record 7 +0x00 name (0x4c273c = 0x4c26b0 + 7 * 0x14)"),
    dict(stream="d", symbol="g_buildingName4c2764", action="member", parent="f_tab_buildItems_4c26b0",
         note="record 9 +0x00 name"),
    dict(stream="d", symbol="g_buildingName4c27a0", action="member", parent="f_tab_buildItems_4c26b0",
         note="record 12 +0x00 name"),
    dict(stream="d", symbol="g_buildingName4c27c8", action="member", parent="f_tab_buildItems_4c26b0",
         note="record 14 +0x00 name"),
    dict(stream="d", symbol="g_short4c283e", action="member", parent="f_tab_buildItems_4c26b0",
         note="record 19 +0x12 value12 (0x4c283e = 0x4c26b0 + 19 * 0x14 + 0x12)"),
    # B's two save words are dwords 1 and 2 of F's int32[4] at 0x4c2848 (sve fields 24 / 35, separate readers).
    dict(stream="b", symbol="g_objectOfferCount", action="member", parent="f_w_4c2848", note="element 1"),
    dict(stream="b", symbol="g_greensFee", action="member", parent="f_w_4c2848", note="element 2"),
    # F's two tail-A buffers are bytes of golfer-type records 76.. (customizeCharacter writes record 76's name and
    # look bytes there; g_golferTypes 0x4d6088 + 76 * 0x230 = 0x4e06c8).
    dict(stream="f", symbol="f_buf_4e06c8", action="member", parent="g_golferTypes",
         note="record 76 +0x00 (0x4d6088 + 76 * 0x230 = 0x4e06c8; save tail A starts here)"),
    dict(stream="f", symbol="f_w_4e06e8", action="member", parent="g_golferTypes", note="record 76 +0x20"),
    # A string that starts inside another referenced string or table (suffix pointers): an alias view.
    dict(stream="f", symbol="f_strpool_4bba89", action="alias", parent="f_strpool_4bba88",
         note="suffix of the pool at 0x4bba88"),
    dict(stream="f", symbol="f_strpool_4c266f", action="member", parent="f_tab_locations_4c1e90",
         note="inside record 15 text41 (0x4c1e90 + 15 * 0x82 + 0x41 = 0x4c2657 .. 0x4c2698)"),
    dict(stream="f", symbol="f_k_4d32d6", action="alias", parent="f_strpool_4d317c",
         note="last byte of the pool read as a byte constant"),
    dict(stream="f", symbol="f_k_4e43d0", action="alias", parent="f_strpool_4e43ac",
         note="last byte of the pool read as a byte constant"),
    # E1's g_ui_4c79ea is F's F_Point16 table entries 13..15 (0x4c79b4 + 13 * 4 + 2 = 0x4c79ea).
    dict(stream="e1", symbol="g_ui_4c79ea", action="member", parent="f_tab_point16_4c79b4",
         note="entries 13 +2 .. 15 (0x4c79ea - 0x4c79b4 = 0x36)"),
    # D's single Surfaces are elements of other streams' SpriteRef arrays (E2 section 11, B section 2.2).
    dict(stream="d", symbol="g_surface587fdc", action="element", parent="g_surfaces587ea8",
         note="element 7 of the Surface[224] array (0x587fdc = 0x587ea8 + 7 * 0x2c)"),
    dict(stream="d", symbol="g_surface58baa0", action="element", parent="g_sprites58b680",
         note="element 24 (0x58baa0 = 0x58b680 + 24 * 0x2c); loadInterfaceArt steps a pointer from there"),
    dict(stream="d", symbol="g_surface5a57e8", action="element", parent="g_sprites5a53c8",
         note="element 24 (0x5a57e8 = 0x5a53c8 + 24 * 0x2c)"),
    # D and E1 typed the same 8-element SpriteRef array (initGlobalArray_004050b0 constructs 8 x 0x2c at 0x5678b8):
    # one row, E1's (both carry the constructor evidence; the class name is E2's SpriteRef).
    dict(stream="d", symbol="g_surfaces5678b8", action="drop_dup", parent="g_surfaces5678b8",
         note="same object as e1's row (ehVectorConstructorIterator(0x5678b8, 0x2c, 8, Surface_ctor))"),
    # D's credits pointers: 0x83b15c / 0x83b160 are Reader +0x15c / +0x160 of g_sectionReader (Reader::allocBuffers'
    # two line buffers); 0x83b164 / 0x83b168 are separate globals (E2 section 11).
    # D typed 0x519a20 as a 0x2c Surface (low); staticInit_00404700 constructs it with C4837f0::ctor (`mov ecx,
    # 0x519a20; jmp 0x4837f0` at 0x00404700), the FontRef constructor of the 11 other g_font* objects, and its atexit
    # routine 0x00404720 stores the FontRef vtable 0x4ba468 and tail-calls releaseBacking 0x004838b0: a FontRef of
    # 0x20 bytes, ending where E1's g_fontRef_519a40 starts.
    dict(stream="d", symbol="g_object519a20", action="edit", size=0x20, stride=0x20, element_type="FontRef",
         note="FontRef (C4837f0::ctor from 0x00404700, vtable 0x4ba468 stored at 0x00404720), 0x20 bytes"),
    # D typed the Cursor object as its vtable dword (`the object size is not read anywhere`); Cursor::ctor 0x488490
    # stores [eax] = 0x4ba7f4 and zeroes +4, +8, +0xc, +0x10 (0x00488494..0x004884a3): E1's SgCursor, 0x14 bytes.
    dict(stream="d", symbol="g_cursor822cc8", action="edit", size=0x14, stride=0x14, element_type="SgCursor",
         note="SgCursor of 0x14 bytes: Cursor::ctor (called from 0x0045ba60) writes +0x00..+0x10 (0x00488494..0x004884a3)"),
    # F typed 0x4c1a40 as a 5-byte string; it is the file image of the tile-type attribute table: mainLoop and
    # loadWorldAssets copy 0x114 dwords from it to tile_type_attr 0x578350 (`mov ecx, 0x114; mov esi, 0x4c1a40;
    # mov edi, 0x578350; rep movsd` at 0x0040f5f7, 0x00440f9a, 0x004411d6, 0x004412cf, 0x004414a6): 23 x 0x30
    # TileTypeAttr, ending where f_tab_locations_4c1e90 starts. The string is record 0's name field.
    dict(stream="f", symbol="f_strpool_4c1a40", action="edit", rename="f_tab_tileTypeAttrInit_4c1a40", size=0x450,
         count=23, stride=0x30, element_type="TileTypeAttr",
         note="const TileTypeAttr[23], the file image copied to tile_type_attr (rep movsd 0x114 dwords at 0x0040f5f7, "
              "0x00440f9a, 0x004411d6, 0x004412cf, 0x004414a6)"),
    # F's f_w_4c2e14 claimed two dwords; only 0x4c2e14 is referenced (no literal 0x4c2e18 in the image) and save
    # field 46 is 4 bytes: g_proChallenger, a g_golfPros index set at 0x0041037d, -1 at 0x0040f48b (G2's
    # sve_field_status.md 3, re-read for this merge).
    dict(stream="f", symbol="f_w_4c2e14", action="edit", size=4, count=1, stride=4, element_type="int32_t",
         note="one int32 (save field 46, g_proChallenger); no instruction references 0x4c2e18"),
    dict(stream="d", symbol="g_credits_text_ptrs", action="edit", va=0x83b164, size=8, count=2,
         note="starts at 0x83b164 with two pointers; 0x83b15c / 0x83b160 are g_sectionReader +0x15c / +0x160"),
]

# ADDITIONS: objects stream G types (clusters no stream typed: stream A's X clusters, the money-popup ring).
def _g(symbol, va, size, et, count, stride, signed, clusters, evidence, confidence="high", sve="-"):
    return dict(symbol=symbol, module="golf_clean.exe", va=va, size=size, element_type=et,
                c_type=CANONICAL_TYPE.get(et, et), count=count, stride=stride, signed=signed, stream="g1",
                sve_field=sve, clusters=clusters, relation="top", parent="-", evidence=evidence,
                confidence=confidence)


G_ROWS = [
    _g("g_animSprites", 0x509688, 591 * 0x2c, "SpriteRef", 591, 0x2c, "-", "497",
       "initGlobalArray_00403fe0 pushes (0x509688, 0x2c, 0x24f, Surface_ctor 0x473ab0, 0x4041f0) to "
       "ehVectorConstructorIterator 0x4a55d2 (0x00403fe0..0x00403ff6); FUN_00404010 destroys the same 0x24f x 0x2c "
       "(0x4a5713, 0x00404010..0x00404021); the fourth 591-entry animation array next to g_animPalettes / g_flicObjs / "
       "g_flicStreams; no other instruction holds the literal"),
    _g("g_sprites540d98", 0x540d98, 0x20 * 0x2c, "SpriteRef", 0x20, 0x2c, "-", "609",
       "initGlobalArray_00404f50 pushes (0x540d98, 0x2c, 0x20, Surface_ctor 0x473ab0, 0x4041f0) to "
       "ehVectorConstructorIterator 0x4a55d2 (0x00404f50..0x00404f63); FUN_00405010 destroys 0x20 x 0x2c "
       "(0x00405085..0x00405093)"),
    _g("g_saveIoRead", 0x53e638, 4, "int32_t", 1, 4, "-", "566",
       "saveFieldIo 0x0040af70 loads it and calls __read 0x4a583a when nonzero, __write 0x4a5b58 when zero "
       "(0x0040af7f / 0x0040af88); serializeCourse 0x0040afab and deserializeCourse 0x0040bbfb store their first "
       "argument into it"),
    _g("g_jpegSrcPitch", 0x56125c, 4, "int32_t", 1, 4, "s", "670",
       "saveCourseJpg stores the source Surface's slot +0xe0 result (jgld Surface::getField40) or 0 at 0x00431dda; "
       "FUN_00431be0 reads it at 0x00431c3d and advances its 16-bit pixel pointer by 2 * value per row "
       "(lea ebx, [eax + eax] 0x00431c42, add edx, ebx 0x00431c4e) and scales the row cursor 0x543cf0 by it "
       "(0x00431cfb..0x00431d12): the row pitch in pixels"),
    _g("g_namePool", 0x56fcb0, 0x1002, "char", 0x1002, 1, "-", "779",
       "save field 76 (re/tools/sve.py FIELDS, 0x1002 bytes; serializeCourse 0x0040b3ba..0x0040b3c4, "
       "deserializeCourse 0x0040bea8..0x0040beb2); clearBuffers zeroes 0x400 dwords + 1 word (0x0045b881..0x0045b88f); "
       "appendToBuffer45b8b0 addresses entry k at offset g_namePoolOffset[k] (lea ebx, [edx + 0x56fcb0] 0x0045b8d3)",
       sve="76"),
    _g("g_namePoolOffset", 0x59d81c, 0x100, "int16_t", 0x80, 2, "s", "969",
       "save field 77 (0x100 bytes, serializeCourse 0x0040b3c9..0x0040b3d3); clearBuffers fills 0x40 dwords with -1 "
       "(0x0045b891..0x0045b89e); appendToBuffer45b8b0 reads word [k*2 + 0x59d81c] and stops at 0xffff "
       "(0x0045b8bd..0x0045b8c9), movsx it as the offset into g_namePool (0x0045b8cb), stores 0xffff "
       "(0x0045b907) and rewrites every offset above the removed one (loop 0x0045b916..0x0045b92e, end 0x59d91c)",
       sve="77"),
    _g("g_pendingCourseInfo", 0x587da0, 0x587e7c - 0x587da0, "char", 0x587e7c - 0x587da0, 1, "-", "874",
       "FUN_004321a0 copies a string into it (rep movsd / movsb 0x004321b5..0x004321c4); flushCourseInfo tests its "
       "first byte (0x00432170), passes it to writeCourseInfo 0x431fa0 and saveThumbAndFull 0x431ee0 "
       "(0x00432179..0x00432188) and stores 0 in the first byte (0x00432190); extent bounded by the next "
       "referenced address 0x587e7c (the biased base of g_surfaces587ea8); no instruction fixes the length",
       confidence="low"),
    _g("g_resampleBuf", 0x83ac10, 4, "int16_t*", 1, 4, "-", "1685",
       "allocResampleTable stores malloc's result (0x004818b4 call 0x4a504f, 0x004818c0) and writes / reads words "
       "through it (mov word ptr [edx + ecx*2], ax 0x00481a8b; sub si, word ptr [ecx + edx*2] 0x00481ab5; "
       "0x00481ac8); freeResampleTable 0x481b20 releases it"),
    _g("g_keyTables", 0x83b170, 4 * 0x120, "KeyTable", 4, 0x120, "-", "1710",
       "initKeyTableArray pushes (0x83b170, 0x120, 4, KeyTable ctor 0x4882a0, dtor 0x4882f0) to "
       "ehVectorConstructorIterator 0x4a55d2 (0x00487e10..0x00487e26); KeyTable::lookup walks esi from 0x83b170 by "
       "0x120 to the end 0x83b5f0 (0x0048842b, 0x00488447..0x00488453) = 4 x 0x120"),
    _g("g_sinTable", 0x83b9f4, 0x400, "int32_t", 256, 4, "s", "1722",
       "initSinTable stores 256 dwords (esi from 0x83b9f4 to 0x83bdf4 step 4, 0x00491c1a..0x00491c4a) = "
       "__ftol(sin(i * [0x4bba50]) * 65535.0), with the double at 0x4bba50 = (pi / 2) / 255, so entry 255 = sin(pi/2); "
       "sinScaled 0x491c70 reads [i] and [i + 1] for i = (x & 0x3fffffff) >> 22 = 0..255 (0x00491ca0 / 0x00491ca7)"),
    _g("g_dataDirA", 0x83be04, 0x100, "char", 0x100, 1, "-", "1723",
       "resolveDataPath tests its first byte (0x00491f53) and copies it into g_pathScratch (repne scasb 0x00491f71); "
       "the two literals 0x00491f54 / 0x00491f61 are the only ones in the image: never written, always empty; "
       "extent = distance to g_dataDirB", confidence="medium"),
    _g("g_dataDirB", 0x83bf04, 0x100, "char", 0x100, 1, "-", "1724",
       "resolveDataPath tests its first byte (0x00491e38) and copies it (0x00491e4b..0x00491e5c); literals only at "
       "0x00491e39 / 0x00491e4c: never written, always empty; extent = distance to g_pathScratch",
       confidence="medium"),
    _g("g_pathScratch", 0x83c004, 0x100, "char", 0x100, 1, "-", "1725",
       "resolveDataPath writes it (mov byte ptr [0x83c004], 0 0x00491dde; strcpy / strcat 0x00491def..) and returns "
       "it (mov eax, 0x83c004 0x00491dc4); Reader::open copies it (0x004880b1..0x004880be); extent = distance to "
       "g_dataDirC", confidence="medium"),
    _g("g_dataDirC", 0x83c104, 1, "char", 1, 1, "-", "1726",
       "resolveDataPath copies it (mov edi, 0x83c104 0x00491ecf, repne scasb 0x00491ee0); the literal at 0x00491ed0 "
       "is the only one in the image: never written, always the empty string; only its first byte is ever read "
       "(no instruction fixes the length)", confidence="low"),
    _g("f_k_dbl_4ba2a0", 0x4ba2a0, 8, "double", 1, 8, "-", "9",
       "allocResampleTable reads it as a qword operand of x87 code (fsub 0x00481906, fdivr 0x00481919, fld 0x00481931); "
       "the bytes are the double 1.0 (high dword 0x3ff00000 at 0x4ba2a4); it follows vbtable 0x4ba28c (2 entries: 0, "
       "0x14) and 12 zero bytes; no instruction writes it (.rdata)"),
    _g("g_moneyLabelX", 0x542fd8, 0x20, "int32_t", 8, 4, "s", "647",
       "pointsPopup stores the popup x at [ring * 4 + 0x542fd8] (0x0040c8cf) with ring = [0x59abb0] and "
       "(ring + 1) & 7 (0x0040c8f3..0x0040c8f4); drawMoneyLabels reads [ebx + 0x542fd8] (0x0040c94e) and passes it "
       "to the world-to-screen 0x42fb90; parallel to g_moneyLabelValue 0x542dd8 / g_moneyLabelTime 0x542f00"),
    _g("g_moneyLabelY", 0x542ff8, 0x20, "int32_t", 8, 4, "s", "647",
       "pointsPopup stores the popup y at [ring * 4 + 0x542ff8] (0x0040c8da); drawMoneyLabels reads it "
       "(0x0040c936); ends at C's tile_level 0x543018"),
]

# Addresses of a cluster that are no object of their own (coverage marks): biased bases and loop ends.
MARKS = {
    0x005722e7: "biased base of tile_type (column y - 1): mainLoop 0x00411611, scanTileLine 0x004222bc, trialShot "
                "0x004228c2, updateGolfers 0x0042c7db / 0x0042cbbe index it with x * 50 + y (edge read, model_defects.md)",
    0x00587e7c: "biased base of g_surfaces587ea8 (element k - 1, k >= 1): mainLoop `lea ecx, [eax*4 + 0x587e7c]` "
                "0x00410ad9 / 0x00410ce3 with eax = (..) * 11, 0x2c-byte steps",
    0x005a8c34: "biased base of g_objectTypeLevel 0x5a8c38 (1-based index): mainLoop `mov esi, [ebp*4 + 0x5a8c34]` "
                "0x00413146 after `test ebp, ebp; je` 0x0041313e",
    0x00838a8c: "loop end of g_storyFiles (8 x 0x10c from 0x838228) for a pointer at record +4: loadStoryStrings "
                "`add eax, 0x10c; cmp eax, 0x838a8c` 0x00466bae..0x00466bb4 reading [eax + 4]",
    0x00838c1b: "biased base of C's 19 x 19 grid at 0x838c1c (column k - 1, k = 1..16): genNoise "
                "`mov byte ptr [eax + edx + 0x838c1b], bl` 0x0046746e after `inc edx` 0x0046746a",
    0x0056e950: "loop end of the animal table for a pointer at record +0x28: freeRecordAtPos `cmp eax, 0x56e950` "
                "0x004011d6 (0x56d1b8 + 100 * 0x3c + 0x28)",
    0x0059d91c: "loop end of g_namePoolOffset (0x80 words): appendToBuffer45b8b0 `add ecx, 2; cmp ecx, 0x59d91c` "
                "0x0045b925..0x0045b92e (the only reference; nothing reads or writes the dword)",
}


def num(x):
    x = (x or "").strip()
    if not x or x == "-" or x == "var":
        return None
    return int(x, 16) if x.lower().startswith("0x") else int(x)


def hx(v):
    return f"0x{v:08x}" if v is not None else "-"


def load_stream(s):
    out = []
    path = MODEL / f"{s}_addresses.tsv"
    for r in csv.DictReader(open(path, encoding="utf-8"), delimiter="\t"):
        va = num(r["va"])
        size = num(r["size"])
        count = num(r.get("count"))
        stride = num(r.get("stride"))
        if size is None and count and stride:
            size = count * stride
        et = (r.get("element_type") or "").strip()
        base = re.sub(r"^struct\s+", "", et)
        base = re.split(r"[\s\[(*]", base)[0]
        if et.startswith("{"):
            ctype = "struct " + et
        else:
            ctype = CANONICAL_TYPE.get(base, base)
            if "*" in et.split("[")[0] and not ctype.endswith("*"):
                ctype += " *"
        out.append(dict(symbol=r["symbol"].strip(), module=r["module"].strip(), va=va, size=size or 1,
                        element_type=et, c_type=ctype, count=count, stride=stride,
                        signed=(r.get("signed") or "-").strip() or "-", stream=s,
                        sve_field=(r.get("sve_field") or "-").strip() or "-",
                        clusters=(r.get("clusters") or r.get("cluster") or "-").strip() or "-",
                        relation="top", parent="-", evidence=(r.get("evidence") or "").strip(),
                        confidence=(r.get("confidence") or "").strip()))
    return out


def load_e2_vtables(have):
    """Stream E2 lists its 44 exe vtables in e2_vtables.tsv only (slot rows); one map row per vtable whose start no
    other row has (E1's vt_* rows already cover the window family)."""
    vt = collections.OrderedDict()
    for r in csv.DictReader(open(MODEL / "e2_vtables.tsv", encoding="utf-8"), delimiter="\t"):
        if r["module"] != "golf_clean.exe":
            continue
        va = int(r["vtable"], 16)
        e = vt.setdefault(va, dict(cls=r["class"], slots=0, ev=r["evidence"]))
        e["slots"] = max(e["slots"], int(r["slot"]) + 1)
    out = []
    for va, e in vt.items():
        if va in have:
            continue
        out.append(dict(symbol=f"vt_{re.sub(r'[^A-Za-z0-9_]', '_', e['cls'])}_{va:06x}", module="golf_clean.exe",
                        va=va, size=4 * e["slots"], element_type="vtable", c_type="vtable", count=e["slots"],
                        stride=4, signed="-", stream="e2", sve_field="-", clusters="-", relation="top", parent="-",
                        evidence=f"{e['slots']} slots in re/model/e2_vtables.tsv; {e['ev']}", confidence="high"))
    return out


def merged_rows():
    rows = []
    for s in STREAMS:
        rows += load_stream(s)
    rows += [dict(r) for r in G_ROWS]
    rows += load_e2_vtables({r["va"] for r in rows})
    by = {(r["stream"], r["symbol"]): r for r in rows}
    keep = []
    dropped = []
    for res in RESOLUTIONS:
        r = by.get((res["stream"], res["symbol"]))
        if r is None:
            raise SystemExit(f"resolution for a missing row: {res['stream']}:{res['symbol']}")
        a = res["action"]
        tag = f"[G1: {res['note']}]"
        if a == "drop_dup":
            r["_drop"] = True
            dropped.append((r, res))
            continue
        if a == "edit":
            for k in ("va", "size", "count", "stride", "element_type", "confidence"):
                if k in res:
                    r[k] = res[k]
            if "element_type" in res:
                r["c_type"] = CANONICAL_TYPE.get(res["element_type"], res["element_type"])
            if "rename" in res:
                r["evidence"] = f"[G1: was {r['symbol']}] " + r["evidence"]
                r["symbol"] = res["rename"]
        else:
            r["relation"] = {"member": "member_of", "element": "element_of", "alias": "alias_of"}[a]
            r["parent"] = res["parent"]
        r["evidence"] = f"{r['evidence']} {tag}".strip()
    keep = [r for r in rows if not r.get("_drop")]
    # The names files are the one naming source (Ghidra labels come from them): a row whose start carries a label
    # takes that label as its symbol; the stream's own symbol stays in `stream_symbol`.
    labels = names_labels()
    for r in keep:
        r["stream_symbol"] = r["symbol"]
        lab = labels.get((r["module"], r["va"]))
        if lab and r["relation"] == "top":
            r["symbol"] = lab
    parents = {r["stream_symbol"]: r["symbol"] for r in keep}
    for r in keep:
        if r["parent"] != "-":
            r["parent"] = parents.get(r["parent"], r["parent"])
    return keep, dropped


def names_labels():
    import glob
    out = {}
    for f in sorted(glob.glob(str(ROOT / "re" / "names" / "*_globals.tsv"))):
        for l in open(f, encoding="utf-8").read().splitlines()[1:]:
            c = l.split("	")
            if len(c) == 5:
                out.setdefault((c[0], int(c[1], 16)), c[2])
    return out


def overlaps(rows):
    """Pairs of rows whose byte ranges intersect and are not a declared nested relation."""
    sym = {}
    for r in rows:
        sym.setdefault(r["symbol"], []).append(r)
    rs = sorted(rows, key=lambda r: (r["module"], r["va"], -r["size"]))
    bad, nested = [], []
    active = []
    for r in rs:
        active = [a for a in active if a["module"] == r["module"] and a["va"] + a["size"] > r["va"]]
        for a in active:
            inside = a["va"] <= r["va"] and r["va"] + r["size"] <= a["va"] + a["size"]
            declared = inside and r["relation"] != "top" and r["parent"] == a["symbol"]
            # a nested row may also sit inside the parent's parent (member of a member)
            if inside and r["relation"] != "top" and not declared:
                p = sym.get(r["parent"], [None])[0]
                while p is not None and p["relation"] != "top":
                    if p["parent"] == a["symbol"]:
                        declared = True
                        break
                    p = sym.get(p["parent"], [None])[0]
            if inside and a["relation"] != "top" and a["parent"] == r.get("parent") and r["relation"] != "top":
                declared = True  # two members of one parent that overlap each other (alias views)
            (nested if declared else bad).append((a, r))
        active.append(r)
    return bad, nested


def load_inventory():
    A = list(csv.DictReader(open(MODEL / "inventory_addresses.tsv", encoding="utf-8"), delimiter="\t"))
    C = {r["cluster"]: r for r in csv.DictReader(open(MODEL / "inventory_clusters.tsv", encoding="utf-8"),
                                                 delimiter="\t")}
    O = {r["cluster"]: r for r in csv.DictReader(open(MODEL / "ownership.tsv", encoding="utf-8"), delimiter="\t")}
    for r in csv.DictReader(open(MODEL / "ownership_e.tsv", encoding="utf-8"), delimiter="\t"):
        O[r["cluster"]]["stream"] = r["stream"]
    return A, C, O


def coverage(rows):
    A, C, O = load_inventory()
    exe = sorted((r for r in rows if r["module"] == "golf_clean.exe"), key=lambda r: r["va"])
    starts = [r["va"] for r in exe]
    import bisect
    ends = set(r["va"] + r["size"] for r in exe)

    def covering(va):
        best = None
        i = bisect.bisect_right(starts, va)
        for r in exe[max(0, i - 400):i]:
            if r["va"] <= va < r["va"] + r["size"]:
                if best is None or r["relation"] == "top":
                    best = r
        return best

    per_cluster = collections.defaultdict(list)
    for a in A:
        if a["kept"] != "1":
            continue
        va = int(a["va"], 16)
        r = covering(va)
        how = None
        if r is not None:
            how = "typed"
        elif a["role"] == "end_marker" and (va in ends or covering(va - 1) is not None):
            how = "end_marker"
        elif va in MARKS:
            how = "mark"
        per_cluster[a["cluster"]].append((va, how, r, int(a["functions"] or 0), a))
    res = {}
    for cid, c in C.items():
        items = per_cluster.get(cid, [])
        n = len(items)
        done = sum(1 for i in items if i[1])
        fw = int(c["functions"] or 0)
        typed_by = collections.Counter(i[2]["stream"] for i in items if i[2] is not None)
        res[cid] = dict(cluster=cid, lo=c["lo"], hi=c["hi"], section=c["section"], cls=c["class"],
                        owner=O[cid]["stream"], addresses=n, done=done, functions=fw,
                        status="typed" if n and done == n else ("partial" if done else "untyped"),
                        typed_by=",".join(f"{k}:{v}" for k, v in typed_by.most_common()),
                        missing=[hx(i[0]) for i in items if not i[1]], fnlist=c["function_list"])
    return res


def write_map(rows, out):
    rows = sorted(rows, key=lambda r: (r["module"], r["va"], -r["size"], r["symbol"]))
    with open(out, "w", encoding="utf-8", newline="") as f:
        f.write("\t".join(COLS) + "\n")
        for r in rows:
            v = dict(r)
            v["va"] = hx(r["va"])
            v["size"] = hex(r["size"])
            v["count"] = "-" if r["count"] is None else str(r["count"])
            v["stride"] = "-" if r["stride"] is None else hex(r["stride"])
            v["evidence"] = r["evidence"].replace("\t", " ").replace("\n", " ")
            f.write("\t".join(str(v[c]) for c in COLS) + "\n")


def struct_sizes():
    """sizeof of every struct the section headers assert (`..._STATIC_ASSERT(sizeof(Name) == N, ...)`)."""
    out = {}
    for s in STREAMS:
        t = (MODEL / "include" / "sections" / f"{s}.h").read_text(encoding="utf-8")
        for m in re.finditer(r"STATIC_ASSERT\(\s*sizeof\((\w+)\)\s*==\s*(0x[0-9a-fA-F]+|\d+)", t):
            out[m.group(1)] = int(m.group(2), 0)
    return out


def consistency(rows):
    """Rows typed with a header struct: element stride = sizeof, size = count * stride."""
    sizes = struct_sizes()
    bad = []
    for r in rows:
        t = r["c_type"]
        if t not in sizes:
            continue
        sz = sizes[t]
        stride = r["stride"] or sz
        cnt = r["count"] or 1
        if stride != sz or r["size"] != cnt * stride:
            bad.append((r, sz))
    return bad


# ---------------------------------------------------------------------------------------------------------------
# Merged header: re/model/include/simgolf_model.h is GENERATED from the six section files. The sections keep their
# instruction-address evidence in comments (e2.h, d.h); decision 2 keeps original addresses out of the header the
# port ships, so the generator rewrites every address token in a comment (code addresses dropped, data addresses
# replaced by the map symbol when the symbol carries no address, else dropped). Code lines never hold one (checked).
HEADER = MODEL / "include" / "simgolf_model.h"
SECTION_ORDER = ["f", "c", "b", "d", "e2", "e1"]  # no section uses another's types (checked); data -> classes
ADDR_RE = re.compile(r"0x00[4-8][0-9a-fA-F]{5}\b|0x10[0-9a-fA-F]{6}\b|0x[4-8][0-9a-fA-F]{5}\b")
ADDR_SEQ = (r"(?:{a})(?:\s*(?:\.\.|/|,|-|and|or)\s*(?:{a}))*(?:\.\.)?").format(
    a=r"0x00[4-8][0-9a-fA-F]{5}|0x10[0-9a-fA-F]{6}|0x[4-8][0-9a-fA-F]{5}")

INTEGRATION = r"""
/* =====================================================================================================
 * Integration (stream G1, 2026-10-18): the same object typed by two streams. The canonical type is the
 * class stream E2 / E1 named; the other spelling stays for the stream's own tables. These asserts keep the
 * two layouts identical (decisions in re/model/DECISIONS.md, 2026-10-18 G1 section).
 * ===================================================================================================== */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define SG_MODEL_STATIC_ASSERT(cond, tag) _Static_assert(cond, #tag)
#elif defined(__cplusplus)
#define SG_MODEL_STATIC_ASSERT(cond, tag) static_assert(cond, #tag)
#else
#define SG_MODEL_STATIC_ASSERT(cond, tag) typedef char sg_model_static_assert_##tag[(cond) ? 1 : -1]
#endif
#define SG_MODEL_MEMBER_SIZE(type, member) sizeof(((type *)0)->member)

/* D SgSnd = E2 Snd (one of the 300 sound slots) */
SG_MODEL_STATIC_ASSERT(sizeof(SgSnd) == sizeof(Snd), g1_snd_size);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, volume) == offsetof(Snd, volume), g1_snd_volume);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, pan) == offsetof(Snd, pan), g1_snd_pan);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, loop) == offsetof(Snd, loop), g1_snd_loop);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, field_34) == offsetof(Snd, field_34), g1_snd_34);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, field_38) == offsetof(Snd, rate), g1_snd_rate);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, group) == offsetof(Snd, group), g1_snd_group);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, voice) == offsetof(Snd, sound), g1_snd_sound);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, state) == offsetof(Snd, state), g1_snd_state);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, prev) == offsetof(Snd, prev), g1_snd_prev);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, next) == offsetof(Snd, next), g1_snd_next);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, path) == offsetof(Snd, name), g1_snd_name);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, mode) == offsetof(Snd, mode), g1_snd_mode);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, options) == offsetof(Snd, options), g1_snd_options);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, pitch) == offsetof(Snd, pitch), g1_snd_pitch);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, unk_60) == offsetof(Snd, pitch_scale), g1_snd_pitch_scale);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, length_ms) == offsetof(Snd, length_ms), g1_snd_length);
SG_MODEL_STATIC_ASSERT(offsetof(SgSnd, started_at) == offsetof(Snd, started_at), g1_snd_started);

/* D SgSndGroupDevice = E2 NodeList (the music device); D SgSndGroup = E2 NodeListBucket */
SG_MODEL_STATIC_ASSERT(sizeof(SgSndGroupDevice) == sizeof(NodeList), g1_nodelist_size);
SG_MODEL_STATIC_ASSERT(offsetof(SgSndGroupDevice, groups) == offsetof(NodeList, bucket), g1_nodelist_buckets);
SG_MODEL_STATIC_ASSERT(sizeof(SgSndGroup) == sizeof(NodeListBucket), g1_bucket_size);
SG_MODEL_STATIC_ASSERT(offsetof(SgSndGroup, enabled) == offsetof(NodeListBucket, enabled), g1_bucket_enabled);
SG_MODEL_STATIC_ASSERT(offsetof(SgSndGroup, muted) == offsetof(NodeListBucket, muted), g1_bucket_muted);
SG_MODEL_STATIC_ASSERT(offsetof(SgSndGroup, gain) == offsetof(NodeListBucket, gain), g1_bucket_gain);

/* D's opaque placeholders = E2 classes */
SG_MODEL_STATIC_ASSERT(sizeof(SgSurfaceObj) == sizeof(SpriteRef), g1_spriteref);
SG_MODEL_STATIC_ASSERT(sizeof(SgFlicObj) == sizeof(Flic), g1_flic);
SG_MODEL_STATIC_ASSERT(sizeof(SgFlicStream) == sizeof(FlicFrame), g1_flicframe);
SG_MODEL_STATIC_ASSERT(sizeof(SgCache58) == sizeof(PaletteCache), g1_palettecache);

/* E1 keeps E2's classes as opaque bytes: sizes must agree */
SG_MODEL_STATIC_ASSERT(SGE1_IMAGE_SIZE == sizeof(Image), g1_image);
SG_MODEL_STATIC_ASSERT(sizeof(SgHotList) == SG_MODEL_MEMBER_SIZE(Image, hot_a), g1_hotlist_a);
SG_MODEL_STATIC_ASSERT(sizeof(SgHotList) == SG_MODEL_MEMBER_SIZE(Image, hot_b), g1_hotlist_b);
SG_MODEL_STATIC_ASSERT(SG_MODEL_MEMBER_SIZE(SgWindow, image) == sizeof(Image), g1_window_image);
"""


def strip_addresses(text, symbols):
    """Rewrite the address tokens of every comment of a section header (see HEADER above)."""
    def fix_comment(c):
        c = re.sub(r"\s*\(\s*(?:at\s+)?" + ADDR_SEQ + r"\s*\)", "", c)          # (0x.., 0x..)
        c = re.sub(r"\s*\bat\s+" + ADDR_SEQ, "", c)                             # at 0x..
        c = re.sub(r"\s*\bvtable\s+" + ADDR_SEQ, " vtable", c)

        def one(m):
            tok = m.group(0)
            v = int(tok, 16)
            sym = symbols.get(v)
            if sym and not re.search(r"[0-9a-f]{6}", sym):
                return sym
            return ""
        c = re.sub(ADDR_SEQ, lambda m: " ".join(x for x in (one(t) for t in ADDR_RE.finditer(m.group(0))) if x), c)
        c = re.sub(r"\(\s*[,/;]?\s*\)", "", c)
        c = re.sub(r"\(\s*[,/;]?\s+", "(", c)
        c = re.sub(r"\s*[,/]\s*\)", ")", c)
        c = re.sub(r"(?<=\S)[ \t]{2,}(?=\S)", " ", c)
        c = re.sub(r"\s+([,;:)])", r"\1", c)
        return c

    out = []
    pos = 0
    for m in re.finditer(r"/\*.*?\*/|//[^\n]*", text, flags=re.S):
        code = text[pos:m.start()]
        if ADDR_RE.search(code):
            raise SystemExit(f"address token in code: {code[-200:]!r}")
        out.append(code)
        out.append(fix_comment(m.group(0)))
        pos = m.end()
    out.append(text[pos:])
    return "".join(out)


def build_header():
    rows, _ = merged_rows()
    symbols = {}
    for r in rows:
        if r["relation"] == "top":
            symbols.setdefault(r["va"], r["stream_symbol"])  # stream symbols: the header must not change with a names-file rename
    parts = [
        "/* SimGolf v1.03 data model: the merged C99 header (phase 1, stream G1, 2026-10-18).\n"
        " *\n"
        " * GENERATED by `py -3.12 re/tools/model_merge.py header` from re/model/include/sections/{f,c,b,d,e2,e1}.h:\n"
        " * do not edit here, edit the section and regenerate (tests/test_model_header.py checks it is current).\n"
        " * Fixed-width types, sizes asserted, pointers of the original 32-bit process stored as uint32_t.\n"
        " * No original address appears here (decision 2): every object's address is in re/model/addresses.tsv\n"
        " * (symbol -> va, size, type, evidence); the instruction-address evidence of each member is in the section\n"
        " * files and the stream tables (e2_classes.tsv, e1_fields.tsv, b_types.tsv, ...). Original defects are\n"
        " * catalogued in re/analysis/data/model_defects.md, never repaired in the layouts (decision 3).\n"
        " */\n"
        "#ifndef SIMGOLF_MODEL_H\n#define SIMGOLF_MODEL_H\n\n#include <stdint.h>\n#include <stddef.h>\n",
    ]
    for s in SECTION_ORDER:
        t = (MODEL / "include" / "sections" / f"{s}.h").read_text(encoding="utf-8")
        t = strip_addresses(t, symbols)
        parts.append(f"\n/* ===== section {s} (re/model/include/sections/{s}.h) ===== */\n")
        parts.append(t.rstrip() + "\n")
    parts.append(INTEGRATION)
    parts.append("\n#endif /* SIMGOLF_MODEL_H */\n")
    text = "".join(parts)
    left = [m.group(0) for m in ADDR_RE.finditer(text)]
    if left:
        raise SystemExit(f"address tokens left in the header: {left[:10]}")
    return text


def cmd_header(args):
    text = build_header()
    if args.check:
        cur = HEADER.read_text(encoding="utf-8") if HEADER.exists() else ""
        if cur != text:
            print("simgolf_model.h is not current: run `re/tools/model_merge.py header`")
            return 1
        print("simgolf_model.h is current")
        return 0
    HEADER.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote {HEADER} ({text.count(chr(10))} lines)")
    return 0


def cmd_merge(args):
    rows, dropped = merged_rows()
    syms = collections.Counter(r["symbol"] for r in rows)
    dup = [s for s, n in syms.items() if n > 1]
    bad, nested = overlaps(rows)
    incons = consistency(rows)
    write_map(rows, args.out)
    print(f"rows {len(rows)} (dropped duplicates {len(dropped)}), nested {len(nested)}, overlaps {len(bad)}, "
          f"duplicate symbols {len(dup)}, struct size disagreements {len(incons)}")
    for r, sz in incons:
        print(f"  SIZE {r['stream']}:{r['symbol']} {r['c_type']} sizeof {sz:#x} row size {r['size']:#x} "
              f"count {r['count']} stride {r['stride']}")
    for a, b in bad:
        print(f"  OVERLAP {a['stream']}:{a['symbol']} {hx(a['va'])}+{a['size']:#x} <> "
              f"{b['stream']}:{b['symbol']} {hx(b['va'])}+{b['size']:#x}")
    for s in dup:
        print("  DUPLICATE SYMBOL", s)
    return 1 if bad or dup else 0


def cmd_overlaps(args):
    rows, _ = merged_rows()
    bad, nested = overlaps(rows)
    for a, b in nested:
        print(f"nested {b['relation']:10s} {b['stream']}:{b['symbol']} in {a['stream']}:{a['symbol']}")
    for a, b in bad:
        print(f"OVERLAP {a['stream']}:{a['symbol']} {hx(a['va'])}+{a['size']:#x} <> "
              f"{b['stream']}:{b['symbol']} {hx(b['va'])}+{b['size']:#x}")
    print(f"nested {len(nested)}, overlaps {len(bad)}")
    return 1 if bad else 0


def cmd_coverage(args):
    rows, _ = merged_rows()
    cov = coverage(rows)
    tot = collections.defaultdict(lambda: collections.Counter())
    for c in cov.values():
        for key in (c["owner"], "all"):
            t = tot[key]
            t["clusters"] += 1
            t["clusters_typed"] += c["status"] == "typed"
            t["clusters_partial"] += c["status"] == "partial"
            t["addresses"] += c["addresses"]
            t["addresses_done"] += c["done"]
            t["fw"] += c["functions"]
            t["fw_done"] += c["functions"] * c["done"] / c["addresses"] if c["addresses"] else 0
    print("owner  clusters typed partial | addresses done | fn-weight done")
    for k in sorted(tot):
        t = tot[k]
        print(f"{k:5s} {t['clusters']:5d} {t['clusters_typed']:5d} {t['clusters_partial']:4d} | {t['addresses']:5d} "
              f"{t['addresses_done']:5d} | {t['fw']:6d} {t['fw_done']:8.1f} ({100 * t['fw_done'] / max(1, t['fw']):.2f}%)")
    for c in cov.values():
        if c["status"] != "typed":
            print(f"  {c['status']:8s} cluster {c['cluster']} {c['lo']}-{c['hi']} owner {c['owner']} class {c['cls']} "
                  f"missing {' '.join(c['missing'][:8])} fns {c['fnlist'][:120]}")
    if args.json:
        json.dump({"clusters": cov, "totals": {k: dict(v) for k, v in tot.items()}}, open(args.json, "w"), indent=1)
    return 0


# ---------------------------------------------------------------------------------------------------------------
# Hook cross-check: every literal address in the code (comments stripped) of shim/src/re/*.cpp, negative-control
# sources excluded, that falls inside a typed range; the access width comes from the idiom around the literal
# (G<T>(..), at<T>, ref<T>, reinterpret_cast<T*>, (T*), G8 / G16 / G32, inline-asm `byte|word|dword|qword ptr [..]`),
# constant `+ k` terms after the literal are added, `* K` terms are an index (stride K). Verdicts: ok (width = the
# field's element width), narrow (narrower, aligned), wide (wider than the field element: a block copy or a
# disagreement, listed), unaligned, no_field, untyped (address only: lea / pointer base / table entry).
HOOK_W = {"char": 1, "signed char": 1, "unsigned char": 1, "uint8_t": 1, "int8_t": 1, "BYTE": 1, "bool": 1, "u8": 1,
          "s8": 1, "i8": 1, "short": 2, "unsigned short": 2, "uint16_t": 2, "int16_t": 2, "WORD": 2, "u16": 2, "s16": 2,
          "i16": 2, "int": 4, "unsigned": 4, "unsigned int": 4, "uint32_t": 4, "int32_t": 4, "DWORD": 4, "long": 4,
          "unsigned long": 4, "float": 4, "u32": 4, "s32": 4, "i32": 4, "double": 8, "__int64": 8, "uint64_t": 8}
HOOK_IDIOM = [
    (re.compile(r"\b(?:G|at|ref|Abs|Gv|R|W|fld|F|Get|Set|rd|wr|P|Pv|D|U|I|Rd|Wr)<\s*(?:const\s+)?(?:volatile\s+)?"
                r"([A-Za-z_][A-Za-z0-9_ ]*?)\s*(\*?)\s*>\s*\(\s*$"), "tmpl"),
    (re.compile(r"reinterpret_cast<\s*(?:const\s+)?(?:volatile\s+)?([A-Za-z_][A-Za-z0-9_ ]*?)\s*(?:const\s*)?\*\s*(\*?)"
                r"\s*>\s*\(\s*$"), "cast"),
    (re.compile(r"\(\s*(?:const\s+)?(?:volatile\s+)?([A-Za-z_][A-Za-z0-9_ ]*?)\s*\*\s*(\*?)\s*\)\s*\(?\s*$"), "ccast"),
    (re.compile(r"\b(G8|GU8|GS8|G16|GU16|GS16|G32|GU32|HD|HW|HB)\s*\(\s*$"), "fixed"),
    (re.compile(r"\b(byte|word|dword|qword)\s+ptr\s*\[[^\]]*$", re.I), "asm"),
]
HOOK_FIXED = {"HD": 4, "HW": 2, "HB": 1, "G8": 1, "GU8": 1, "GS8": 1, "G16": 2, "GU16": 2, "GS16": 2, "G32": 4,
              "GU32": 4, "byte": 1, "word": 2, "dword": 4, "qword": 8}
HOOK_LIT = re.compile(r"0x([0-9a-fA-F]{6,8})\b")


def strip_cpp_comments(src):
    src = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), src, flags=re.S)
    return "\n".join(l.split("//", 1)[0] for l in src.split("\n"))


def hook_tail(line, end):
    """constant offset and index strides of the expression after a literal, up to the enclosing ')' / ']' / ','"""
    depth, k = 0, end
    while k < len(line):
        ch = line[k]
        if ch in "([":
            depth += 1
        elif ch in ")]":
            if depth == 0:
                break
            depth -= 1
        elif ch in ",;" and depth == 0:
            break
        k += 1
    expr = line[end:k]
    off, strides = 0, []
    for sign, term in re.findall(r"([+-])\s*([^+-]+)", expr):
        term = term.strip()
        if re.fullmatch(r"0x[0-9a-fA-F]+|\d+", term):
            off += (1 if sign == "+" else -1) * int(term, 0)
        else:
            m = re.search(r"\*\s*(0x[0-9a-fA-F]+|\d+)|(0x[0-9a-fA-F]+|\d+)\s*\*", term)
            strides.append(int(m.group(1) or m.group(2), 0) if m else 1)
    return off, strides


def cmd_hooks(args):
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    import model_check as mc
    lines = [l for l in open(args.types, encoding="utf-8") if not l.startswith("#")]
    rs = []
    for r in csv.DictReader(lines, delimiter="\t"):
        g = mc.Range(r)
        rs.append((g.va, g.stride * g.count, g.stride, g, r["name"]))
    rs.sort(key=lambda x: x[0])
    starts = [x[0] for x in rs]
    import bisect
    out = []
    for p in sorted((ROOT / "shim" / "src" / "re").glob("*.cpp")):
        if "negctl" in p.name:
            continue
        code = strip_cpp_comments(p.read_text(encoding="utf-8", errors="replace"))
        for ln, line in enumerate(code.split("\n"), 1):
            for m in HOOK_LIT.finditer(line):
                lit = int(m.group(1), 16)
                off, strides = hook_tail(line, m.end())
                v = lit + off
                i = bisect.bisect_right(starts, v) - 1
                if i < 0 or not (rs[i][0] <= v < rs[i][0] + rs[i][1]):
                    continue
                va, size, stride, g, name = rs[i]
                pre = line[:m.start()]
                depth = 0
                for k in range(len(pre) - 1, -1, -1):
                    ch = pre[k]
                    if ch in ")]":
                        depth += 1
                    elif ch in "([":
                        if depth == 0:
                            pre = pre[:k + 1]
                            break
                        depth -= 1
                width, how = None, "address"
                for rx, kind in HOOK_IDIOM:
                    mm = rx.search(pre)
                    if mm:
                        if kind in ("fixed", "asm"):
                            width, how = HOOK_FIXED[mm.group(1).lower() if kind == "asm" else mm.group(1)], mm.group(1)
                        else:
                            t = mm.group(1).strip()
                            width, how = (4, t + "**") if mm.group(2) else (HOOK_W.get(t), t)
                        break
                rel = (v - va) % stride
                f = [x for x in g.fields if x.off <= rel < x.end]
                if not f:
                    verdict, fdesc, ew = "no_field", "-", None
                else:
                    f = f[0]
                    fdesc, ew = f"+{f.off:#x}:{f.typ}" + (f"[{f.n}]" if f.n > 1 else "") + (f" {f.name}" if f.name else ""), f.size
                    if width is None:
                        verdict = "untyped"
                    elif width == ew:
                        verdict = "ok" if (rel - f.off) % ew == 0 else "unaligned"
                    elif width < ew:
                        verdict = "narrow" if (rel - f.off) % width == 0 else "unaligned"
                    else:
                        verdict = "wide"
                bad_stride = [s for s in strides if s > 1 and s % stride and stride % s and not
                              (f and ew and s % ew == 0 and f.n > 1)] if f else []
                if verdict in ("ok", "narrow") and bad_stride:
                    verdict = "stride"
                out.append(dict(file=p.name, line=ln, addr=f"{v:#010x}", range=name, elem_off=f"{rel:#x}",
                                field=fdesc, field_width=ew or "-", hook_type=how, hook_width=width or "-",
                                strides=",".join(hex(s) for s in strides) or "-", verdict=verdict,
                                context=line.strip()[:160]))
    cols = ["file", "line", "addr", "range", "elem_off", "field", "field_width", "hook_type", "hook_width", "strides",
            "verdict", "context"]
    with open(args.out, "w", encoding="utf-8", newline="") as fh:
        fh.write("\t".join(cols) + "\n")
        for o in out:
            fh.write("\t".join(str(o[c]) for c in cols) + "\n")
    c = collections.Counter(o["verdict"] for o in out)
    print(f"{len(out)} literal uses in typed ranges: " + ", ".join(f"{k} {v}" for k, v in c.most_common()))
    return 0


# ---------------------------------------------------------------------------------------------------------------
# model_check.py input for the whole map: each stream's own check rows (they carry the struct field lists the streams
# verified, flattened where model_check has no nested structs), rows built from the map for G1's rows, E2's vtables
# and the rows a resolution edited; member / element / alias rows are covered by their parent's row.
STREAM_CHECKS = {"b": "re/model/b_types.tsv", "c": "log/p1/c/types.tsv", "d": "log/p1/d/d_check.tsv",
                 "e1": "log/p1/e1/check_types.tsv", "e2": "log/p1/e2/check_e2.tsv", "f": "log/p1/f/model_f_check.tsv"}
CHECK_COLS = ["module", "va", "size", "type", "count", "stride", "sign", "const", "fields", "name"]
SCALAR = {"char": "char", "int8_t": "char", "uint8_t": "uchar", "int16_t": "short", "uint16_t": "ushort",
          "int32_t": "int", "uint32_t": "uint", "float": "float", "double": "double", "flags32": "flags32",
          "flags16": "flags16", "flags8": "flags8", "vtable": "ptr", "vbtable": "int"}


def struct_file(name):
    for s in STREAMS:
        p = f"re/model/include/sections/{s}.h"
        if re.search(r"typedef\s+struct\s+" + re.escape(name) + r"\s*\{", (ROOT / p).read_text(encoding="utf-8")):
            return p
    return None


LEAF = {"int8_t": 1, "uint8_t": 1, "char": 1, "int16_t": 2, "uint16_t": 2, "int32_t": 4, "uint32_t": 4, "float": 4,
        "double": 8}


def struct_body(name):
    for s in STREAMS:
        t = (MODEL / "include" / "sections" / f"{s}.h").read_text(encoding="utf-8")
        m = re.search(r"typedef\s+struct\s+" + re.escape(name) + r"\s*\{(.*?)\}\s*" + re.escape(name) + r"\s*;", t, re.S)
        if m:
            return m.group(1)
    return None


def flatten_fields(name, base=0, out=None):
    """Leaf members of a section struct with nested structs expanded, as model_check `off:type[n]` fields. Offsets
    come from natural alignment of the members (the sections' `+0xNN` comments are checked against it)."""
    sizes = struct_sizes()
    out = [] if out is None else out
    body = struct_body(name)
    if body is None:
        raise ValueError(f"struct {name} not in a section")
    off = 0
    for line in body.splitlines():
        mm = re.match(r"\s*(?:struct\s+)?(\w+)\s*(\*?)\s*(\w+)\s*(?:\[(\w+)\])?\s*;(?:\s*/\*\s*\+(0x[0-9a-fA-F]+))?", line)
        if not mm:
            continue
        typ, ptr, field, n, cmt = mm.groups()
        n = int(n, 0) if n else 1
        if ptr:
            typ = "ptr"
        if typ in LEAF or typ == "ptr":
            size = LEAF.get(typ, 4)
            align = size
        elif typ in sizes:
            size = sizes[typ]
            align = 4  # every nested struct of the sections holds a 4-byte member (asserted sizes are multiples of 4)
        else:
            raise ValueError(f"{name}.{field}: type {typ} unknown")
        off = (off + align - 1) // align * align
        if cmt and int(cmt, 16) != off:
            raise ValueError(f"{name}.{field}: comment +{cmt}, layout +{off:#x}")
        if typ in LEAF or typ == "ptr":
            out.append(f"{base + off:#x}:{typ}" + (f"[{n}]" if n > 1 else ""))
        else:
            for i in range(n):
                flatten_fields(typ, base + off + i * size, out)
        off += size * n
    return out


def check_row(r):
    t = r["c_type"]
    row = dict(module=r["module"], va=hx(r["va"]), size=hex(r["size"]), type="", count=str(r["count"] or 1),
               stride=hex(r["stride"]) if r["stride"] else "", sign="", const="", fields="", name=r["symbol"])
    if t.endswith("*"):
        row["type"] = "ptr"
    elif t in SCALAR:
        row["type"] = SCALAR[t]
        if t in ("vtable", "vbtable"):
            row["const"] = "1"
    else:
        f = struct_file(t)
        if f is None:
            return None
        row["type"] = "struct"
        row["fields"] = ",".join(flatten_fields(t))
        if not row["stride"]:
            row["stride"] = hex(struct_sizes()[t])
    if row["type"] != "struct" and row["stride"] and int(row["stride"], 16) > {"char": 1, "uchar": 1, "short": 2,
                                                                                "ushort": 2, "double": 8}.get(row["type"], 4):
        row["stride"] = ""
    return row


def check_rows():
    rows, _ = merged_rows()
    edited = {(x["stream"], x["symbol"]) for x in RESOLUTIONS if x["action"] == "edit"}
    stream_rows = {}
    for s, p in STREAM_CHECKS.items():
        lines = [l for l in open(ROOT / p, encoding="utf-8") if not l.startswith("#")]
        for c in csv.DictReader(lines, delimiter="\t"):
            stream_rows[(s, c["name"].strip())] = c
    out, missing = [], []
    for r in rows:
        if r["relation"] != "top":
            continue
        c = None if (r["stream"], r["stream_symbol"]) in edited else stream_rows.get((r["stream"], r["stream_symbol"]))
        if c is None:
            c = check_row(r)
        if c is None:
            missing.append(r)
            continue
        out.append({k: (c.get(k) or "").strip() for k in CHECK_COLS})
    return out, missing


def cmd_checkinput(args):
    out, missing = check_rows()
    with open(args.out, "w", encoding="utf-8", newline="") as f:
        f.write("\t".join(CHECK_COLS) + "\n")
        for c in out:
            f.write("\t".join(c[k] for k in CHECK_COLS) + "\n")
    print(f"wrote {args.out}: {len(out)} ranges; no check type for {len(missing)}")
    for r in missing:
        print("  no check type:", r["stream"], r["symbol"], r["c_type"])
    return 0


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("merge"); p.add_argument("--out", default=str(MODEL / "addresses.tsv"))
    sub.add_parser("overlaps")
    p = sub.add_parser("coverage"); p.add_argument("--json")
    p = sub.add_parser("header"); p.add_argument("--check", action="store_true")
    p = sub.add_parser("checkinput"); p.add_argument("--out", default=str(ROOT / "log/p1/g1/check_all.tsv"))
    p = sub.add_parser("hooks"); p.add_argument("--types", default=str(ROOT / "log/p1/g1/check_all.tsv"))
    p.add_argument("--out", default=str(ROOT / "log/p1/g1/hooks.tsv"))
    a = ap.parse_args()
    return {"merge": cmd_merge, "overlaps": cmd_overlaps, "coverage": cmd_coverage, "header": cmd_header,
            "checkinput": cmd_checkinput, "hooks": cmd_hooks}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
