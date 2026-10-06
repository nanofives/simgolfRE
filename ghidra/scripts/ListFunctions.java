// Dump every function as TSV: entry, name, size, callers, callees. Prints lines prefixed FN\t.
// With a script arg (output path) it writes re/functions_ghidra*.tsv instead: header line, entries as 8 hex digits
// (the exe: VA; DLLs: RVA from the image base), names with their namespace (Class::method), and the body's address
// ranges ("start-end;...", same address space as the entry, end exclusive). A body is not always contiguous
// (mainLoop 0x0040f5c0 has code up to 0x00421614 with other functions in between), so `size` is not an extent.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.listing.Function;
import ghidra.util.task.TaskMonitor;

import java.io.PrintWriter;

public class ListFunctions extends GhidraScript {
    private static String ranges(Function f, long base) {
        StringBuilder sb = new StringBuilder();
        for (AddressRange r : f.getBody()) {
            if (sb.length() > 0) sb.append(';');
            sb.append(String.format("%08x-%08x", r.getMinAddress().getOffset() - base, r.getMaxAddress().getOffset() + 1 - base));
        }
        return sb.toString();
    }

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
        long base = dll ? currentProgram.getImageBase().getOffset() : 0;
        int n = 0;
        try (PrintWriter w = new PrintWriter(args[0], "UTF-8")) {
            w.print("entry\tname\tsize\tcallers\tcallees\tranges\n");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                long a = f.getEntryPoint().getOffset() - base;
                w.print(String.format("%08x", a) + "\t" + f.getName(true) + "\t" + f.getBody().getNumAddresses()
                    + "\t" + f.getCallingFunctions(TaskMonitor.DUMMY).size() + "\t" + f.getCalledFunctions(TaskMonitor.DUMMY).size()
                    + "\t" + ranges(f, base) + "\n");
                n++;
            }
        }
        println("LISTFUNCTIONS " + currentProgram.getName() + " " + n + " -> " + args[0]);
    }
}
