// VC6 STL code instantiated in Terrain.dll (debug build): std::list<Tile*> (Terrain::m_drawList, +0x164ac4; see
// terrain_ctor.cpp / terrain_drawtile.cpp) with its iterators and allocator<Tile*>, 0x1000ae60..0x1000bb6f.
// The bodies are VC6's own <list>/<xmemory> code: this file only instantiates them. Names were assigned with
// re/tools/match_autoname.py (every obj function against every address); where several members compile to the
// same bytes the annotation says so and the name is NOT determined by the code (12 of 34).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
#include <list>
#include <string>
class Tile;
template class std::list<Tile*>;
template class std::basic_string<char>;
template class std::allocator<Tile*>;
// Default constructors of the nested iterator classes are only generated when used.
void useListIterators()
{
    std::list<Tile*>::iterator it;
    std::list<Tile*>::const_iterator cit;
    std::list<Tile*>::reverse_iterator rit;
    std::allocator<Tile*> al;
    std::_Destroy((char*)0);
}

// MATCH: Terrain.dll 0x1000ae60 ??0?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QAE@ABV?$allocator@PAVTile@@@1@@Z
// MATCH: Terrain.dll 0x1000aed0 ??1?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QAE@XZ
// MATCH: Terrain.dll 0x1000af70 ?begin@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QAE?AViterator@12@XZ   // 2 names compile identically: name not determined
// MATCH: Terrain.dll 0x1000afe0 ?end@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QAE?AViterator@12@XZ   // 2 names compile identically: name not determined
// MATCH: Terrain.dll 0x1000b040 ?empty@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QBE_NXZ
// MATCH: Terrain.dll 0x1000b090 ?push_back@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QAEXABQAVTile@@@Z   // 2 names compile identically: name not determined
// MATCH: Terrain.dll 0x1000b100 ?insert@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QAE?AViterator@12@V312@ABQAVTile@@@Z
// MATCH: Terrain.dll 0x1000b210 ?clear@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QAEXXZ
// MATCH: Terrain.dll 0x1000b280 ??0?$reverse_bidirectional_iterator@Viterator@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@PAVTile@@AAPAV4@PAPAV4@H@std@@QAE@XZ   // 2 names compile identically: name not determined
// MATCH: Terrain.dll 0x1000b2d0 ??Dconst_iterator@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QBEABQAVTile@@XZ   // 2 names compile identically: name not determined
// MATCH: Terrain.dll 0x1000b320 ??Eiterator@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QAE?AV012@H@Z
// MATCH: Terrain.dll 0x1000b380 ??8const_iterator@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QBE_NABV012@@Z   // 2 names compile identically: name not determined
// MATCH: Terrain.dll 0x1000b3d0 ??9const_iterator@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QBE_NABV012@@Z   // 2 names compile identically: name not determined
// MATCH: Terrain.dll 0x1000b430 ?size@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QBEIXZ   // 3 names compile identically: name not determined
// MATCH: Terrain.dll 0x1000b470 ?erase@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QAE?AViterator@12@V312@0@Z
// MATCH: Terrain.dll 0x1000b500 ?_Buynode@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@IAEPAU_Node@12@PAU312@0@Z
// MATCH: Terrain.dll 0x1000b5b0 ?_Freenode@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@IAEXPAU_Node@12@@Z
// MATCH: Terrain.dll 0x1000b600 ?_Next@_Acc@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@SAAAPAU_Node@23@PAU423@@Z
// MATCH: Terrain.dll 0x1000b630 ?_Prev@_Acc@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@SAAAPAU_Node@23@PAU423@@Z
// MATCH: Terrain.dll 0x1000b660 ?_Value@_Acc@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@SAAAPAVTile@@PAU_Node@23@@Z
// MATCH: Terrain.dll 0x1000b690 ?construct@?$allocator@PAVTile@@@std@@QAEXPAPAVTile@@ABQAV3@@Z
// MATCH: Terrain.dll 0x1000b6e0 ??0const_iterator@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QAE@XZ
// MATCH: Terrain.dll 0x1000b710 ?_Mynode@const_iterator@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QBEPAU_Node@23@XZ
// MATCH: Terrain.dll 0x1000b750 ??0iterator@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QAE@PAU_Node@12@@Z
// MATCH: Terrain.dll 0x1000b7a0 ??Econst_iterator@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QAEAAV012@XZ   // 4 names compile identically: name not determined
// MATCH: Terrain.dll 0x1000b800 ?erase@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@QAE?AViterator@12@V312@@Z
// MATCH: Terrain.dll 0x1000b910 ?_Charalloc@?$allocator@PAVTile@@@std@@QAEPADI@Z
// MATCH: Terrain.dll 0x1000b960 ?deallocate@?$allocator@PAVTile@@@std@@QAEXPAXI@Z   // 2 names compile identically: name not determined
// MATCH: Terrain.dll 0x1000b9b0 ??0?$reverse_bidirectional_iterator@Vconst_iterator@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@PAVTile@@ABQAV4@PBQAV4@H@std@@QAE@Vconst_iterator@?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@1@@Z   // 5 names compile identically: name not determined
// MATCH: Terrain.dll 0x1000b9f0 ?destroy@?$allocator@PAVTile@@@std@@QAEXPAPAVTile@@@Z
// MATCH: Terrain.dll 0x1000ba40 ?_Construct@std@@YAXPAPAVTile@@ABQAV2@@Z
// MATCH: Terrain.dll 0x1000bab0 ??2@YAPAXIPAX@Z
// MATCH: Terrain.dll 0x1000bae0 ?_Allocate@std@@YAPADHPAD@Z
// MATCH: Terrain.dll 0x1000bb40 ?_Destroy@std@@YAXPAD@Z   // 3 names compile identically: name not determined
