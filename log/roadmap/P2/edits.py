"""P2 anchored edits: apply to a clean `git archive HEAD` tree.

    ~/bin/py -3.12 log/roadmap/P2/edits.py <tree root>

gditext.cpp: a fresh temporary directory every time, process-private font registration (FR_PRIVATE), negctl mode 8 = fix off.
Also writes port/test/test_gditext_tempdir.py from log/roadmap/P2/test_gditext_tempdir.py.
"""
import pathlib
import shutil
import sys

root = pathlib.Path(sys.argv[1])
p = root / "port/src/sim/gditext.cpp"
s = p.read_bytes().decode("utf-8")
crlf = "\r\n" in s
s = s.replace("\r\n", "\n")


def rep(a, b):
    global s
    assert s.count(a) == 1, a
    s = s.replace(a, b)


rep("//   7 the TextMetrics interface call ignores its font argument (the context's own +0x5c is measured)\n",
    "//   7 the TextMetrics interface call ignores its font argument (the context's own +0x5c is measured)\n"
    "//   8 P2 fix off: an existing temporary directory is accepted and the fonts are registered session-wide (the old\n"
    "//     behaviour; read RED by port/test/test_gditext_tempdir.py)\n")
rep("""      static long counter = 0;
      char name[MAX_PATH + 64];
      std::snprintf(name, sizeof(name), "%ssg_gditext_%lu_%ld", base, static_cast<unsigned long>(GetCurrentProcessId()),
                    InterlockedIncrement(&counter));
      if (!CreateDirectoryA(name, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        throw MissingResource{"text font file"};
      temp_dir = name;
""", """      static long counter = 0;
      char name[MAX_PATH + 64];
      // P2 (2026-10-31): a FRESH directory every time. A process killed before ~Impl leaves its .ttf locked by the
      // session; a later process that Windows gives the same pid would pick the same name and fail to overwrite the
      // locked file. An existing directory is therefore never accepted: the name gets a tick-count suffix after the
      // first clash, and the search gives up after a bounded number of tries.
      for (int tries = 0; tries < 64; ++tries) {
        const long seq = InterlockedIncrement(&counter);
        if (tries == 0)
          std::snprintf(name, sizeof(name), "%ssg_gditext_%lu_%ld", base,
                        static_cast<unsigned long>(GetCurrentProcessId()), seq);
        else
          std::snprintf(name, sizeof(name), "%ssg_gditext_%lu_%ld_%llx", base,
                        static_cast<unsigned long>(GetCurrentProcessId()), seq,
                        static_cast<unsigned long long>(GetTickCount64()) + static_cast<unsigned>(tries));
        if (CreateDirectoryA(name, nullptr)) {
          temp_dir = name;
          break;
        }
        const DWORD err = GetLastError();
        if (err == ERROR_ALREADY_EXISTS && SG_NEGCTL(g_negctl_gditext) == 8) {  // fix off: the old acceptance
          temp_dir = name;
          break;
        }
        if (err != ERROR_ALREADY_EXISTS) throw MissingResource{"text font file"};
      }
      if (temp_dir.empty()) throw MissingResource{"text font file"};
""")
rep("""    if (!AddFontResourceA(fot.c_str())) throw MissingResource{"text font file"};
    fots[key] = fot;""", """    // The original registers session-wide (Font::loadFile: AddFontResourceA). The port's provider is a host service, so
    // the registration is process-private (FR_PRIVATE, a port choice): it ends with the process even when it is killed
    // and leaves no locked font file behind. CreateFontIndirectA still resolves the faces by name inside the process.
    if (SG_NEGCTL(g_negctl_gditext) == 8) {
      if (!AddFontResourceA(fot.c_str())) throw MissingResource{"text font file"};
    } else if (!AddFontResourceExA(fot.c_str(), FR_PRIVATE, nullptr)) {
      throw MissingResource{"text font file"};
    }
    fots[key] = fot;""")
rep("    for (auto &f : fots) RemoveFontResourceA(f.second.c_str());",
    """    for (auto &f : fots) {
      if (SG_NEGCTL(g_negctl_gditext) == 8) RemoveFontResourceA(f.second.c_str());
      else RemoveFontResourceExA(f.second.c_str(), FR_PRIVATE, nullptr);
    }""")
if crlf:
    s = s.replace("\n", "\r\n")
p.write_bytes(s.encode("utf-8"))

t = pathlib.Path(__file__).with_name("test_gditext_tempdir.py")
if t.exists():
    shutil.copyfile(t, root / "port/test/test_gditext_tempdir.py")
print("P2 edits applied")
