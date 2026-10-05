"""Generate a jgld sprite blitter from the shared C template plus the function's own 8 inline-asm blocks.
    py -3.12 re/tools/blit/gen.py <addr> <name> [<addr> <name> ...] > out.cpp"""
import sys, subprocess, re
sys.path.insert(0, 're/tools')

def listing(addr):
    out = subprocess.run([sys.executable, 're/tools/asm2inline.py', 'jgld.dll', addr, '--list'], capture_output=True, text=True).stdout
    return [(l.split('  ', 1)[0], l.split('  ', 1)[1].strip()) for l in out.splitlines()]

def blocks(addr, n):
    L = listing(addr)
    st = [k for k in range(len(L) - 1) if L[k][1] == 'push esi' and L[k + 1][1] == 'push edi' and k > 10]
    en = [k + 1 for k in range(len(L) - 1) if L[k][1] == 'pop edi' and L[k + 1][1] == 'pop esi' and k < len(L) - 10]
    assert len(st) == len(en) == n, (addr, len(st), len(en), n)
    res = []
    for s, e in zip(st, en):
        o = subprocess.run([sys.executable, 're/tools/asm2inline.py', 'jgld.dll', addr, '--from', L[s][0], '--to', L[e][0]],
                           capture_output=True, text=True).stdout
        res.append(o.split('    // globals')[0].rstrip('\n'))
    return res

HEAD = open('re/tools/blit/head.cpp').read()
import os
BODY = open('re/tools/blit/' + os.environ.get('TPL', 'body') + '.cpp').read()
# a template may start with its own global declarations, separated from the function by a '// ----' line
SEP = '// ----' + chr(10)
DECL, BODY = BODY.split(SEP, 1) if SEP in BODY else ('', BODY)
NBLK = len(set(re.findall(r'^    ASM(\d+)$', BODY, re.M)))     # the template's ASM1..ASMn placeholders
args = sys.argv[1:]
names=[args[i+1] for i in range(0,len(args),2)]
bodies = []
sigs = []
for i in range(0, len(args), 2):
    addr, name = args[i], args[i + 1]
    b = blocks(addr, NBLK)
    last = listing(addr)[-1][1]
    # stack parameters from `ret N`: (dst, x, y), + char flag (0x10), + two ints read only by the asm (0x14)
    extra = {'ret 0xc': ('', ''), 'ret 0x10': (', char flag', 'D'), 'ret 0x14': (', int p4, int p5', 'HH')}.get(last)
    if os.environ.get('TPL') == 'body8':      # 8-bit destination: the 4th parameter is the blend table
        extra = {'ret 0x10': (', unsigned char* table', 'PAE')}[last]
    if os.environ.get('TPL') == 'body8b':     # 8-bit destination: the 4th parameter is a colour byte
        extra = {'ret 0x10': (', unsigned char color', 'E')}[last]
    if os.environ.get('TPL') in ('body2', 'body3', 'body2n'):      # family 2: the 4th parameter is an int kept in g_10128548
        extra = {'ret 0x10': (', int p4', 'H'), 'ret 0x14': (', int p4, int p5', 'HH')}[last]
    if os.environ.get('TPL') == 'body8n':     # 8-bit destination, no 4th parameter
        extra = {'ret 0xc': ('', '')}[last]
    if os.environ.get('TPL') == 'body8p':     # 8-bit destination: blend table + an int kept in g_1012866c
        extra = {'ret 0x14': (', unsigned char* table, int p5', 'PAEH')}[last]
    if os.environ.get('TPL') == 'bodyc':      # 16-bit destination: the 4th parameter is a colour (negative = raw 16-bit pixel, else palette index)
        extra = {'ret 0x10': (', int c', 'H')}[last]
    if os.environ.get('SIG'):                 # explicit '<C parameters>|<decorated suffix>' for a new skeleton
        extra = tuple(os.environ['SIG'].split('|'))
    if os.environ.get('SIGFULL'):             # whole parameter list '<C parameters>|<decorated parameters>' (template writes FPARAMS/FDECOR)
        extra = ('FULL:' + os.environ['SIGFULL'].split('|')[0], os.environ['SIGFULL'].split('|')[1])
    sigs.append((name, extra[0]))
    t = BODY.replace('NAME', name).replace('ADDR', f'0x{int(addr, 16):08x}')
    if extra[0].startswith('FULL:'):
        t = t.replace('FDECOR', extra[1]).replace('FPARAMS', extra[0][5:])
    t = t.replace('DECOR', extra[1]).replace('PARAMS', extra[0])
    for k, blk in enumerate(b, 1):
        t = t.replace(f'    ASM{k}\n', blk + '\n')
    bodies.append(t)
decl = ''.join(('    int %s(%s);' % (n, f[5:]) if f.startswith('FULL:') else '    int %s(Surface* dst, int x, int y%s);' % (n, f)) + chr(10) for n, f in sigs)
head = HEAD.replace('METHODS' + chr(10), decl).replace('class Sprite {', DECL + 'class Sprite {', 1)
# globals the asm blocks name that the skeleton does not declare (mid-variable addresses such as the high
# half of a 16.16 value): declared here so the inline assembly resolves them
used = sorted(set(re.findall(r'\bg_[0-9a-f]{8}\b', ''.join(bodies))))
extra = [g for g in used if not re.search(r'\b' + g + r'\b', head)]
head = head.replace('class Sprite {', ''.join('extern int %s;' % g + chr(10) for g in extra) + 'class Sprite {', 1)
print(head)
print(''.join(bodies))
