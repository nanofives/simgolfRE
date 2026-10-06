// Apply the evidence-backed names of re/names/*.tsv to the open program. Function rows (module addr name subsystem
// evidence purpose) rename a function when its current name is DEFAULT or ANALYSIS (FUN_*, FidDb names stay only if
// they were imported or user-set); `Class::method` puts the function in a class namespace. Rows of *_globals.tsv
// (module addr name type evidence) label data addresses whose primary symbol is a default one. Script arg: the
// re/names directory. Prints one line per change and a summary.
//@category Symbol
import ghidra.app.script.GhidraScript;
import ghidra.app.util.NamespaceUtils;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.*;

import java.io.File;
import java.nio.file.Files;
import java.util.List;

public class ApplyNames extends GhidraScript {
	@Override
	protected void run() throws Exception {
		File dir = new File(getScriptArgs()[0]);
		String prog = currentProgram.getName();
		SymbolTable st = currentProgram.getSymbolTable();
		int renamed = 0, kept = 0, missing = 0, labeled = 0;
		File[] files = dir.listFiles((d, n) -> n.endsWith(".tsv"));
		java.util.Arrays.sort(files);
		for (File f : files) {
			boolean globals = f.getName().endsWith("_globals.tsv");
			List<String> lines = Files.readAllLines(f.toPath());
			for (int i = 1; i < lines.size(); i++) {
				String[] c = lines.get(i).split("\t");
				if (c.length < 3 || !c[0].equals(prog) || c[2].isEmpty()) {
					continue;
				}
				Address addr = toAddr(Long.parseLong(c[1].replace("0x", ""), 16));
				if (globals) {
					Symbol cur = st.getPrimarySymbol(addr);
					if (cur != null && cur.getSource() != SourceType.DEFAULT) {
						kept++;
						continue;
					}
					Symbol s = st.createLabel(addr, SymbolUtilities.replaceInvalidChars(c[2], true), SourceType.USER_DEFINED);
					s.setPrimary();
					labeled++;
					continue;
				}
				Function fn = getFunctionAt(addr);
				if (fn == null) {
					println("MISSING " + addr + " " + c[2] + " (" + f.getName() + ")");
					missing++;
					continue;
				}
				SourceType src = fn.getSymbol().getSource();
				if (src == SourceType.USER_DEFINED || src == SourceType.IMPORTED) {
					if (!fn.getName(true).equals(c[2])) {
						println("KEEP " + addr + " " + fn.getName(true) + " (" + src + ") over " + c[2]);
					}
					kept++;
					continue;
				}
				String full = c[2];
				int sep = full.lastIndexOf("::");
				Namespace ns = currentProgram.getGlobalNamespace();
				String name = full;
				if (sep > 0) {
					String scope = full.substring(0, sep);
					name = full.substring(sep + 2);
					ns = NamespaceUtils.createNamespaceHierarchy(scope.replace("<", "(").replace(">", ")").replace("*", "P"),
							null, currentProgram, SourceType.USER_DEFINED);
				}
				name = SymbolUtilities.replaceInvalidChars(name, true);
				fn.getSymbol().setNameAndNamespace(name, ns, SourceType.USER_DEFINED);
				println("RENAME " + addr + " " + full);
				renamed++;
			}
		}
		println("APPLYNAMES " + prog + " renamed=" + renamed + " labeled=" + labeled + " kept=" + kept + " missing=" + missing);
	}
}
