"""mkrlem.py <kind> <pad> <note>: fill bodyrlem.cpp's INNER/SETUP/PAD1 for one masked run-length variant."""
import sys
kind, pad, note = sys.argv[1], int(sys.argv[2]), sys.argv[3]
t = open('re/tools/blit/bodyrlem.cpp').read()
I = ' ' * 24
inner = {
    'l': I + 'if (c < 0xfe && m[dx] > level && c != 0xfe)\n' + I + '    d[dx] = g_10128600[c];',
    'k': I + 'if (c != 0xff) {\n' + I + '    if (m[dx] > level) {\n' + I + '        if (c != 0xfe)\n' + I + '            d[dx] = g_10128600[c];\n'
         + I + '    } else if (c == 0xfe)\n' + I + '        d[dx] = color;\n' + I + '}',
}[kind]
setup = {'l': '', 'k': ''}[kind]
t = t.replace('INNER', inner).replace('SETUP\n', setup).replace('PAD1\n', '\n' * pad).replace('PADNOTE', note)
open('re/tools/blit/bodyrlem%s.cpp' % kind, 'w').write(t)
