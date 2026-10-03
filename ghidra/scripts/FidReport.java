// Report Function ID matches for the current program against the installed FidDbs plus one extra
// FidDb file, WITHOUT applying them. Run read-only on a pool slot (re/tools/fid_report.py).
// Script args: <extra.fidb> <out.tsv>
// TSV: entry, current name, size, best score, unique (1 = one distinct name among best matches),
//      names (;-joined), library family|version|variant of the best match.
//@category FunctionID
import java.io.*;
import java.util.*;

import ghidra.app.script.GhidraScript;
import ghidra.feature.fid.db.*;
import ghidra.feature.fid.service.*;

public class FidReport extends GhidraScript {

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		FidFileManager fm = FidFileManager.getInstance();
		FidFile extra = fm.addUserFidFile(new File(args[0]));
		FidService service = new FidService();
		try (FidQueryService q = service.openFidQueryService(currentProgram.getLanguage(), false);
				PrintWriter w = new PrintWriter(new FileWriter(args[1]))) {
			w.println("entry\tname\tsize\tscore\tunique\tnames\tlibrary");
			List<FidSearchResult> results =
				service.processProgram(currentProgram, q, service.getDefaultScoreThreshold(), monitor);
			results.sort(Comparator.comparing(r -> r.function.getEntryPoint()));
			int n = 0;
			for (FidSearchResult r : results) {
				if (r.matches == null || r.matches.isEmpty()) continue;
				float best = 0;
				for (FidMatch m : r.matches) best = Math.max(best, m.getOverallScore());
				TreeSet<String> names = new TreeSet<>();
				LibraryRecord lib = null;
				for (FidMatch m : r.matches) {
					if (m.getOverallScore() < best) continue;
					names.add(m.getFunctionRecord().getName());
					if (lib == null) lib = m.getLibraryRecord();
				}
				w.printf("%s\t%s\t%d\t%.2f\t%d\t%s\t%s|%s|%s%n", r.function.getEntryPoint(),
					r.function.getName(), r.function.getBody().getNumAddresses(), best,
					names.size() == 1 ? 1 : 0, String.join(";", names),
					lib.getLibraryFamilyName(), lib.getLibraryVersion(), lib.getLibraryVariant());
				n++;
			}
			println("FIDREPORT " + currentProgram.getName() + " functions_with_matches=" + n);
		}
		finally {
			fm.removeUserFile(extra);
		}
	}
}
