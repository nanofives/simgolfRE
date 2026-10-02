// Dump every function as TSV: entry, name, size, callers, callees. Prints lines prefixed FN\t.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.util.task.TaskMonitor;

public class ListFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            println("FN\t" + f.getEntryPoint() + "\t" + f.getName() + "\t" + f.getBody().getNumAddresses()
                + "\t" + f.getCallingFunctions(TaskMonitor.DUMMY).size() + "\t" + f.getCalledFunctions(TaskMonitor.DUMMY).size());
        }
    }
}
