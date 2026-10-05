"""Derive the 8-bit-destination skeleton (body8.cpp) from the 16-bit one (body.cpp): no palette, a lookup table
parameter kept in 0x1012862c, byte destination through slot 6, and the family's own globals (mapping found by
aligning the C parts of 0x1002ed60 and 0x10063f60)."""
import re
t = open('re/tools/blit/body.cpp').read()
m = {'g_10122e1c': 'g_10122e54', 'g_10122e20': 'g_10122e58', 'g_1012853c': 'g_1012861c', 'g_10128540': 'g_10128624',
     'g_10128554': 'g_1012863c', 'g_10128558': 'g_10128640', 'g_1012855c': 'g_10128644', 'g_10128560': 'g_10128648',
     'g_10128564': 'g_10128650', 'g_10128570': 'g_10128660', 'g_10128578': 'g_10128670', 'g_1012857c': 'g_10128678',
     'g_10128584': 'g_10128680', 'g_10128588': 'g_10128688', 'g_1012858c': 'g_1012868c', 'g_10128590': 'g_10128690'}
t = re.sub(r'\bg_[0-9a-f]{8}\b', lambda x: m.get(x.group(0), x.group(0)), t)
t = t.replace('    unsigned short* d;\n', '    unsigned char* d;\n')
t = t.replace('    Pal16* pal;\n', '')
t = t.replace('''    if (m_palette)
        pal = m_palette;
    else
        pal = g_palClient1->m_palette;
    if (pal == 0) {
        dst->unlock(1);
        return 16;
    }
''', '')
t = t.replace('''    s = m_bits;
    switch (dst->format()[1]) {
    case 0:
        g_1012856c = pal->table565();
        break;
    case 1:
        g_1012856c = pal->table555();
        break;
    default:
        dst->unlock(1);
        return 1;
    }
''', '''    s = m_bits;
    g_1012862c = table;
''')
t = t.replace('(unsigned short*)dst->bits() + rc.left + rc.top * g_10128644', 'dst->bits8() + rc.left + rc.top * g_10128644')
t = t.replace('g_10128648 = (g_10128644 - g_10128650) * 2;', 'g_10128648 = g_10128644 - g_10128650;')
open('re/tools/blit/body8.cpp', 'w').write(t)
print(t.count('bits8'), t.count('g_10128648 = g_10128644 - g_10128650;'), 'pal' in t)
