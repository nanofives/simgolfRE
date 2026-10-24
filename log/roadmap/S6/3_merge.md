# S6.3 merge notes

No shared file needs an edit for the delivered gate: profiles.py uses the existing sg_defect_set / sg_defect_reset / sg_defect_count
through lib.py. port/src/sim/profiles.cpp was not created.

## Optional C ABI (only if the host / playable build needs a native profile; the integrator applies it)

portsim_c.h, next to the sg_defect_* declarations:

```c
/* Profiles over the sg_defect switches. `original` = every switch 1. `fixed` = the members the caller passes
   (switch names and values, from re/port/defect_profiles.tsv read by the host). Returns SG_OK, SG_E_ARG for an unknown profile or switch
   (nothing is changed), SG_E_ARG for a member list given with `original`. */
SG_API int sg_profile_set(const char *profile, const char *const *switches, const int32_t *values, int32_t n);
/* "original" when every switch is 1, "fixed" when set by sg_profile_set("fixed", ...), else "custom". */
SG_API const char *sg_profile_get(void);
```

portsim_c.cpp: validate all names with the same lookup sg_defect_set uses before changing anything, call sg_defect_reset(), then set
each member; remember the last profile name and clear it when sg_defect_set is called directly. Python side then maps
`P.apply` to it. Not needed for S6.4 / S6.5 (they only need a switch and a row in the TSV).
