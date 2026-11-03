import sys, struct
sys.path.insert(0, "re/tools")
import trace_format as TF
data = open("log/p2/p24/golfer_events/trace_rec.trace","rb").read()
tr = TF.parse(data, decode_fields=False)
print("records", len(tr.records), "meta keys", list(tr.meta))
for i, r in enumerate(tr.records):
    if r.type == TF.DELIVER and r.sub not in TF.DELIVER_FMT:
        print(i, hex(r.offset), r.sub, r.payload.hex())
        for j in range(max(0,i-4), i+6):
            rr = tr.records[j]
            try: f = TF.decode(rr)
            except Exception as e: f = str(e)
            print("  ", j, rr.name, rr.sub, f if rr.type != TF.FRAME else {k:v for k,v in f.items() if k!='snap'})
