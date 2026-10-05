// Old Microsoft iostream (<fstream.h>) inline members emitted into Terrain.dll's objects (debug build, /MTd: the
// _MT versions of lock()/unlock() call _mtlock/_mtunlock). The bodies are the header's; this file only makes VC6
// emit them. Names assigned by re/tools/match_autoname.py. 0x10037930 sits after the CRT, next to normalize /
// reloadTextures / drawTile (0x10037c80..), i.e. it was emitted with that later block.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd
#include <fstream.h>
void useOldIostream(const char* name, char* buf, int n)
{
    ifstream f(name, ios::in | ios::nocreate | ios::binary);
    if (f.fail() || f.bad() || f.eof() || !f.good())
        return;
    f.lock(); f.unlock();
    f.rdbuf()->lock(); f.rdbuf()->unlock();
    f.read(buf, n);
    f.getline(buf, n);
    f >> n;
    f.seekg(0, ios::beg);
    f.close();
    ifstream g;
    g.open(name);
    filebuf* fb = g.rdbuf();
    fb->is_open();
}

// MATCH: Terrain.dll 0x100018b0 ?lock@ios@@QAAXXZ   // 2 names compile identically: name not determined
// MATCH: Terrain.dll 0x10002510 ?fail@ios@@QBEHXZ
// MATCH: Terrain.dll 0x10003f80 ?lockptr@ios@@IAEPAU_CRT_CRITICAL_SECTION@@XZ
// MATCH: Terrain.dll 0x100046a0 ??_Difstream@@QAEXXZ
// MATCH: Terrain.dll 0x10037930 ?unlock@ios@@QAAXXZ
// MATCH: Terrain.dll 0x10010590 ?getline@istream@@QAEAAV1@PADHD@Z
