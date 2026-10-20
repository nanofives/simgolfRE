"""Phase 3.7a: the hand edits of converted hook bodies (log/p3/p37a_conv.py --hook output) -> port/src/sim/tick_*.inc.

usage: py -3.12 log/p3/p37a_patch_scene.py <part> <raw.inc> <out.inc>
Each part lists exact replacements (asserted to occur `count` times) with the reason; nothing else is changed.
"""
import re
import sys

PARTS = {
    # buildSceneSprites (shim/src/re/c3fj.cpp)
    "scene": {
        "src": "shim/src/re/c3fj.cpp (buildSceneSprites)",
        "edits": [
            # the sprite scale feeds Surface_drawSprite only (a dropped drawing call): its .rdata double constants are not read
            ("    auto scale = [&]() { return static_cast<float>(static_cast<double>(*tbyte) * G<double>(CODE_4ba818)); };",
             "    auto scale = [&]() { return 0.0f; };  // *tbyte * [.rdata double]: only Surface_drawSprite's scale (dropped)", 1),
            ("        const float s = static_cast<float>(static_cast<double>(*tbyte) * G<double>(CODE_4ba480) + G<double>(CODE_4ba478));",
             "        const float s = 0.0f;  // *tbyte * [.rdata] + [.rdata]: only Surface_drawSprite's scale (dropped)", 1),
            # __itoa returns its buffer; strcat over the address space
            ("        VP<char> s = kItoa(*vp<int>(r2 + 0xc), vp<char>(GS(g_golferIdText)), 10);\n"
             "        strcat(vp<char>(GS(g_textBuffer)), s);",
             "        const u32 s = static_cast<u32>(kItoa(*vp<int>(r2 + 0xc), GS(g_golferIdText), 10));\n"
             "        StrCat(GS(g_textBuffer), s);", 1),
            # locals whose address a callee takes live on the address-space stack
            ("            int tx = kind, ty = variant;", "            LV tx(*this, kind), ty(*this, variant);", 1),
            ("        int px = x, py = y;", "        LV px(*this, x), py(*this, y);", 1),
            ("void BuildSceneSprites_re(int kind, int variant, int x, int y, int x2, int y2, int obj) {",
             "void buildSceneSprites(int kind, int variant, int x, int y, int x2, int y2, int obj) {", 1),
        ],
    },
    # drawFlyingBall (shim/src/re/c3fs.cpp)
    "ball": {
        "src": "shim/src/re/c3fs.cpp (drawFlyingBall)",
        "edits": [
            # CallOnFrame1 only seeded the A/B's stack slot: a plain call (the slot it seeds is F+0x14, zeroed here, U-0920)
            ("const unsigned kTypeBit7Clear = CODE_46c940;\n", "", 1),
            ("CallOnFrame1(F, kTypeBit7Clear, g)", 'cc("typeBit7Clear", g)', 1),
            # the text buffer helpers over the address space
            ("static VP<char> kText = vp<char>(GS(g_textBuffer));\n"
             "void CopyText(unsigned src) {\n"
             "    VP<char> s = vp<char>(src);\n"
             "    memcpy(kText, s, strlen(s) + 1);\n"
             "}\n"
             "void AppendText(const char* s) {\n"
             "    VP<char> d = kText + strlen(kText);\n"
             "    memcpy(d, s, strlen(s) + 1);\n"
             "}",
             "void CopyText(unsigned src) { StrCpy(kBuf, src); }\n"
             "void AppendText(u32 s) { StrCat(kBuf, s); }", 1),
            ("kText[0] = 0;", "GU8(kBuf) = 0;", 1),
            ("static_cast<int>(kText)", "static_cast<int>(kBuf)", 1),
            ("AppendText(kItoa(FI(0x14), vp<char>(GS(g_flyBall4e9a88)), 10));",
             "AppendText(static_cast<u32>(kItoa(FI(0x14), GS(g_flyBall4e9a88), 10)));", 1),
            ("static_cast<unsigned>(R)", "static_cast<u32>(R)", 1),
            # the original's frame (`sub esp, 0x24` + four pushes) on TkBase's stack, zeroed
            ("void DrawFlyingBallBody(unsigned char* F) {",
             "void drawFlyingBall() {\n    FrameSlots slots(*this, 0x34);\n    const u32 F = slots.va;", 1),
        ],
    },
    # updateGolfers (shim/src/re/c3ec.cpp)
    "ug": {
        "src": "shim/src/re/c3ec.cpp (updateGolfers)",
        "edits": [
            # the screen points of the drawing block are passed by address to worldToScreen / tileToScreen
            ("    int L54 = 0, L58 = 0, L5c = 0, L60 = 0;", "    LV L54(*this), L58(*this), L5c(*this), L60(*this);", 1),
            ("static VP<char> kText = vp<char>(GS(g_textBuffer));\n"
             "// strcat as the `repne scasb` / `rep movsd` / `rep movsb` sequences do it (the NUL is copied)\n"
             "void Append(const char* src) {\n"
             "    VP<char> d = kText;\n"
             "    while (*d) d++;\n"
             "    while ((*d++ = *src++) != 0) {}\n"
             "}\n"
             "void AppendAt(unsigned src) { Append(vp<char>(src)); }",
             "// strcat as the `repne scasb` / `rep movsd` / `rep movsb` sequences do it (the NUL is copied)\n"
             "void AppendAt(unsigned src) { StrCat(kBuf, src); }", 1),
            ("Append(kItoa(hole, vp<char>(GS(g_textBuf)), 10));", "AppendAt(static_cast<u32>(kItoa(hole, GS(g_textBuf), 10)));", 1),
            ("Append(kItoa(GC(i, (GS(g_golfers) + 33)), vp<char>(GS(g_textBuf)), 10));",
             "AppendAt(static_cast<u32>(kItoa(GC(i, (GS(g_golfers) + 33)), GS(g_textBuf), 10)));", 1),
            ("unsigned& GameFlags() {", "Ref<unsigned> GameFlags() {", 1),
            ("int UpdateGolfers_re() {", "int updateGolfers() {", 1),
        ],
    },
    # showTournamentResults (shim/src/re/c3eo.cpp)
    "str": {
        "src": "shim/src/re/c3eo.cpp (showTournamentResults)",
        "edits": [
            ("void ShowTournamentResults_re() {",
             "// the file-scope pointers of c3eo.cpp the body uses (the text buffer, the itoa scratch, the root window's surface)\n"
             "VP<char> kText = vp<char>(GS(g_textBuffer));\n"
             "u32 kItoaBuf = GS(g_ui_821c50);\n"
             "u32 kScreen = GS(g_rootWindow) + 0x274;\n"
             "u32 kRootWindow = GS(g_rootWindow);\n"
             "void showTournamentResults() {", 1),
            # the scorecard layout's close button is a modal wait for a click: its pumpMessages is a Blocking pump of the
            # context's InputSource (tick.cpp lc_pump_messages at the modal site; NeedInput when no input is left)
            ("        int px = 0, py = 0;", "        LV px(*this), py(*this);", 1),
        ],
    },
    # matchUpdate (shim/src/re/c3aj.cpp)
    "mu": {
        "src": "shim/src/re/c3aj.cpp (matchUpdate)",
        "edits": [
            # the hook header's malformed address of the walker sweep (its loop: 0x4c records from g_walkers+0x12)
            ("the 0x0058585862 record sweep", "the walker record sweep g_walkers+0x12 (stride 0x4c, matchUpdate+0x3b6)", 1),
            ("template <typename T> T& ref(unsigned addr) { return *reinterpret_cast<T*>(addr); }",
             "template <class T> Ref<T> ref(u32 addr) { return G<T>(addr); }", 1),
            ("void appendText(const char* src) {\n"
             "    VP<char> dst = kText;\n"
             "    while (*dst) ++dst;\n"
             "    do { *dst++ = *src; } while (*src++);\n"
             "}\n"
             "void appendAt(unsigned strAddr) { appendText(vp<char>(strAddr)); }\n"
             "void setTextAt(unsigned strAddr) { strcpy(kText, vp<char>(strAddr)); }",
             "void appendText(u32 src) { StrCat(kBuf, src); }\n"
             "void appendAt(unsigned strAddr) { appendText(strAddr); }\n"
             "void setTextAt(unsigned strAddr) { StrCpy(kBuf, strAddr); }", 1),
            ("void MatchUpdate_re(int golfer) {", "void matchUpdate(int golfer) {", 1),
        ],
    },
    # membershipReport (shim/src/re/c3ah.cpp; addScore there is hand-written in tick_leaves.inc, clearObjectFootprint is
    # phase 3.1a's, so both bodies are cut)
    "ah": {
        "src": "shim/src/re/c3ah.cpp (membershipReport)",
        "cut": [("struct ScoreRow {", "// --------------------------------------------------------------------- membershipReport")],
        "edits": [
            ("template <typename T> T& ref(unsigned addr) { return *reinterpret_cast<T*>(addr); }",
             "template <class T> Ref<T> ref(u32 addr) { return G<T>(addr); }", 1),
            ("    unsigned char& tierByte = ref<unsigned char>(", "    Ref<unsigned char> tierByte = ref<unsigned char>(", 1),
            ("ref<const char*>(", "ref<u32>(", 4),
            ("void MembershipReport_re(unsigned golfer) {", "void membershipReport(unsigned golfer) {", 1),
        ],
    },
    # drawBuildingPanel (shim/src/re/c3ei.cpp)
    # drawRecords (shim/src/re/c3ei.cpp): P1's per-tile record sprites; its sprite queue entries, cell ids (g_animalCell) and
    # the animation bytes of the record banks are simulation-class, the Image_draw blits are dropped
    "drec": {
        "src": "shim/src/re/c3ei.cpp (drawRecords)",
        "edits": [
            ("void DrawRecords_re(int p1, int p2, int p3) {\n    int sx = 0, sy = 0;",
             "void drawRecords(int p1, int p2, int p3) {\n    LV sx(*this), sy(*this);   // worldToScreen's two out slots (the "
             "original reuses the p2 / p3 argument slots)", 1),
            ("static_cast<int>(&sx), static_cast<int>(&sy)", "static_cast<int>(&sx), static_cast<int>(&sy)", 2),
            # the hook's start values of the two registers the rotation switch leaves stale for rotations 1, 3, 5, > 6 are
            # not the original's (its fixtures keep to 0, 2, 4, 6): such a rotation stops the run instead
            ("                int rot = G<int>(GS(g_cameraRot));                                // drawRecords+0x124\n",
             "                int rot = G<int>(GS(g_cameraRot));                                // drawRecords+0x124\n"
             "                if (rot != 0 && rot != 2 && rot != 4 && rot != 6)\n"
             "                    throw MissingResource{\"drawRecords: an odd camera rotation (stale registers)\"};\n", 1),
        ],
    },
    "dbp": {
        "src": "shim/src/re/c3ei.cpp (drawBuildingPanel)",
        "edits": [
            ("int DrawBuildingPanel_re(int a, int b) {", "int drawBuildingPanel(int a, int b) {", 1),
        ],
    },
}


def main():
    part, raw, out = sys.argv[1], sys.argv[2], sys.argv[3]
    spec = PARTS[part]
    text = open(raw, encoding="utf8").read()
    for a, b in spec.get("cut", []):  # remove [a, b): bodies another module owns
        i, j = text.index(a), text.index(b)
        text = text[:i] + "// (cut: a body this module does not take)\n" + text[j:]
    for old, new, n in spec["edits"]:
        got = text.count(old)
        if got != n:
            sys.exit("%s: edit expected %d, found %d: %s" % (part, n, got, old[:90]))
        text = text.replace(old, new)
    code = "\n".join(ln.split("//")[0] for ln in text.splitlines())
    left = re.findall(r"CODE_[0-9a-f]+|WINAPI|__asm|reinterpret_cast", code)
    if left:
        sys.exit("left: %s" % sorted(set(left)))
    header = ("// GENERATED by log/p3/p37a_conv.py --hook from %s and edited by log/p3/p37a_patch_scene.py %s.\n"
              "// Included inside a TkBase-derived struct (tick.cpp). Citations: <function>+0x<off>, <object>+0x<off>, ML+0x<off>.\n"
              % (spec["src"], part))
    open(out, "w", encoding="utf8", newline="\n").write(header + text)


main()
