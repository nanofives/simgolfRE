// Give data addresses a USER_DEFINED primary label, only where the current primary symbol is a default one
// (DAT_*, or none). Never renames a user/imported/analysis name. Script args: <va_hex>#<name> (not "=": analyzeHeadless.bat splits on it) ...
//@category Symbol
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.*;

public class LabelGlobals extends GhidraScript {
	@Override
	protected void run() throws Exception {
		SymbolTable st = currentProgram.getSymbolTable();
		int done = 0, kept = 0;
		for (String a : getScriptArgs()) {
			String[] p = a.split("#");
			Address addr = toAddr(Long.parseLong(p[0], 16));
			Symbol cur = st.getPrimarySymbol(addr);
			if (cur != null && cur.getSource() != SourceType.DEFAULT) {
				println("KEEP " + addr + " " + cur.getName() + " (" + cur.getSource() + ")");
				kept++;
				continue;
			}
			Symbol s = st.createLabel(addr, p[1], SourceType.USER_DEFINED);
			s.setPrimary();
			println("LABEL " + addr + " " + p[1]);
			done++;
		}
		println("LABELGLOBALS labeled=" + done + " kept=" + kept);
	}
}
