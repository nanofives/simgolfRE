import sys
sys.path.insert(0, "port/build/S3_30/re/tools")
import trace_format as TF
old = TF.parse(open("log/p2/p24/golfer_events/trace_rec.trace","rb").read(), decode_fields=False)
new = TF.parse(open("log/roadmap/S3/30_work/golfer_events.rec2.trace","rb").read(), decode_fields=False)
print("records", len(old.records), len(new.records), "meta same", old.meta == new.meta)
if old.meta != new.meta:
    for k in set(old.meta)|set(new.meta):
        if old.meta.get(k) != new.meta.get(k): print(" meta", k, str(old.meta.get(k))[:200], "|", str(new.meta.get(k))[:200])
print("header", (old.step,old.t0,old.qstep,old.q0), (new.step,new.t0,new.qstep,new.q0))
nd = 0
for i,(a,b) in enumerate(zip(old.records,new.records)):
    if (a.type,a.sub,a.payload)!=(b.type,b.sub,b.payload):
        nd += 1
        if nd <= 5:
            print(i, a.name, a.sub, a.payload.hex(), "|", b.name, b.sub, b.payload.hex())
print("differing records", nd)
c = [ (i,TF.decode(r)) for i,r in enumerate(new.records) if r.type==TF.DELIVER and r.sub==7]
print(c)
print("--- with new offset by 2 after record 0")
nr = new.records
nd = 0
for i,(a,b) in enumerate(zip(old.records,nr)):
    if (a.type,a.sub,a.payload)!=(b.type,b.sub,b.payload):
        nd += 1
        if nd <= 6: print(i, a.name, a.sub, a.payload.hex(), "|", b.name, b.sub, b.payload.hex())
print("differing", nd, len(old.records), len(nr))
mm = [(i, TF.decode(r)) for i, r in enumerate(new.records) if r.type == TF.MSG and TF.decode(r)["msg"] == 0x200][:5]
print("mousemoves new", sum(1 for r in new.records if r.type==TF.MSG and TF.decode(r)["msg"]==0x200), "old", sum(1 for r in old.records if r.type==TF.MSG and TF.decode(r)["msg"]==0x200), mm)
