# S6 decision table schema (re/port/defect_decisions.tsv)

One row per defect (model_defects.md numbers `D<n>`) or per non-PASS divergence row (`V:<kind>:<name>`). Tab separated, header row.

columns:
- id            D<n> | V:<kind>:<name>
- source        model_defects.md section / divergences.tsv kind
- function      function(s) and address(es) of the original, as the source cites them
- summary       one line, no banned words (probably, likely, seems to, appears to, I think, presumably, might be, maybe)
- class         hang | fault | overrun-silent | wrong-value | dead-state | port-gap | harmless
                hang = the original never leaves a loop; fault = the original raises an exception or reads/writes an unmapped address;
                overrun-silent = out-of-bounds access that lands in other game memory with no crash; wrong-value = computes a value
                that is not what the code means; dead-state = written never read or read never written; port-gap = a port feature gap,
                not a defect of the original; harmless = no observable effect
- changes_save, changes_balance, changes_sim, changes_frame, hang, fault: yes | no | unknown, one column each. yes only when
  the fix changes that. A fix that only replaces a hang or a fault by a clean return is "no" for save / balance / sim / frame.
- effect_note   which state, which instruction or port address shows it
- recommendation fix | keep | ask
                fix = D5 pre-approved: the defect is a hang or a fault AND the fix changes no save, balance, sim state or frame
                      on any run the original completes; keep = reproduce the original in both profiles; ask = goes to Mariano
- switch        existing sg_defect name | proposed `<function>.<what>` | none
- switch_state  exists | to-implement | not-needed
- evidence      the test, vector, segment or oracle run that shows the defect or would judge the fix
- uncertain     U-row or empty
