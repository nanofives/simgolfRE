"""Derive the run-length variants from bodyrle2.cpp: inner store and palette setup differ; padding recalibrated."""
import sys
t = open('re/tools/blit/bodyrle2.cpp').read()
kind, pad1, pad2 = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
def inner(idx):
    old = """                        if (c != 0xff) {
                            if (c >= 0xf8)
                                d[%s] = g_101285ec[d[%s]];
                            else
                                d[%s] = g_10128600[g_101285c8[c]];
                        }
""" % (idx, idx, idx)
    assert old in t, idx
    return old
for idx in ('dx', 'j'):
    o = inner(idx)
    if kind == 'c':
        n = "                        if (c != 0xff)\n                            d[%s] = g_101285a8;\n" % idx
    elif kind == 't':
        n = """                        if (c != 0xff) {
                            if (c >= 0xf8)
                                d[%s] = g_101285ec[d[%s]];
                            else
                                d[%s] = g_10128600[c];
                        }
""" % (idx, idx, idx)
    elif kind == 'p':
        n = "                        if (c < 0xfe)\n                            d[%s] = g_101285c8[g_10128600[c]];\n" % idx
    else:
        n = "                        if (c < 0xfe)\n                            d[%s] = g_10128600[c];\n" % idx
    t = t.replace(o, n)
t = t.replace("""    g_101285ec = p5;
    g_101285c8 = p4;
""", "    g_101285ec = p4;\n" if kind == 't' else "    g_101285c8 = p4;\n" if kind == 'p' else "")
if kind == 'c':
    t = t.replace("g_10128600 = pal->table565();", "g_101285a8 = pal->table565()[color];")
    t = t.replace("g_10128600 = pal->table555();", "g_101285a8 = pal->table555()[color];")
    t = t.replace("extern unsigned short* g_10128600;           // palette table\n", "extern unsigned short g_101285a8;            // colour: palette entry of the 4th parameter\n")
if kind != 't': t = t.replace("extern unsigned short* g_101285ec;           // 5th parameter: table indexed by the destination pixel (codes >= 0xf8)\n", "")
t = t.replace("extern unsigned char* g_101285c8;            // 4th parameter: byte table indexed by the source code, then the palette\n",
              "extern unsigned short* g_101285c8;           // 4th parameter: table indexed by the palette entry\n" if kind == 'p' else "")
# padding
lines = t.split('\n')
k = [i for i, l in enumerate(lines) if l.startswith('    // original reports sit')][0]
j = k + 1
while lines[j] == '':
    j += 1
lines = lines[:k + 1] + [''] * pad1 + lines[j:]
t = '\n'.join(lines)
t = t.replace("    // original reports sit at base+0x5c/0x60/0x62 (unscaled) and base+0xa9/0xad/0xaf (scaled)",
              "    // original reports sit at base+" + (sys.argv[4] if len(sys.argv) > 4 else "0x55/0x59/0x5b (unscaled) and base+0x9d/0xa1/0xa3 (scaled)"))
b = "        g_101285c0 = (g_10122dc8 << 16) / abs(g_10122dc4);\n"
i = t.index(b) + len(b)
e = i
while t[e] == '\n':
    e += 1
t = t[:i] + '\n' * pad2 + t[e:]
open('re/tools/blit/bodyrle%s.cpp' % kind, 'w').write(t)
