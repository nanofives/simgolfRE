// Create functions at entry points Ghidra's analysis missed (reached only through a stored pointer, so no call
// reference made analysis start one there). Script args: one or more hex entry addresses. Disassembles from each
// entry and creates the function with Ghidra's own body computation; an address that already starts a function is
// reported and left alone. Prints one line per address and a summary.
//@category Functions
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class CreateFunctions extends GhidraScript {
    @Override
    protected void run() throws Exception {
        int created = 0, kept = 0, failed = 0;
        for (String a : getScriptArgs()) {
            Address entry = toAddr(Long.parseLong(a.replaceFirst("^0x", ""), 16));
            Function f = getFunctionAt(entry);
            if (f != null) {
                println("CREATEFN KEEP " + entry + " " + f.getName());
                kept++;
                continue;
            }
            new DisassembleCommand(entry, null, true).applyTo(currentProgram, monitor);
            CreateFunctionCmd cmd = new CreateFunctionCmd(entry);
            if (cmd.applyTo(currentProgram, monitor) && getFunctionAt(entry) != null) {
                f = getFunctionAt(entry);
                println("CREATEFN NEW " + entry + " " + f.getName() + " body " + f.getBody());
                created++;
            } else {
                println("CREATEFN FAIL " + entry + " " + cmd.getStatusMsg());
                failed++;
            }
        }
        println("CREATEFN created=" + created + " kept=" + kept + " failed=" + failed);
    }
}
