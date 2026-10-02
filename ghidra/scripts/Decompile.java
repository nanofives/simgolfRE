// Decompile functions at the given addresses. Output between DECOMP-BEGIN/DECOMP-END markers.
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

public class Decompile extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        for (String a : getScriptArgs()) {
            Function f = getFunctionAt(toAddr(a));
            println("DECOMP-BEGIN " + a);
            if (f == null) { println("NO FUNCTION"); }
            else {
                DecompileResults r = d.decompileFunction(f, 60, monitor);
                println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "FAILED " + r.getErrorMessage());
            }
            println("DECOMP-END " + a);
        }
        d.dispose();
    }
}
