// For each address arg: the functions that reference it (callers), following one level of thunks
// (incremental-linking jmp stubs). Prints CALLER\t<target>\t<caller entry>\t<caller name>.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import java.util.*;

public class Callers extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String a : getScriptArgs()) {
            Address t = toAddr(a);
            List<Address> targets = new ArrayList<>(List.of(t));
            for (Reference r : getReferencesTo(t)) {          // thunks that jump to the body
                Function f = getFunctionContaining(r.getFromAddress());
                if (f != null && f.getBody().getNumAddresses() <= 5) targets.add(f.getEntryPoint());
                else if (f == null) targets.add(r.getFromAddress());
            }
            Set<String> seen = new TreeSet<>();
            for (Address x : targets)
                for (Reference r : getReferencesTo(x)) {
                    Function f = getFunctionContaining(r.getFromAddress());
                    if (f != null && !f.getEntryPoint().equals(t) && f.getBody().getNumAddresses() > 5)
                        seen.add(f.getEntryPoint() + "\t" + f.getName(true));
                }
            for (String s : seen) println("CALLER\t" + a + "\t" + s);
        }
    }
}
