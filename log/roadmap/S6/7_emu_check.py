import sys
sys.path.insert(0, r"log/roadmap/S6/6_probes")
import plib as P
import port.test.test_defects_golfers_w2 as W
from port.test import lib as L
from port.test.test_golfers import VL
lib=L.load()
for n,o,s in lib.objects(): W._OFF[n]=o
HOLES,PURSE,CASH,PRO,DIFF,RNG=0x575ab0,0x567b04,0x571fd4,0x58dd50,0x822c88,0x822d9c
def emu_state(d,h,p):
    im=P.Img(P.zero_image())
    for x in h: im.w(HOLES+0x208*x,1,1)
    im.w(DIFF,d&0xffffffff,4); im.w(RNG,777,4); im.w(PURSE,p,4)
    for k in range(100):
        ok=k in W.FULL
        im.w(PRO+k*0x38+0x20,0 if ok else 0xff,1); im.w(PRO+k*0x38+0x36,10 if ok else 0,2)
    return im.done()
def port_img(d,h,p,defect):
    lib.defect(W.SWITCH,defect); lib.negctl("loadGolfPro",0)
    with lib.state() as st:
        st.set_data_base(VL.DATA_VA); W._synthetic(d,W.FULL,h,p)(st)
        try: r=("ok",st.call("loadGolfPro",[0]).eax)
        except L.SgError as e: r=e.code
        return r, st.read(0,len(P.zero_image()))
bad=0
for d in (5,6,2,-1):
  for h in [(1,),(2,),(1,2,3)]:
    for p in (0,50,5000):
      data=emu_state(d,h,p)
      r=P.emu(0x46c970,[],data,max_insns=3_000_000)
      pr,img=port_img(d,h,p,1)
      if r.status=="completed":
          b=bytearray(data)
          for va,(o,n) in r.writes.items(): b[va-0x4c1000]=n
          same = pr==("ok",r.eax) and bytes(b)==img
          if not same:
              diff=[i for i in range(len(img)) if img[i]!=b[i]][:5]
              print("  diff", [hex(i+0x4c1000) for i in diff], pr, hex(r.eax))
      else:
          same = pr==L.SG_E_NO_RETURN
      bad+= not same
      print(d,h,p,"orig",r.status,"port",pr if not isinstance(pr,tuple) else pr[0],"SAME" if same else "DIFF")
print("bad",bad)
