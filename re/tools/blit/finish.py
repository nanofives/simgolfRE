"""finish.py <in.cpp> <out.cpp> <comment line 1> <comment line 2>: replace the head comment block, keep the rest."""
import sys
t = open(sys.argv[1]).read().split('\n')
k = 0
while t[k].startswith('//'):
    k += 1
out = ['// FLAGS jgld.dll: /Od /ZI /GZ', '// ' + sys.argv[3], '// ' + sys.argv[4]] + t[k:]
open(sys.argv[2], 'w').write('\n'.join(out))
