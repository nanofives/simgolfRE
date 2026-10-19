// The port modules whose functions the registry (src/registry.cpp) exposes to the C ABI and the test harness.
// One line per module file port/src/sim/<module>.cpp, which defines `extern const portsim::ModuleDef kModule_<module>`.
// Phase 3 children: append your module here (one line), nothing else in the build needs editing.
SG_MODULE(util)
