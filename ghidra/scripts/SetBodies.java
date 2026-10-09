// Set function extents that Ghidra got wrong (noreturn calls, padding). Script args: <hex entry>:<hex or decimal size>
// pairs, e.g. 4aecd0:28 4aecf0:48 (sizes as in re/match/boundaries.tsv). An existing function at the entry gets the
// one-range body [entry, entry+size); a missing one is created with that body, after every shrink has been applied so
// the bytes it needs are free. Prints one line per pair and a summary.
//@category Functions
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

import java.util.ArrayList;
import java.util.List;

public class SetBodies extends GhidraScript {
    @Override
    protected void run() throws Exception {
        List<Address> create = new ArrayList<>();
        List<AddressSet> createBody = new ArrayList<>();
        int set = 0, made = 0, failed = 0;
        for (String a : getScriptArgs()) {
            String[] p = a.split(":");
            Address entry = toAddr(Long.parseLong(p[0].replaceFirst("^0x", ""), 16));
            long size = p[1].startsWith("0x") ? Long.parseLong(p[1].substring(2), 16) : Long.parseLong(p[1]);
            AddressSet body = new AddressSet(entry, entry.add(size - 1));
            Function f = getFunctionAt(entry);
            if (f == null) {
                create.add(entry);
                createBody.add(body);
                continue;
            }
            try {
                f.setBody(body);
                println("SETBODY SET " + entry + " " + f.getName() + " " + body);
                set++;
            } catch (Exception e) {
                println("SETBODY FAILED " + entry + " " + e.getMessage());
                failed++;
            }
        }
        for (int i = 0; i < create.size(); i++) {
            Address entry = create.get(i);
            try {
                Function f = currentProgram.getFunctionManager().createFunction(null, entry, createBody.get(i),
                        SourceType.DEFAULT);
                println("SETBODY NEW " + entry + " " + f.getName() + " " + f.getBody());
                made++;
            } catch (Exception e) {
                println("SETBODY FAILED " + entry + " " + e.getMessage());
                failed++;
            }
        }
        println("SETBODY set=" + set + " created=" + made + " failed=" + failed);
    }
}
