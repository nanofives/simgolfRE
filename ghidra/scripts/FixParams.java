// Commit decompiler-inferred parameters for functions whose signature is still Ghidra's default, switching to
// __thiscall first when the body reads ECX before writing it (decompilation shows in_ECX).
// Args: [dry] -> report only. Prints "FIXPARAMS <entry> <old proto> => <new proto>" per change and a summary.
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.*;
import ghidra.program.model.symbol.SourceType;

public class FixParams extends GhidraScript {
    @Override
    public void run() throws Exception {
        boolean dry = getScriptArgs().length > 0 && getScriptArgs()[0].equals("dry");
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        int seen = 0, thiscall = 0, committed = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (monitor.isCancelled()) break;
            if (f.isThunk() || f.isExternal()) continue;
            if (f.getSignatureSource() != SourceType.DEFAULT) continue;
            seen++;
            String before = f.getSignature().getPrototypeString(true);
            DecompileResults r = d.decompileFunction(f, 60, monitor);
            if (!r.decompileCompleted()) continue;
            String c = r.getDecompiledFunction().getC();
            String conv = f.getCallingConventionName();
            boolean ecx = c.contains("in_ECX");
            if (ecx && !conv.equals("__thiscall") && !conv.equals("__fastcall")) {
                f.setCallingConvention("__thiscall");
                thiscall++;
                r = d.decompileFunction(f, 60, monitor);
                if (!r.decompileCompleted()) continue;
            }
            HighFunction hf = r.getHighFunction();
            try {
                HighFunctionDBUtil.commitParamsToDatabase(hf, true, HighFunctionDBUtil.ReturnCommitOption.COMMIT, SourceType.ANALYSIS);
                committed++;
            } catch (Exception e) {
                println("FIXPARAMS-ERR " + f.getEntryPoint() + " " + e.getMessage());
                continue;
            }
            String after = f.getSignature().getPrototypeString(true);
            if (!after.equals(before)) println("FIXPARAMS " + f.getEntryPoint() + " " + before + " => " + after);
        }
        println("FIXPARAMS-SUMMARY default=" + seen + " thiscall=" + thiscall + " committed=" + committed + (dry ? " (dry)" : ""));
        d.dispose();
    }
}
