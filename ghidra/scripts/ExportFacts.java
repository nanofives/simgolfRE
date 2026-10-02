// Headless fact export for the test suite. Prints one JSON object on a line prefixed FACTS:.
// Run: analyzeHeadless <proj_dir> SimGolf -process golf_clean.exe -readOnly -noanalysis
//        -scriptPath ghidra/scripts -postScript ExportFacts.java [addr ...]
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;

public class ExportFacts extends GhidraScript {
    @Override
    public void run() throws Exception {
        FunctionManager fm = currentProgram.getFunctionManager();
        StringBuilder sb = new StringBuilder();
        sb.append("{\"program\":\"").append(currentProgram.getName()).append("\"");
        sb.append(",\"image_base\":\"").append(currentProgram.getImageBase()).append("\"");
        sb.append(",\"function_count\":").append(fm.getFunctionCount());
        sb.append(",\"entry\":{");
        String[] args = getScriptArgs();
        for (int i = 0; i < args.length; i++) {
            Address a = toAddr(args[i]);
            Function f = fm.getFunctionAt(a);
            if (i > 0) sb.append(",");
            sb.append("\"").append(args[i]).append("\":");
            sb.append(f == null ? "null" : "\"" + f.getName() + "\"");
        }
        sb.append("}}");
        println("FACTS:" + sb);
    }
}
