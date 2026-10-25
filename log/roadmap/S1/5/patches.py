"""S1.5 shared-file edits (log/roadmap/S1/5_merge.md), applied to the mirrored copies by mirror.py.

Each entry: (relative path under port/, [(old, new), ...]); every `old` must occur exactly once in the file at HEAD."""

TICK_PARSE_VARS_OLD = "  bool after_mousemove = false;\n  while (p + 4 <= n) {\n"
TICK_PARSE_VARS_NEW = """  bool after_mousemove = false;
  // KEYS THE WINDOW TREE'S FOCUS CHILD TOOK (roadmap S1.5, U-1032; log/roadmap/S1/5_notes.md). A WM_KEYDOWN reaches the
  // root's onChar (HANDLER 8; a key with no character, MapVirtualKeyA 0, the root's onKey with a0 = 0: HANDLER 7) and a
  // WM_CHAR the root's onKey (HANDLER 7) only when no focused child window returns non-zero for it first (uiKeyEvent ->
  // onCommand -> dispatchAccel, uiKeyEvent148 -> Window::onKeyChar -> Window::key, which ask the root's focus ring before
  // the root's own slot). A key MSG record that no HANDLER record follows before the next MSG record, clock read or frame
  // head was taken by such a child (the character editor's bio EditBox: EditBox::onKey, EditBox::insertChar): it becomes a
  // RootHandler command 12 (WM_KEYDOWN, a0 = the virtual key) or 13 (WM_CHAR, a0 = the character), which apply_command
  // treats as nothing and the bio entry routes to its EditBox (chareditor.cpp). Such commands only join a batch some
  // HANDLER record opens (the open one, or the next one of the same frame), so no batch is added. In the 64 corpus traces
  // every one of the 11,312 WM_KEYDOWN / WM_CHAR MSG records is followed by a root handler, so none of them gets one.
  struct PendKey {
    bool on = false;
    uint32_t msg = 0, wp = 0;
  } pend_key;
  std::vector<Command> focus_held;
  auto focus_resolve = [&]() {  // the pending key MSG record got no matching root handler
    if (!pend_key.on) return;
    Command c;
    c.kind = CmdKind::RootHandler;
    c.handler = pend_key.msg == 0x100 ? 12u : 13u;
    c.a0 = static_cast<int32_t>(pend_key.wp);
    if (SG_NEGCTL(tk::g_negctl_frame) != 8) focus_held.push_back(c);  // negative control 8: the commands dropped
    pend_key.on = false;
  };
  auto focus_flush = [&]() {  // into the open batch, after the handlers it holds so far
    if (open_batch && cur && !focus_held.empty()) {
      cur->cmds.insert(cur->cmds.end(), focus_held.begin(), focus_held.end());
      focus_held.clear();
    }
  };
  while (p + 4 <= n) {
"""

TICK_CLOCK_OLD = """      } else {
        ctx_ra = ra;
        open_batch = false;
      }
    } else if (type == 0x08 && sub == 1) {"""
TICK_CLOCK_NEW = """      } else {
        focus_resolve();
        focus_flush();
        ctx_ra = ra;
        open_batch = false;
      }
    } else if (type == 0x05 && len >= 16) {  // MSG: pos, msg, wParam, lParam (the keys the focus child may take, above)
      focus_resolve();
      focus_flush();
      const uint32_t msg = rd32(q + 4);
      if (msg == 0x100 || msg == 0x102) pend_key = {true, msg, rd32(q + 8)};
    } else if (type == 0x08 && sub == 1) {"""

TICK_HANDLER_OLD = """    } else if (type == 0x09) {  // HANDLER (id = sub): pos, a0, a1
      if (!frames.empty()) {
        TraceFrame &f = frames.back();"""
TICK_HANDLER_NEW = """    } else if (type == 0x09) {  // HANDLER (id = sub): pos, a0, a1
      pend_key.on = false;  // the key reached a root handler
      if (!frames.empty()) {
        TraceFrame &f = frames.back();"""

TICK_HANDLER2_OLD = """        cur = &f.batches.back();
        Command c;
        c.kind = CmdKind::RootHandler;
        c.handler = sub;"""
TICK_HANDLER2_NEW = """        cur = &f.batches.back();
        focus_flush();
        Command c;
        c.kind = CmdKind::RootHandler;
        c.handler = sub;"""

TICK_FRAME_OLD = """    } else if (type == 0x02) {  // FRAME: frame, pos, t, snapshot
      TraceFrame f;"""
TICK_FRAME_NEW = """    } else if (type == 0x02) {  // FRAME: frame, pos, t, snapshot
      focus_resolve();
      focus_flush();
      focus_held.clear();  // a taken key with no batch of its frame to join (none in the corpus)
      TraceFrame f;"""

PATCHES = {
    "src/sim/tick.cpp": [(TICK_PARSE_VARS_OLD, TICK_PARSE_VARS_NEW), (TICK_CLOCK_OLD, TICK_CLOCK_NEW),
                         (TICK_HANDLER_OLD, TICK_HANDLER_NEW), (TICK_HANDLER2_OLD, TICK_HANDLER2_NEW),
                         (TICK_FRAME_OLD, TICK_FRAME_NEW)],
}


def names():
    return sorted(PATCHES)


def apply(rel: str, data: bytes) -> bytes:
    if rel not in PATCHES:
        return data
    s = data.decode("utf-8")
    nl = "\r\n" if "\r\n" in s else "\n"
    s = s.replace("\r\n", "\n")
    for old, new in PATCHES[rel]:
        if s.count(old) != 1:
            raise SystemExit(f"patch for {rel}: expected one occurrence of {old[:60]!r}, found {s.count(old)}")
        s = s.replace(old, new)
    return s.replace("\n", nl).encode("utf-8")
