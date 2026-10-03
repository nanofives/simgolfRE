// Apply Function ID to the current program with Ghidra's own ApplyFidEntriesCommand (the FID analyzer's
// markup: label + demangle, plate comment, "Function ID Analyzer" bookmark), using the installed FidDbs plus
// one extra FidDb. alwaysApplyFidLabels = false: functions with a USER_DEFINED or IMPORTED symbol are skipped.
// Script args: <extra.fidb> <changes.tsv>   (TSV: entry, old name, new name)
// Run via re/tools/fid_apply.py (backs up nothing itself; the wrapper refuses a locked master).
//@category FunctionID
import java.io.*;
import java.util.*;

import ghidra.app.script.GhidraScript;
import ghidra.feature.fid.cmd.ApplyFidEntriesCommand;
import ghidra.feature.fid.db.*;
import ghidra.feature.fid.service.FidService;
import ghidra.program.model.listing.Function;

public class ApplyFid extends GhidraScript {

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		Map<String, String> before = new HashMap<>();
		for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
			before.put(f.getEntryPoint().toString(), f.getName());
		}
		FidFileManager fm = FidFileManager.getInstance();
		FidFile extra = fm.addUserFidFile(new File(args[0]));
		try {
			FidService service = new FidService();
			ApplyFidEntriesCommand cmd = new ApplyFidEntriesCommand(null,
				service.getDefaultScoreThreshold(), service.getDefaultMultiNameThreshold(), false, true);
			if (!cmd.applyTo(currentProgram, monitor)) {
				throw new IllegalStateException("ApplyFidEntriesCommand failed: " + cmd.getStatusMsg());
			}
		}
		finally {
			fm.removeUserFile(extra);
		}
		// "$L<n>" names are compiler-local labels inside library objects, not function names: undo them.
		for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
			String old = before.get(f.getEntryPoint().toString());
			if (old != null && f.getName().startsWith("$L") && old.startsWith("FUN_")) {
				println("REVERT " + f.getEntryPoint() + " " + f.getName());
				f.setName(null, ghidra.program.model.symbol.SourceType.DEFAULT);
				f.setComment(null);
				currentProgram.getBookmarkManager().removeBookmarks(
					new ghidra.program.model.address.AddressSet(f.getEntryPoint()),
					ghidra.program.model.listing.BookmarkType.ANALYSIS,
					ApplyFidEntriesCommand.FID_BOOKMARK_CATEGORY, monitor);
			}
		}
		int n = 0;
		try (PrintWriter w = new PrintWriter(new FileWriter(args[1]))) {
			w.println("entry\told\tnew");
			for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
				String old = before.get(f.getEntryPoint().toString());
				if (old != null && !old.equals(f.getName())) {
					w.printf("%s\t%s\t%s%n", f.getEntryPoint(), old, f.getName());
					n++;
				}
			}
		}
		println("FIDAPPLY " + currentProgram.getName() + " renamed=" + n);
	}
}
