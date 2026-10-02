// Per-function fingerprints for cross-version matching (v1.00 vs v1.03). One JSON object per line,
// prefixed FP:. Fields: entry, size, nins, exact (hash of mnemonics + operands with addresses masked),
// shape (hash of the mnemonic sequence only), calls (callee entries), strings (referenced string data),
// consts (distinct scalar immediates < 0x10000 that are not addresses).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.util.task.TaskMonitor;
import java.util.*;

public class ExportFingerprints extends GhidraScript {
    private static String esc(String s) {
        StringBuilder b = new StringBuilder();
        for (char c : s.toCharArray()) {
            if (c == '"' || c == '\\') b.append('\\').append(c);
            else if (c < 0x20 || c > 0x7e) b.append(String.format("\\u%04x", (int) c));
            else b.append(c);
        }
        return b.toString();
    }

    @Override
    public void run() throws Exception {
        Listing listing = currentProgram.getListing();
        long lo = currentProgram.getMinAddress().getOffset(), hi = currentProgram.getMaxAddress().getOffset();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            StringBuilder exact = new StringBuilder(), shape = new StringBuilder();
            TreeSet<String> strs = new TreeSet<>(), consts = new TreeSet<>();
            int n = 0;
            for (Instruction ins : listing.getInstructions(f.getBody(), true)) {
                n++;
                shape.append(ins.getMnemonicString()).append(';');
                exact.append(ins.getMnemonicString());
                for (int i = 0; i < ins.getNumOperands(); i++) {
                    exact.append(i == 0 ? ' ' : ',');
                    boolean masked = false;
                    for (Object o : ins.getOpObjects(i)) {
                        if (o instanceof Address) { masked = true; }
                        if (o instanceof Scalar) {
                            long v = ((Scalar) o).getUnsignedValue();
                            if (v >= lo && v <= hi) masked = true;
                            else if (v < 0x10000 && v > 1) consts.add(Long.toHexString(v));
                        }
                    }
                    exact.append(masked ? "ADDR" : ins.getDefaultOperandRepresentation(i));
                }
                exact.append(';');
                for (Reference r : ins.getReferencesFrom()) {
                    Data d = listing.getDataAt(r.getToAddress());
                    if (d != null && d.hasStringValue()) {
                        String s = StringDataInstance.getStringDataInstance(d).getStringValue();
                        if (s != null && s.length() >= 3) strs.add(s.length() > 80 ? s.substring(0, 80) : s);
                    }
                }
            }
            StringBuilder calls = new StringBuilder();
            for (Function c : f.getCalledFunctions(TaskMonitor.DUMMY)) {
                if (calls.length() > 0) calls.append(',');
                calls.append('"').append(c.isExternal() ? "EXT:" + c.getName() : c.getEntryPoint().toString()).append('"');
            }
            StringBuilder sj = new StringBuilder();
            for (String s : strs) { if (sj.length() > 0) sj.append(','); sj.append('"').append(esc(s)).append('"'); }
            StringBuilder cj = new StringBuilder();
            for (String s : consts) { if (cj.length() > 0) cj.append(','); cj.append('"').append(s).append('"'); }
            println("FP:{\"entry\":\"" + f.getEntryPoint() + "\",\"name\":\"" + esc(f.getName()) + "\",\"size\":"
                + f.getBody().getNumAddresses() + ",\"nins\":" + n
                + ",\"exact\":\"" + Integer.toHexString(exact.toString().hashCode()) + Integer.toHexString(exact.length())
                + "\",\"shape\":\"" + Integer.toHexString(shape.toString().hashCode()) + "\",\"mn\":\"" + esc(shape.toString())
                + "\",\"calls\":[" + calls + "],\"strings\":[" + sj + "],\"consts\":[" + cj + "]}");
        }
    }
}
