// Remove the user-defined labels at the given addresses (the counterpart of LabelGlobals.java for a label put at a
// wrong address). Script args: one or more hex addresses. Only non-function, non-external symbols at exactly that
// address are removed; the bytes stay. Prints one line per symbol and a summary.
//@category Symbol
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolType;

public class DeleteLabels extends GhidraScript {
    @Override
    protected void run() throws Exception {
        int removed = 0, missing = 0;
        for (String a : getScriptArgs()) {
            Address addr = toAddr(Long.parseLong(a.replaceFirst("^0x", ""), 16));
            boolean any = false;
            for (Symbol s : currentProgram.getSymbolTable().getSymbols(addr)) {
                if (s.getSymbolType() != SymbolType.LABEL || s.isExternal()) {
                    continue;
                }
                String name = s.getName();
                if (s.delete()) {
                    println("DELETELABEL REMOVED " + addr + " " + name);
                    removed++;
                    any = true;
                }
            }
            if (!any) {
                println("DELETELABEL NONE " + addr);
                missing++;
            }
        }
        println("DELETELABEL removed=" + removed + " missing=" + missing);
    }
}
