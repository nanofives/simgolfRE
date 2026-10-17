// Function boundary repair from a spec file (b1, 2026-10-17). Script args: <spec.tsv> <dry|apply> <report.txt>.
// Spec rows (tab separated, hex VAs, written by an offline recursive descent over the anchored image):
//   delete <entry> <name> <owner>           remove a function whose code belongs to <owner> (instructions stay)
//   set    <entry> <ranges> <starts>        give an existing function the body <ranges> (lo-hi;..., end exclusive)
//   create <entry> <ranges> <starts>        create a function (default FUN_ name) with that body
//   switch <site> <owner> <targets> [how]   computed-jump references from the `jmp [r*4 + T]` at <site>
//   thunk  <entry> <target>                 an incremental-link `jmp rel32` thunk as a thunk function of <target>
// <starts> are the instruction starts the descent decoded in the ranges the listing lacks; they are disassembled
// (flow restricted to the body) before any body is set. Bodies are explicit, so a function created before the code
// it jumps to cannot absorb that code. Order: deletes, disassembly, switch references, sets, creates, thunks,
// jump-table overrides (only for sites that had no computed-jump reference before). dry: every check, no change.
//@category Functions
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.pcode.JumpTable;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.SourceType;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class RepairBodies extends GhidraScript {
    private PrintWriter out;
    private final Map<String, Integer> counts = new LinkedHashMap<>();

    private void count(String k) { counts.merge(k, 1, Integer::sum); }

    private Address a(String hex) { return toAddr(Long.parseLong(hex.replaceFirst("^0x", ""), 16)); }

    private AddressSet ranges(String spec) {
        AddressSet s = new AddressSet();
        for (String r : spec.split(";")) {
            String[] p = r.split("-");
            s.add(a(p[0]), a(p[1]).subtract(1));
        }
        return s;
    }

    private List<Address> list(String spec) {
        List<Address> l = new ArrayList<>();
        if (spec == null || spec.isEmpty()) return l;
        for (String x : spec.split(",")) l.add(a(x));
        return l;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        boolean apply = args[1].equals("apply");
        out = new PrintWriter(args[2], "UTF-8");
        List<String[]> rows = new ArrayList<>();
        try (BufferedReader r = new BufferedReader(new FileReader(args[0]))) {
            String line;
            while ((line = r.readLine()) != null) {
                if (!line.isBlank()) rows.add(line.split("\t", -1));
            }
        }
        FunctionManager fm = currentProgram.getFunctionManager();
        Listing listing = currentProgram.getListing();
        ReferenceManager rm = currentProgram.getReferenceManager();

        // 1. deletes
        for (String[] r : rows) {
            if (!r[0].equals("delete")) continue;
            Function f = fm.getFunctionAt(a(r[1]));
            if (f == null) { out.println("DELETE MISSING " + r[1]); count("delete_missing"); continue; }
            if (!f.getName().equals(r[2])) { out.println("DELETE NAMEDIFF " + r[1] + " " + f.getName() + " expected " + r[2]); count("delete_namediff"); }
            out.println("DELETE " + r[1] + " " + f.getName() + " body " + f.getBody() + " -> part of " + r[3]);
            if (apply) fm.removeFunction(a(r[1]));
            count("delete");
        }

        // 2. disassembly of the starts the listing lacks, flow restricted to the row's body
        for (String[] r : rows) {
            if (!(r[0].equals("set") || r[0].equals("create"))) continue;
            AddressSet body = ranges(r[2]);
            AddressSet seeds = new AddressSet();
            int ok = 0, undef = 0, conflict = 0, foreign = 0;
            for (Address s : list(r.length > 3 ? r[3] : "")) {
                Instruction ins = listing.getInstructionAt(s);
                if (ins != null) { ok++; continue; }
                CodeUnit cu = listing.getCodeUnitContaining(s);
                Function owner = fm.getFunctionContaining(s);
                if (owner != null && !owner.getEntryPoint().equals(a(r[1]))) {
                    foreign++;
                    out.println("FOREIGN " + r[0] + " " + r[1] + " start " + s + " inside " + owner.getName());
                }
                if (cu instanceof Instruction || (cu instanceof Data && ((Data) cu).isDefined())) {
                    conflict++;
                    out.println("CONFLICT " + r[0] + " " + r[1] + " start " + s + " covered by " + cu.getMinAddress() + " " + cu);
                    if (apply && owner == null) listing.clearCodeUnits(cu.getMinAddress(), cu.getMaxAddress(), false);
                } else {
                    undef++;
                }
                seeds.add(s);
            }
            if (apply && !seeds.isEmpty()) {
                DisassembleCommand cmd = new DisassembleCommand(seeds, body, true);
                cmd.applyTo(currentProgram, monitor);
            }
            out.println("DISASM " + r[0] + " " + r[1] + " present " + ok + " undefined " + undef + " conflicts " + conflict + " foreign " + foreign);
            if (conflict > 0) count("rows_with_conflicts");
            if (foreign > 0) count("rows_with_foreign");
        }

        // 3. computed-jump references
        Set<String> newSwitch = new HashSet<>();
        for (String[] r : rows) {
            if (!r[0].equals("switch")) continue;
            Address site = a(r[1]);
            Instruction ins = listing.getInstructionAt(site);
            if (ins == null && !apply) { out.println("SWITCH " + r[1] + " no instruction yet (disassembled on apply)"); count("switch_pending"); continue; }
            if (ins == null) { out.println("SWITCH " + r[1] + " NO INSTRUCTION"); count("switch_noins"); continue; }
            Set<Address> have = new HashSet<>();
            for (Reference ref : rm.getReferencesFrom(site)) {
                if (ref.getReferenceType() == RefType.COMPUTED_JUMP) have.add(ref.getToAddress());
            }
            int added = 0;
            for (Address t : list(r[3])) {
                if (have.contains(t)) continue;
                if (apply) rm.addMemoryReference(site, t, RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
                added++;
            }
            if (added > 0) newSwitch.add(r[1]);
            out.println("SWITCH " + r[1] + " owner " + r[2] + " existing " + have.size() + " added " + added);
            count(have.isEmpty() ? "switch_new" : "switch_known");
        }

        // 4. sets, 5. creates
        for (String pass : new String[] {"set", "create"}) {
            for (String[] r : rows) {
                if (!r[0].equals(pass)) continue;
                Address e = a(r[1]);
                AddressSet body = ranges(r[2]);
                Function f = fm.getFunctionAt(e);
                if (pass.equals("set")) {
                    if (f == null) { out.println("SET MISSING " + r[1]); count("set_missing"); continue; }
                    out.println("SET " + r[1] + " " + f.getName() + " " + f.getBody() + " -> " + body);
                    if (apply) {
                        try { f.setBody(body); count("set"); }
                        catch (Exception ex) { out.println("SET FAILED " + r[1] + " " + ex.getMessage()); count("set_failed"); }
                    } else count("set");
                } else {
                    if (f != null) { out.println("CREATE EXISTS " + r[1] + " " + f.getName()); count("create_exists"); continue; }
                    Function other = fm.getFunctionContaining(e);
                    if (other != null) { out.println("CREATE INSIDE " + r[1] + " " + other.getName()); count("create_inside"); }
                    if (apply) {
                        try {
                            Function nf = fm.createFunction(null, e, body, SourceType.DEFAULT);
                            out.println("CREATE " + r[1] + " " + nf.getName() + " " + nf.getBody());
                            count("create");
                        } catch (Exception ex) { out.println("CREATE FAILED " + r[1] + " " + ex.getMessage()); count("create_failed"); }
                    } else { out.println("CREATE " + r[1] + " " + body); count("create"); }
                }
            }
        }

        // 5b. thunk <entry> <target>: an incremental-link `jmp rel32` thunk becomes a thunk function of its target
        for (String[] r : rows) {
            if (!r[0].equals("thunk")) continue;
            Address e = a(r[1]);
            Function t = fm.getFunctionAt(a(r[2]));
            Instruction ins = listing.getInstructionAt(e);
            if (t == null) { out.println("THUNK NOTARGET " + r[1] + " " + r[2]); count("thunk_notarget"); continue; }
            if (fm.getFunctionAt(e) != null) { out.println("THUNK EXISTS " + r[1]); count("thunk_exists"); continue; }
            out.println("THUNK " + r[1] + " -> " + r[2] + " " + t.getName() + (ins == null ? " (to disassemble)" : ""));
            if (apply) {
                try {
                    if (ins == null) new DisassembleCommand(e, new AddressSet(e, e.add(4)), false).applyTo(currentProgram, monitor);
                    Function nf = fm.createFunction(null, e, new AddressSet(e, e.add(4)), SourceType.DEFAULT);
                    nf.setThunkedFunction(t);
                    count("thunk");
                } catch (Exception ex) { out.println("THUNK FAILED " + r[1] + " " + ex.getMessage()); count("thunk_failed"); }
            } else count("thunk");
        }

        // 6. jump-table overrides for switches the program had no references for
        if (apply) {
            for (String[] r : rows) {
                if (!r[0].equals("switch") || !newSwitch.contains(r[1])) continue;
                Address site = a(r[1]);
                Function f = fm.getFunctionContaining(site);
                if (f == null) { out.println("OVERRIDE NOFUNC " + r[1]); count("override_nofunc"); continue; }
                ArrayList<Address> dest = new ArrayList<>(list(r[3]));
                try { new JumpTable(site, dest, true).writeOverride(f); count("override"); }
                catch (Exception ex) { out.println("OVERRIDE FAILED " + r[1] + " " + ex.getMessage()); count("override_failed"); }
            }
        }

        // 7. verification: undefined bytes left inside every set / created body
        if (apply) {
            for (String[] r : rows) {
                if (!(r[0].equals("set") || r[0].equals("create"))) continue;
                Function f = fm.getFunctionAt(a(r[1]));
                if (f == null) continue;
                AddressSet want = ranges(r[2]);
                long undef = 0;
                for (Address x : want.getAddresses(true)) {
                    if (listing.getInstructionContaining(x) == null) undef++;
                }
                boolean same = f.getBody().equals(want);
                if (undef > 0 || !same) out.println("VERIFY " + r[1] + " " + f.getName() + " body_equal " + same + " non_instruction_bytes " + undef);
                count(undef == 0 && same ? "verify_ok" : "verify_diff");
            }
        }
        out.println("REPAIRBODIES " + currentProgram.getName() + " " + (apply ? "apply" : "dry") + " " + counts);
        out.close();
        println("REPAIRBODIES " + currentProgram.getName() + " " + (apply ? "apply" : "dry") + " " + counts);
    }
}
