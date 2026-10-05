// jgld.dll (Jackal graphics, debug build): member getters at 0x10017f30.. (class not named yet).
// FLAGS jgld.dll: /Od /ZI /GZ
class J17 {
public:
    int get28();
    int get14();
    int get20();
    int get2c();
    char pad[0x14]; int m_14; char pad18[8]; int m_20; char pad24[4]; int m_28; int m_2c;
};
// MATCH: jgld.dll 0x10017f30 ?get28@J17@@QAEHXZ
int J17::get28() { return m_28; }
// MATCH: jgld.dll 0x10017f70 ?get14@J17@@QAEHXZ
int J17::get14() { return m_14; }
// MATCH: jgld.dll 0x10017fe0 ?get20@J17@@QAEHXZ
int J17::get20() { return m_20; }
// MATCH: jgld.dll 0x10018020 ?get2c@J17@@QAEHXZ
int J17::get2c() { return m_2c; }
