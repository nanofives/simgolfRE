// S11.2: read-only export of a program's names. Script arg: output TSV path.
// Rows: kind (FN | SYM) addr(VA, 8 hex) name(with namespace) source primary(1/0) symtype.
// FN = every function (getName(true)); SYM = every non-DEFAULT symbol that is not a function symbol.
//@category Symbol
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.*;

import java.io.PrintWriter;

public class ExportNames extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String out = getScriptArgs()[0];
		int nf = 0, ns = 0;
		try (PrintWriter w = new PrintWriter(out, "UTF-8")) {
			w.println("kind\taddr\tname\tsource\tprimary\tsymtype");
			for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
				Symbol s = f.getSymbol();
				w.println("FN\t" + String.format("%08x", f.getEntryPoint().getOffset()) + "\t" + f.getName(true) + "\t"
						+ s.getSource() + "\t" + (s.isPrimary() ? 1 : 0) + "\t" + s.getSymbolType());
				nf++;
			}
			SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
			while (it.hasNext()) {
				Symbol s = it.next();
				if (s.getSource() == SourceType.DEFAULT || s.getSymbolType() == SymbolType.FUNCTION || s.isExternal()
						|| !s.getAddress().isMemoryAddress()) {
					continue;
				}
				if (s.getSymbolType() != SymbolType.LABEL) {
					continue;
				}
				w.println("SYM\t" + String.format("%08x", s.getAddress().getOffset()) + "\t" + s.getName(true) + "\t"
						+ s.getSource() + "\t" + (s.isPrimary() ? 1 : 0) + "\t" + s.getSymbolType());
				ns++;
			}
		}
		println("EXPORTNAMES " + currentProgram.getName() + " functions=" + nf + " symbols=" + ns);
	}
}
