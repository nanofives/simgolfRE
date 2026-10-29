import pathlib
a = pathlib.Path("log/roadmap/S4/3_assemble.py")
s = a.read_text()
old = r'''    a = (f"int g_negctl_{name} = 0;\nvoid a_{name}('''
assert old in s
s = s.replace(old, r'''    a = (f"void a_{name}(''')
old2 = r'''    out = [HEAD.read_text().rstrip("\n").rsplit("}  // namespace", 1)[0], pre] + blocks'''
assert old2 in s
s = s.replace(old2, r'''    names = [MARK.match(text[m.start():]).group(1) for m in ms]
    negs = "// negative-control modes (0 = the correct body; mode 1 every function, mode 2 the spill controls)\n" + \
        "".join(f"int g_negctl_{n} = 0;\n" for n in names)
    out = [HEAD.read_text().rstrip("\n").rsplit("}  // namespace", 1)[0], negs, pre] + blocks''')
a.write_text(s)
p = pathlib.Path("log/roadmap/S4/3_parts/body.cpp")
t = p.read_text()
def rep(old, new, n=1):
    global t
    assert t.count(old) == n, (old, t.count(old))
    t = t.replace(old, new)
# codecRemoveMean: mode 2 keeps the accumulator unspilled (a double across the loop)
rep("""    float acc = 0.0f;
    int n = *count;
    for (int i = 1; i <= n; ++i)
        acc = static_cast<float>(static_cast<double>(acc) + static_cast<double>(in[i]));
    acc = static_cast<float>(static_cast<double>(acc) / static_cast<double>(*count));""",
"""    float acc = 0.0f;
    int n = *count;
    if (NEG(codecRemoveMean) == 2) {                // control: the sum is never spilled to its float slot
        double wide = 0.0;
        for (int i = 1; i <= n; ++i) wide += static_cast<double>(in[i]);
        acc = static_cast<float>(wide / static_cast<double>(*count));
    } else {
        for (int i = 1; i <= n; ++i)
            acc = static_cast<float>(static_cast<double>(acc) + static_cast<double>(in[i]));
        acc = static_cast<float>(static_cast<double>(acc) / static_cast<double>(*count));
    }""")
# lpcSynthesize: mode 2 keeps the energy sum unspilled
rep("""        energy = static_cast<float>(static_cast<double>(hist[k]) * static_cast<double>(hist[k])
                                    + static_cast<double>(energy));                    // 0x3d7d4-0x3d7df""",
"""        if (NEG(lpcSynthesize) == 2)                // control: no float spill of the energy sum
            energyWide = static_cast<double>(hist[k]) * static_cast<double>(hist[k]) + energyWide;
        else
            energy = static_cast<float>(static_cast<double>(hist[k]) * static_cast<double>(hist[k])
                                        + static_cast<double>(energy));                // 0x3d7d4-0x3d7df""")
rep("""    float energy;
    *pc<uint32_t>(&energy) = 0;               // 0x3d673""",
"""    float energy;
    *pc<uint32_t>(&energy) = 0;               // 0x3d673
    double energyWide = 0.0;                  // mode 2 only""")
rep("""        crt_sqrt(product / static_cast<double>(energy)));""",
"""        crt_sqrt(product / (NEG(lpcSynthesize) == 2 ? energyWide : static_cast<double>(energy))));""")
# codecHighPassFilter: mode 2 skips the spill of the first stage's t
rep("""        float t = static_cast<float>(
            (static_cast<double>(s0) * static_cast<double>(ka) + static_cast<double>(sample))
            - static_cast<double>(s1) * static_cast<double>(kb));        // fstp at 0x40ec4
        sample = static_cast<float>(
            (static_cast<double>(t) - (static_cast<double>(s0) + static_cast<double>(s0)))
            + static_cast<double>(s1));                                  // fstp at 0x40ed2""",
"""        const double tw = (static_cast<double>(s0) * static_cast<double>(ka) + static_cast<double>(sample))
                          - static_cast<double>(s1) * static_cast<double>(kb);
        float t = static_cast<float>(tw);                                // fstp at 0x40ec4
        sample = static_cast<float>(
            ((NEG(codecHighPassFilter) == 2 ? tw : static_cast<double>(t))   // control: no spill of t
             - (static_cast<double>(s0) + static_cast<double>(s0)))
            + static_cast<double>(s1));                                  // fstp at 0x40ed2""")
p.write_text(t)
