// Dump every function as TSV: entry, name, size, callers, callees. Prints lines prefixed FN\t.
// With a script arg (output path) it writes re/functions_ghidra*.tsv instead: header line, entries as 8 hex digits
// (the exe: VA; DLLs: RVA from the image base), names with their namespace (Class::method).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.util.task.TaskMonitor;

import java.io.PrintWriter;

public class ListFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length == 0) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                println("FN\t" + f.getEntryPoint() + "\t" + f.getName() + "\t" + f.getBody().getNumAddresses()
                    + "\t" + f.getCallingFunctions(TaskMonitor.DUMMY).size() + "\t" + f.getCalledFunctions(TaskMonitor.DUMMY).size());
            }
            return;
        }
        boolean dll = !currentProgram.getName().endsWith(".exe");
        long base = currentProgram.getImageBase().getOffset();
        int n = 0;
        try (PrintWriter w = new PrintWriter(args[0], "UTF-8")) {
            w.print("entry\tname\tsize\tcallers\tcallees\n");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                long a = f.getEntryPoint().getOffset() - (dll ? base : 0);
                w.print(String.format("%08x", a) + "\t" + f.getName(true) + "\t" + f.getBody().getNumAddresses()
                    + "\t" + f.getCallingFunctions(TaskMonitor.DUMMY).size() + "\t" + f.getCalledFunctions(TaskMonitor.DUMMY).size() + "\n");
                n++;
            }
        }
        println("LISTFUNCTIONS " + currentProgram.getName() + " " + n + " -> " + args[0]);
    }
}
