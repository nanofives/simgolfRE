// For each address range, list the functions whose instructions reference it (read/write/data), with counts.
// Script args: <out.tsv> <va_hex>:<size_hex> ...
// TSV: va, size, refs, reads, writes, functions ("name@entry xN" by count, all of them)
//@category Analysis
import java.io.*;
import java.util.*;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.*;

public class RefsByRange extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		ReferenceManager rm = currentProgram.getReferenceManager();
		try (PrintWriter w = new PrintWriter(new FileWriter(args[0]))) {
			w.println("va\tsize\trefs\treads\twrites\tfunctions");
			for (int i = 1; i < args.length; i++) {
				String[] p = args[i].split(":");
				Address start = toAddr(Long.parseLong(p[0], 16));
				long size = Long.parseLong(p[1], 16);
				AddressSet set = new AddressSet(start, start.add(size - 1));
				Map<String, Integer> fns = new HashMap<>();
				int refs = 0, reads = 0, writes = 0;
				AddressIterator it = rm.getReferenceDestinationIterator(set, true);
				while (it.hasNext()) {
					Address dst = it.next();
					for (Reference r : rm.getReferencesTo(dst)) {
						refs++;
						if (r.getReferenceType().isRead()) reads++;
						if (r.getReferenceType().isWrite()) writes++;
						Function f = getFunctionContaining(r.getFromAddress());
						String k = f == null ? "nofunc@" + r.getFromAddress() : f.getName() + "@" + f.getEntryPoint();
						fns.merge(k, 1, Integer::sum);
					}
				}
				List<Map.Entry<String, Integer>> l = new ArrayList<>(fns.entrySet());
				l.sort((a, b) -> b.getValue() - a.getValue());
				StringBuilder sb = new StringBuilder();
				for (Map.Entry<String, Integer> e : l) sb.append(e.getKey()).append(" x").append(e.getValue()).append("; ");
				w.printf("%08X\t%X\t%d\t%d\t%d\t%s%n", start.getOffset(), size, refs, reads, writes, sb);
			}
		}
		println("REFSBYRANGE done");
	}
}
