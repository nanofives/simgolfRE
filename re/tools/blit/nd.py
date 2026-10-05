import sys,re,difflib,subprocess
def L(a):
    out=subprocess.run([sys.executable,'re/tools/asm2inline.py','jgld.dll',a,'--list'],capture_output=True,text=True).stdout
    r=[]
    for l in out.splitlines():
        ins=l.split('  ',1)[1].strip()
        ins=re.sub(r'^(j\w+|call) 0x[0-9a-f]+',r'\1 X',ins)
        r.append(ins)
    return r
a,b=L(sys.argv[1]),L(sys.argv[2])
for l in difflib.unified_diff(a,b,lineterm='',n=int(sys.argv[3]) if len(sys.argv)>3 else 2):
    print(l)
