// Remove functions created at addresses that turned out not to be function starts (the counterpart of
// CreateFunctions.java). Script args: one or more hex entry addresses. Only a function whose entry is exactly that
// address is removed; the instructions stay. Prints one line per address and a summary.
//@category Functions
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class DeleteFunctions extends GhidraScript {
    @Override
    protected void run() throws Exception {
        int removed = 0, missing = 0;
        for (String a : getScriptArgs()) {
            Address entry = toAddr(Long.parseLong(a.replaceFirst("^0x", ""), 16));
            Function f = getFunctionAt(entry);
            if (f == null) {
                println("DELETEFN NONE " + entry);
                missing++;
                continue;
            }
            String name = f.getName();
            currentProgram.getFunctionManager().removeFunction(entry);
            println("DELETEFN REMOVED " + entry + " " + name);
            removed++;
        }
        println("DELETEFN removed=" + removed + " missing=" + missing);
    }
}
