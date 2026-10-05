// Print every function's stored prototype: "SIG\t<entry>\t<name>\t<calling convention>\t<prototype>", and every
// named (non-default) data label: "LBL\t<address>\t<name>".
// Used by re/tools/ghidra2src.py to declare callees exactly as the caller's decompilation expects them.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.*;

public class DumpSignatures extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            String proto = f.getSignature().getPrototypeString(true);
            println("SIG\t" + f.getEntryPoint() + "\t" + f.getName() + "\t" + f.getCallingConventionName() + "\t" + proto);
        }
        SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
        while (it.hasNext()) {
            Symbol s = it.next();
            if (s.getSymbolType() != SymbolType.LABEL || s.getSource() == SourceType.DEFAULT) continue;
            if (currentProgram.getFunctionManager().getFunctionAt(s.getAddress()) != null) continue;
            println("LBL\t" + s.getAddress() + "\t" + s.getName());
        }
    }
}
