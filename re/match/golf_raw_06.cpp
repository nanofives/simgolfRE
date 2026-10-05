// FLAGS golf_clean.exe: /O2
// golf_clean.exe functions matched from Ghidra's decompilation by re/tools/ghidra2src.py (raw form: Ghidra's offsets,
// casts and FUN_/DAT_ names; in a /Od build this compiles to the same instructions as the original member
// accesses, so each // MATCH: below is a 100% instruction match). Names, types and layouts are still to be
// written: these are evidence of byte identity, not readable source. String literals are placeholders named by
// address (s_<addr>), never the game's text.

#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_00448220 {
// MATCH: golf_clean.exe 0x00448220 ?FUN_00448220@f_00448220@@YAXXZ
extern int DAT_0080d840;
extern int DAT_0080d8ac;
extern int DAT_0080d918;
extern int DAT_0080d984;
extern int DAT_0080d9f0;
extern int DAT_0080da5c;
extern int DAT_0080dac8;
extern int DAT_0080db34;
extern int DAT_0080dba0;
extern int DAT_0080dc0c;
extern int DAT_0080dc78;
extern int DAT_0080dce4;
extern int DAT_0080dd50;
extern int DAT_0080ddbc;
extern int DAT_0080de28;
extern int DAT_0080de94;
extern int DAT_0080df00;
extern int DAT_0080df6c;
extern int DAT_0080dfd8;
extern int DAT_0080e044;
extern int DAT_0080e0b0;
extern int DAT_0080e11c;
extern int DAT_0080e188;
extern int DAT_0080e1f4;
extern int DAT_0080e260;
extern int DAT_0080e2cc;
extern int DAT_0080e47c;
extern int DAT_0080e4e8;
extern int DAT_0080e554;
extern int DAT_0080e5c0;
extern int DAT_0080e62c;
extern int DAT_0080e698;
extern int DAT_0080e704;
extern int DAT_0080e770;
extern int DAT_0080e7dc;
extern int DAT_0080e848;
extern int DAT_0080e8b4;
extern int DAT_0080e920;
extern int DAT_0080e98c;
extern int DAT_0080e9f8;
extern int DAT_0080ea64;
extern int DAT_0080ead0;
extern int DAT_0080eb3c;
extern int DAT_0080eba8;
extern int DAT_0080ec14;
extern int DAT_0080ec80;
extern int DAT_0080ecec;
extern int DAT_0080ed58;
extern int DAT_0080edc4;
extern int DAT_0080ee30;
extern int DAT_0080ee9c;
extern int DAT_0080ef08;
extern int DAT_0080ef74;
extern int DAT_0080efe0;
extern int DAT_0080f04c;
extern int DAT_0080f0b8;
extern int DAT_0080f124;
extern int DAT_0080f190;
extern int DAT_0080f1fc;
extern int DAT_0080f268;
extern int DAT_0080f2d4;
extern int DAT_0080f340;
extern int DAT_0080f3ac;
extern int DAT_0080f418;
extern int DAT_0080f484;
extern int DAT_0080f4f0;
extern int DAT_0080f55c;
extern int DAT_0080f5c8;
extern int DAT_0080f634;
extern int DAT_0080f6a0;
extern int DAT_0080f70c;
extern int DAT_0080f778;
extern int DAT_0080f7e4;
extern int DAT_0080f850;
extern int DAT_0080f8bc;
extern int DAT_0080f928;
extern int DAT_0080f994;
extern int DAT_0080fa00;
extern int DAT_0080fa6c;
extern int DAT_0080fad8;
extern int DAT_0080fb44;
extern int DAT_0080fc88;
extern int DAT_0080fcf4;
extern int DAT_0080fd60;
extern int DAT_0080fdcc;
extern int DAT_0080fe38;
extern int DAT_0080fea4;
extern int DAT_0080ff10;
extern int DAT_0080ff7c;
extern int DAT_0080ffe8;
extern int DAT_00810054;
extern int DAT_008100c0;
extern int DAT_0081012c;
extern int DAT_00810198;
extern int DAT_00810204;
extern int DAT_00810270;
extern int DAT_008102dc;
extern int DAT_00810348;
extern int DAT_008103b4;
extern int DAT_00810420;
extern int DAT_0081048c;
extern int DAT_008104f8;
extern int DAT_00810564;
extern int DAT_008106a8;
extern int DAT_00810714;
extern int DAT_00810780;
extern int DAT_008108c4;
extern int DAT_00810930;
extern int DAT_0081099c;
extern int DAT_00810ae0;
extern int DAT_00810b4c;
extern int DAT_00810bb8;
extern int DAT_00810cfc;
extern int DAT_00810d68;
extern int DAT_00810dd4;
extern int DAT_00810e40;
extern int DAT_00810eac;
extern int DAT_00810f18;
extern int DAT_00810f84;
extern int DAT_00810ff0;
extern int DAT_0081105c;
extern int DAT_008110c8;
extern int DAT_00811134;
extern int DAT_008111a0;
extern int DAT_0081120c;
extern int DAT_00811278;
extern int DAT_008112e4;
extern int DAT_00811350;
extern int DAT_008113bc;
extern int DAT_00811494;
extern int DAT_00811500;
extern int DAT_0081156c;
extern int DAT_008115d8;
extern int DAT_00811644;
extern int DAT_008116b0;
extern int DAT_0081171c;
extern int DAT_00811788;
extern int DAT_008117f4;
extern int DAT_00811860;
extern int DAT_008118cc;
extern int DAT_00811938;
extern int DAT_008119a4;
extern int DAT_00811a10;
extern int DAT_00811a7c;
extern int DAT_00811ae8;
extern int DAT_00811b54;
extern int DAT_00811bc0;
extern int DAT_00811c2c;
extern int DAT_00811c98;
extern int DAT_00811d04;
extern int DAT_00811d70;
extern int DAT_00811ddc;
extern int DAT_00811e48;
extern int DAT_00811eb4;
extern int DAT_00812790;
extern int DAT_008127fc;
extern int DAT_00812868;
extern int DAT_008128d4;
extern int DAT_00812940;
extern int DAT_008129ac;
extern int DAT_00812a18;
extern int DAT_00812a84;
extern int DAT_00812af0;
extern int DAT_00812b5c;
extern int DAT_00812bc8;
extern int DAT_00812c34;
extern int DAT_00812ca0;
extern int DAT_0081306c;
extern int DAT_008130d8;
extern int DAT_00813144;
extern int DAT_008131b0;
extern int DAT_0081321c;
extern int DAT_008132f4;
extern int DAT_00813360;
extern int DAT_008133cc;
extern int DAT_00813438;
extern int DAT_00813510;
extern int DAT_0081357c;
extern int DAT_008135e8;
extern int DAT_00813654;
extern int DAT_0081372c;
extern int DAT_00813798;
extern int DAT_00813804;
extern int DAT_00813870;
extern int DAT_00813948;
extern int DAT_008139b4;
extern int DAT_00813a20;
extern int DAT_00813a8c;
extern int DAT_00813b64;
extern int DAT_00813bd0;
extern int DAT_00813c3c;
extern int DAT_00813ca8;
extern int DAT_008141b8;
extern int DAT_00814224;
extern int DAT_00814290;
extern int DAT_008143d4;
extern int DAT_00814440;
extern int DAT_008144ac;
extern int DAT_00814518;
extern int DAT_00814584;
extern int DAT_008145f0;
extern int DAT_0081465c;
extern int DAT_008146c8;
extern int DAT_00814734;
extern int DAT_008147a0;
extern int DAT_0081480c;
extern int DAT_00814878;
extern int DAT_008148e4;
extern int DAT_00814950;
extern int DAT_008149bc;
extern char s_004d1a2c[];
extern char s_004d19a8[];
extern char s_004d1a60[];
extern char s_004d1954[];
extern char s_004d1a44[];
extern char s_004d19f8[];
extern char s_004d1a98[];
extern char s_004d1aec[];
extern char s_004d1ad0[];
extern char s_004d1ab4[];
extern char s_004d1608[];
extern char s_004d1970[];
extern char s_004d19c4[];
extern char s_004d1920[];
extern char s_004d1a14[];
extern char s_004d198c[];
extern char s_004d18cc[];
extern char s_004d18b0[];
extern char s_004d1894[];
extern char s_004d1878[];
extern char s_004d1a7c[];
extern char s_004d18e8[];
extern char s_004d19dc[];
extern char s_004d1904[];
extern char s_004d193c[];
extern char s_004d1cdc[];
extern char s_004d1828[];
extern char s_004d1c60[];
extern char s_004d1d0c[];
extern char s_004d1c14[];
extern char s_004d1cf4[];
extern char s_004d1cac[];
extern char s_004d16ac[];
extern char s_004d1d40[];
extern char s_004d16e4[];
extern char s_004d1d88[];
extern char s_004d1d70[];
extern char s_004d1d58[];
extern char s_004d1810[];
extern char s_004d1c2c[];
extern char s_004d1c78[];
extern char s_004d1be4[];
extern char s_004d17c0[];
extern char s_004d1cc4[];
extern char s_004d1844[];
extern char s_004d1c48[];
extern char s_004d1b9c[];
extern char s_004d1b80[];
extern char s_004d1b64[];
extern char s_004d1b48[];
extern char s_004d17f4[];
extern char s_004d17dc[];
extern char s_004d1d24[];
extern char s_004d1bb4[];
extern char s_004d185c[];
extern char s_004d1c90[];
extern char s_004d1bcc[];
extern char s_004d1bfc[];
extern char s_004d1770[];
extern char s_004d1754[];
extern char s_004d1700[];
extern char s_004d178c[];
extern char s_004d1738[];
extern char s_004d171c[];
extern char s_004d17a4[];
extern char s_004d0d20[];
extern char s_004d166c[];
extern char s_004d106c[];
extern char s_004d1084[];
extern char s_004d109c[];
extern char s_004d159c[];
extern char s_004d1510[];
extern char s_004d1628[];
extern char s_004d164c[];
extern char s_004d15c4[];
extern char s_004d1578[];
extern char s_004d16c8[];
extern char s_004d1534[];
extern char s_004d1558[];
extern char s_004d14f4[];
extern char s_004d1690[];
extern char s_004d15ec[];
extern char s_004d1b28[];
extern char s_004d10b0[];
extern char s_004d10fc[];
extern char s_004d1114[];
extern char s_004d1dc4[];
extern char s_004d1da0[];
extern char s_004d1e74[];
extern char s_004d1e14[];
extern char s_004d1dec[];
extern char s_004d1e34[];
extern char s_004d1e54[];
extern char s_004d1eb4[];
extern char s_004d1eec[];
extern char s_004d1f64[];
extern char s_004d1f3c[];
extern char s_004d1f10[];
extern char s_004d1f8c[];
extern char s_004d1ed0[];
extern char s_004d1e98[];
extern char s_004d2020[];
extern char s_004d1ffc[];
extern char s_004d1b08[];
extern char s_004d10e4[];
extern char s_004d112c[];
extern char s_004d1140[];
extern char s_004d11c4[];
extern char s_004d1194[];
extern char s_004d11ac[];
extern char s_004d117c[];
extern char s_004d1164[];
extern char s_004d0ca0[];
extern char s_004d0cc0[];
extern char s_004d0da4[];
extern char s_004d0d40[];
extern char s_004d0d60[];
extern char s_004d0e08[];
extern char s_004d0cfc[];
extern char s_004d0dc4[];
extern char s_004d0ce0[];
extern char s_004d0de8[];
extern char s_004d0d84[];
extern char s_004d1218[];
extern char s_004d1294[];
extern char s_004d1254[];
extern char s_004d1270[];
extern char s_004d1238[];
extern char s_004d131c[];
extern char s_004d146c[];
extern char s_004d1450[];
extern char s_004d0fb8[];
extern char s_004d1438[];
extern char s_004d12d0[];
extern char s_004d13e4[];
extern char s_004d0ff0[];
extern char s_004d1394[];
extern char s_004d13c8[];
extern char s_004d11f8[];
extern char s_004d13ac[];
extern char s_004d141c[];
extern char s_004d0fd4[];
extern char s_004d1010[];
extern char s_004d11dc[];
extern char s_004d14bc[];
extern char s_004d1488[];
extern char s_004d12b4[];
extern char s_004d14a4[];
extern char s_004d1378[];
extern char s_004d1028[];
extern char s_004d1340[];
extern char s_004d135c[];
extern char s_004d1400[];
extern char s_004d14d8[];
extern char s_004d09e8[];
extern char s_004d0a50[];
extern char s_004d0a30[];
extern char s_004d0a0c[];
extern char s_004d08d0[];
extern char s_004d0938[];
extern char s_004d0918[];
extern char s_004d08f4[];
extern char s_004d095c[];
extern char s_004d09c4[];
extern char s_004d09a4[];
extern char s_004d0980[];
extern char s_004d0838[];
extern char s_004d08ac[];
extern char s_004d0888[];
extern char s_004d0860[];
extern char s_004d0b88[];
extern char s_004d0bf0[];
extern char s_004d0bd0[];
extern char s_004d0bac[];
extern char s_004d0b00[];
extern char s_004d0b68[];
extern char s_004d0b48[];
extern char s_004d0b24[];
extern char s_004d0c14[];
extern char s_004d0c7c[];
extern char s_004d0c5c[];
extern char s_004d0c38[];
extern char s_004d0a74[];
extern char s_004d0adc[];
extern char s_004d0abc[];
extern char s_004d0a98[];
extern char s_004d104c[];
extern char s_004d1fb8[];
extern char s_004d1fdc[];
extern char s_004d06e0[];
extern char s_004d06c4[];
extern char s_004d07b8[];
extern char s_004d0818[];
extern char s_004d07f8[];
extern char s_004d07d8[];
extern char s_004d0688[];
extern char s_004d06a8[];
extern char s_004d075c[];
extern char s_004d0738[];
extern char s_004d0644[];
extern char s_004d0718[];
extern char s_004d06fc[];
extern char s_004d0780[];
extern char s_004d079c[];
extern char s_004d0668[];
extern char s_004d0620[];
extern char s_004d12f8[];
extern char s_004d0604[];
extern char s_004d0f18[];
extern char s_004d0f48[];
extern char s_004d0e28[];
extern char s_004d0e50[];
extern char s_004d0e98[];
extern char s_004d0ec8[];
extern char s_004d10cc[];
extern char s_004d0fa4[];
extern char s_004d0e7c[];
extern char s_004d0f78[];
extern char s_004d0ef8[];
struct T_FUN_00484e30 { undefined4 FUN_00484e30(undefined4, uint); };
void __cdecl FUN_00448220()
{
  ((T_FUN_00484e30 *)(((char *)&DAT_00812790)))->FUN_00484e30((unsigned int)(s_004d2020), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008127fc)))->FUN_00484e30((unsigned int)(s_004d1ffc), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00812c34)))->FUN_00484e30((unsigned int)(s_004d1fdc), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00812ca0)))->FUN_00484e30((unsigned int)(s_004d1fb8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00812868)))->FUN_00484e30((unsigned int)(s_004d1f8c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008128d4)))->FUN_00484e30((unsigned int)(s_004d1f64), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00812940)))->FUN_00484e30((unsigned int)(s_004d1f3c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008129ac)))->FUN_00484e30((unsigned int)(s_004d1f10), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080d840)))->FUN_00484e30((unsigned int)(s_004d1eec), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080d8ac)))->FUN_00484e30((unsigned int)(s_004d1ed0), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080d918)))->FUN_00484e30((unsigned int)(s_004d1eb4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080d984)))->FUN_00484e30((unsigned int)(s_004d1e98), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080d9f0)))->FUN_00484e30((unsigned int)(s_004d1e74), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080da5c)))->FUN_00484e30((unsigned int)(s_004d1e54), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080dac8)))->FUN_00484e30((unsigned int)(s_004d1e34), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080db34)))->FUN_00484e30((unsigned int)(s_004d1e14), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080dba0)))->FUN_00484e30((unsigned int)(s_004d1dec), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080ef08)))->FUN_00484e30((unsigned int)(s_004d1dc4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080ef74)))->FUN_00484e30((unsigned int)(s_004d1da0), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080dce4)))->FUN_00484e30((unsigned int)(s_004d1d88), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080ddbc)))->FUN_00484e30((unsigned int)(s_004d1d70), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080df6c)))->FUN_00484e30((unsigned int)(s_004d1d58), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e044)))->FUN_00484e30((unsigned int)(s_004d1d40), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e11c)))->FUN_00484e30((unsigned int)(s_004d1d24), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e1f4)))->FUN_00484e30((unsigned int)(s_004d1d0c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f1fc)))->FUN_00484e30((unsigned int)(s_004d1cf4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f2d4)))->FUN_00484e30((unsigned int)(s_004d1cdc), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f3ac)))->FUN_00484e30((unsigned int)(s_004d1cc4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f484)))->FUN_00484e30((unsigned int)(s_004d1cac), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f55c)))->FUN_00484e30((unsigned int)(s_004d1c90), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f634)))->FUN_00484e30((unsigned int)(s_004d1c78), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f70c)))->FUN_00484e30((unsigned int)(s_004d1c60), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f7e4)))->FUN_00484e30((unsigned int)(s_004d1c48), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f8bc)))->FUN_00484e30((unsigned int)(s_004d1c2c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f994)))->FUN_00484e30((unsigned int)(s_004d1c14), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080fa6c)))->FUN_00484e30((unsigned int)(s_004d1bfc), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080fb44)))->FUN_00484e30((unsigned int)(s_004d1be4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080dc0c)))->FUN_00484e30((unsigned int)(s_004d1bcc), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080fcf4)))->FUN_00484e30((unsigned int)(s_004d1bb4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080fdcc)))->FUN_00484e30((unsigned int)(s_004d1b9c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080fea4)))->FUN_00484e30((unsigned int)(s_004d1b80), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811ddc)))->FUN_00484e30((unsigned int)(s_004d1b64), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811eb4)))->FUN_00484e30((unsigned int)(s_004d1b48), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080de28)))->FUN_00484e30((unsigned int)(s_004d1b28), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080de94)))->FUN_00484e30((unsigned int)(s_004d1b08), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080dc78)))->FUN_00484e30((unsigned int)(s_004d1aec), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080dd50)))->FUN_00484e30((unsigned int)(s_004d1ad0), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080df00)))->FUN_00484e30((unsigned int)(s_004d1ab4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080dfd8)))->FUN_00484e30((unsigned int)(s_004d1a98), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e0b0)))->FUN_00484e30((unsigned int)(s_004d1a7c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e188)))->FUN_00484e30((unsigned int)(s_004d1a60), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f190)))->FUN_00484e30((unsigned int)(s_004d1a44), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f268)))->FUN_00484e30((unsigned int)(s_004d1a2c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f340)))->FUN_00484e30((unsigned int)(s_004d1a14), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f418)))->FUN_00484e30((unsigned int)(s_004d19f8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f4f0)))->FUN_00484e30((unsigned int)(s_004d19dc), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f5c8)))->FUN_00484e30((unsigned int)(s_004d19c4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f6a0)))->FUN_00484e30((unsigned int)(s_004d19a8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f778)))->FUN_00484e30((unsigned int)(s_004d198c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f850)))->FUN_00484e30((unsigned int)(s_004d1970), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f928)))->FUN_00484e30((unsigned int)(s_004d1954), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080fa00)))->FUN_00484e30((unsigned int)(s_004d193c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080fad8)))->FUN_00484e30((unsigned int)(s_004d1920), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080dba0)))->FUN_00484e30((unsigned int)(s_004d1904), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080fc88)))->FUN_00484e30((unsigned int)(s_004d18e8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080fd60)))->FUN_00484e30((unsigned int)(s_004d18cc), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080fe38)))->FUN_00484e30((unsigned int)(s_004d18b0), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811d70)))->FUN_00484e30((unsigned int)(s_004d1894), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811e48)))->FUN_00484e30((unsigned int)(s_004d1878), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008117f4)))->FUN_00484e30((unsigned int)(s_004d185c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008118cc)))->FUN_00484e30((unsigned int)(s_004d1844), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008119a4)))->FUN_00484e30((unsigned int)(s_004d1828), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811a7c)))->FUN_00484e30((unsigned int)(s_004d1810), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811b54)))->FUN_00484e30((unsigned int)(s_004d17f4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811c2c)))->FUN_00484e30((unsigned int)(s_004d17dc), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811d04)))->FUN_00484e30((unsigned int)(s_004d17c0), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811788)))->FUN_00484e30((unsigned int)(s_004d17a4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811860)))->FUN_00484e30((unsigned int)(s_004d178c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811938)))->FUN_00484e30((unsigned int)(s_004d1770), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811a10)))->FUN_00484e30((unsigned int)(s_004d1754), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811ae8)))->FUN_00484e30((unsigned int)(s_004d1738), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811bc0)))->FUN_00484e30((unsigned int)(s_004d171c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811c98)))->FUN_00484e30((unsigned int)(s_004d1700), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080ff10)))->FUN_00484e30((unsigned int)(s_004d16e4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080ff7c)))->FUN_00484e30((unsigned int)(s_004d16c8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080ffe8)))->FUN_00484e30((unsigned int)(s_004d16ac), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810054)))->FUN_00484e30((unsigned int)(s_004d1690), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008100c0)))->FUN_00484e30((unsigned int)(s_004d166c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0081012c)))->FUN_00484e30((unsigned int)(s_004d164c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810198)))->FUN_00484e30((unsigned int)(s_004d1628), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810204)))->FUN_00484e30((unsigned int)(s_004d1608), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810270)))->FUN_00484e30((unsigned int)(s_004d15ec), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008102dc)))->FUN_00484e30((unsigned int)(s_004d15c4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810348)))->FUN_00484e30((unsigned int)(s_004d159c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008103b4)))->FUN_00484e30((unsigned int)(s_004d1578), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810420)))->FUN_00484e30((unsigned int)(s_004d1558), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0081048c)))->FUN_00484e30((unsigned int)(s_004d1534), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008104f8)))->FUN_00484e30((unsigned int)(s_004d1510), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810564)))->FUN_00484e30((unsigned int)(s_004d14f4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e8b4)))->FUN_00484e30((unsigned int)(s_004d14d8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e920)))->FUN_00484e30((unsigned int)(s_004d14bc), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e9f8)))->FUN_00484e30((unsigned int)(s_004d14a4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080ea64)))->FUN_00484e30((unsigned int)(s_004d1488), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e98c)))->FUN_00484e30((unsigned int)(s_004d146c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e260)))->FUN_00484e30((unsigned int)(s_004d1450), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e2cc)))->FUN_00484e30((unsigned int)(s_004d1438), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e47c)))->FUN_00484e30((unsigned int)(s_004d141c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e4e8)))->FUN_00484e30((unsigned int)(s_004d1400), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e554)))->FUN_00484e30((unsigned int)(s_004d13e4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e5c0)))->FUN_00484e30((unsigned int)(s_004d13c8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e62c)))->FUN_00484e30((unsigned int)(s_004d13ac), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e698)))->FUN_00484e30((unsigned int)(s_004d1394), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e704)))->FUN_00484e30((unsigned int)(s_004d1378), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e770)))->FUN_00484e30((unsigned int)(s_004d135c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e7dc)))->FUN_00484e30((unsigned int)(s_004d1340), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e848)))->FUN_00484e30((unsigned int)(s_004d131c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811494)))->FUN_00484e30((unsigned int)(s_004d12f8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811500)))->FUN_00484e30((unsigned int)(s_004d12d0), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080efe0)))->FUN_00484e30((unsigned int)(s_004d12b4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f04c)))->FUN_00484e30((unsigned int)(s_004d1294), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f0b8)))->FUN_00484e30((unsigned int)(s_004d1270), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080f124)))->FUN_00484e30((unsigned int)(s_004d1254), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00812a84)))->FUN_00484e30((unsigned int)(s_004d1238), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00812af0)))->FUN_00484e30((unsigned int)(s_004d1218), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00812b5c)))->FUN_00484e30((unsigned int)(s_004d11f8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00812bc8)))->FUN_00484e30((unsigned int)(s_004d11dc), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0081306c)))->FUN_00484e30((unsigned int)(s_004d11c4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080ead0)))->FUN_00484e30((unsigned int)(s_004d11ac), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080ed58)))->FUN_00484e30((unsigned int)(s_004d1194), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080edc4)))->FUN_00484e30((unsigned int)(s_004d117c), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080ee30)))->FUN_00484e30((unsigned int)(s_004d1164), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080ee9c)))->FUN_00484e30((unsigned int)(s_004d1140), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810cfc)))->FUN_00484e30((unsigned int)(s_004d112c), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810d68)))->FUN_00484e30((unsigned int)(s_004d1114), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810dd4)))->FUN_00484e30((unsigned int)(s_004d10fc), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810e40)))->FUN_00484e30((unsigned int)(s_004d10e4), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810eac)))->FUN_00484e30((unsigned int)(s_004d10cc), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080eb3c)))->FUN_00484e30((unsigned int)(s_004d10b0), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080eba8)))->FUN_00484e30((unsigned int)(s_004d109c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080ec14)))->FUN_00484e30((unsigned int)(s_004d1084), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080ec80)))->FUN_00484e30((unsigned int)(s_004d106c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080ecec)))->FUN_00484e30((unsigned int)(s_004d104c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0081156c)))->FUN_00484e30((unsigned int)(s_004d1028), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008115d8)))->FUN_00484e30((unsigned int)(s_004d1010), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008116b0)))->FUN_00484e30((unsigned int)(s_004d0ff0), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811644)))->FUN_00484e30((unsigned int)(s_004d0fd4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0081171c)))->FUN_00484e30((unsigned int)(s_004d0fb8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00812a18)))->FUN_00484e30((unsigned int)(s_004d0fa4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810ae0)))->FUN_00484e30((unsigned int)(s_004d0f78), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810b4c)))->FUN_00484e30((unsigned int)(s_004d0f48), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810bb8)))->FUN_00484e30((unsigned int)(s_004d0f18), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_008106a8)))->FUN_00484e30((unsigned int)(s_004d0ef8), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810714)))->FUN_00484e30((unsigned int)(s_004d0ec8), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810780)))->FUN_00484e30((unsigned int)(s_004d0e98), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_008108c4)))->FUN_00484e30((unsigned int)(s_004d0e7c), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810930)))->FUN_00484e30((unsigned int)(s_004d0e50), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_0081099c)))->FUN_00484e30((unsigned int)(s_004d0e28), 0x14);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810f18)))->FUN_00484e30((unsigned int)(s_004d0e08), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810f84)))->FUN_00484e30((unsigned int)(s_004d0de8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00810ff0)))->FUN_00484e30((unsigned int)(s_004d0dc4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0081105c)))->FUN_00484e30((unsigned int)(s_004d0da4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008110c8)))->FUN_00484e30((unsigned int)(s_004d0d84), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811134)))->FUN_00484e30((unsigned int)(s_004d0d60), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008111a0)))->FUN_00484e30((unsigned int)(s_004d0d40), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0081120c)))->FUN_00484e30((unsigned int)(s_004d0d20), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811278)))->FUN_00484e30((unsigned int)(s_004d0cfc), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008112e4)))->FUN_00484e30((unsigned int)(s_004d0ce0), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00811350)))->FUN_00484e30((unsigned int)(s_004d0cc0), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008113bc)))->FUN_00484e30((unsigned int)(s_004d0ca0), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008130d8)))->FUN_00484e30((unsigned int)(s_004d0c7c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00813510)))->FUN_00484e30((unsigned int)(s_004d0c5c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00813948)))->FUN_00484e30((unsigned int)(s_004d0c38), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080df00)))->FUN_00484e30((unsigned int)(s_004d0c14), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00813144)))->FUN_00484e30((unsigned int)(s_004d0bf0), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0081357c)))->FUN_00484e30((unsigned int)(s_004d0bd0), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008139b4)))->FUN_00484e30((unsigned int)(s_004d0bac), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080df6c)))->FUN_00484e30((unsigned int)(s_004d0b88), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008131b0)))->FUN_00484e30((unsigned int)(s_004d0b68), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008135e8)))->FUN_00484e30((unsigned int)(s_004d0b48), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00813a20)))->FUN_00484e30((unsigned int)(s_004d0b24), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080dfd8)))->FUN_00484e30((unsigned int)(s_004d0b00), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0081321c)))->FUN_00484e30((unsigned int)(s_004d0adc), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00813654)))->FUN_00484e30((unsigned int)(s_004d0abc), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00813a8c)))->FUN_00484e30((unsigned int)(s_004d0a98), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e044)))->FUN_00484e30((unsigned int)(s_004d0a74), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008132f4)))->FUN_00484e30((unsigned int)(s_004d0a50), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0081372c)))->FUN_00484e30((unsigned int)(s_004d0a30), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00813b64)))->FUN_00484e30((unsigned int)(s_004d0a0c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e11c)))->FUN_00484e30((unsigned int)(s_004d09e8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00813360)))->FUN_00484e30((unsigned int)(s_004d09c4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00813798)))->FUN_00484e30((unsigned int)(s_004d09a4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00813bd0)))->FUN_00484e30((unsigned int)(s_004d0980), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e188)))->FUN_00484e30((unsigned int)(s_004d095c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008133cc)))->FUN_00484e30((unsigned int)(s_004d0938), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00813804)))->FUN_00484e30((unsigned int)(s_004d0918), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00813c3c)))->FUN_00484e30((unsigned int)(s_004d08f4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e1f4)))->FUN_00484e30((unsigned int)(s_004d08d0), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00813438)))->FUN_00484e30((unsigned int)(s_004d08ac), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00813870)))->FUN_00484e30((unsigned int)(s_004d0888), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00813ca8)))->FUN_00484e30((unsigned int)(s_004d0860), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0080e260)))->FUN_00484e30((unsigned int)(s_004d0838), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008141b8)))->FUN_00484e30((unsigned int)(s_004d0818), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00814224)))->FUN_00484e30((unsigned int)(s_004d07f8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00814290)))->FUN_00484e30((unsigned int)(s_004d07d8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008143d4)))->FUN_00484e30((unsigned int)(s_004d07b8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00814440)))->FUN_00484e30((unsigned int)(s_004d079c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008144ac)))->FUN_00484e30((unsigned int)(s_004d0780), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00814518)))->FUN_00484e30((unsigned int)(s_004d075c), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00814584)))->FUN_00484e30((unsigned int)(s_004d0738), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008145f0)))->FUN_00484e30((unsigned int)(s_004d0718), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0081465c)))->FUN_00484e30((unsigned int)(s_004d06fc), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008146c8)))->FUN_00484e30((unsigned int)(s_004d06e0), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00814734)))->FUN_00484e30((unsigned int)(s_004d06c4), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008147a0)))->FUN_00484e30((unsigned int)(s_004d06a8), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_0081480c)))->FUN_00484e30((unsigned int)(s_004d0688), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00814878)))->FUN_00484e30((unsigned int)(s_004d0668), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008148e4)))->FUN_00484e30((unsigned int)(s_004d0644), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_00814950)))->FUN_00484e30((unsigned int)(s_004d0620), 4);
  ((T_FUN_00484e30 *)(((char *)&DAT_008149bc)))->FUN_00484e30((unsigned int)(s_004d0604), 4);
  return;
}
}

namespace f_0044b180 {
// MATCH: golf_clean.exe 0x0044b180 ?FUN_0044b180@f_0044b180@@YAXXZ
extern int DAT_00821308;
int __fastcall FUN_0044b2c0(int);
void __cdecl FUN_0044b180()
{
  FUN_0044b2c0((int)(((char *)&DAT_00821308)));
  return;
}
}

namespace f_0044b3b0 {
// MATCH: golf_clean.exe 0x0044b3b0 ?FUN_0044b3b0@f_0044b3b0@@YAXXZ
extern int DAT_00821f48;
int __fastcall FUN_0044b450(int);
void __cdecl FUN_0044b3b0()
{
  FUN_0044b450((int)(((char *)&DAT_00821f48)));
  return;
}
}

namespace f_0044b4c0 {
// MATCH: golf_clean.exe 0x0044b4c0 ?FUN_0044b4c0@f_0044b4c0@@YAXXZ
extern int DAT_008224c8;
int __fastcall FUN_0044b5a0(int);
void __cdecl FUN_0044b4c0()
{
  FUN_0044b5a0((int)(((char *)&DAT_008224c8)));
  return;
}
}

namespace f_0044b660 {
// MATCH: golf_clean.exe 0x0044b660 ?FUN_0044b660@f_0044b660@@YAXXZ
extern int DAT_00821120;
int __fastcall FUN_0044b6f0(int);
void __cdecl FUN_0044b660()
{
  FUN_0044b6f0((int)(((char *)&DAT_00821120)));
  return;
}
}

namespace f_00485740 {
// MATCH: golf_clean.exe 0x00485740 ?FUN_00485740@f_00485740@@YAXXZ
extern int DAT_008406e8;
void __stdcall FUN_0047d010();
void __stdcall FUN_00483340();
void __stdcall FUN_00483b10();
void __stdcall FUN_00486ff0();
void __stdcall FUN_00490c30();
void __stdcall FUN_00492470();
undefined4 __fastcall FUN_0049d0e0(int);
void __stdcall FUN_0049d280();
void __stdcall FUN_0049d460();
void __stdcall FUN_0049ead0();
void __stdcall FUN_0049ff30();
void __stdcall FUN_004a0060();
void __stdcall FUN_004a0890();
void __cdecl FUN_00485740()
{
  FUN_00492470();
  FUN_00486ff0();
  FUN_004a0890();
  FUN_004a0890();
  FUN_0049ff30();
  FUN_0049ead0();
  FUN_00483b10();
  FUN_0049d280();
  FUN_0049d460();
  FUN_00490c30();
  FUN_004a0890();
  FUN_0047d010();
  FUN_00483340();
  FUN_0049d0e0((int)(((char *)&DAT_008406e8)));
  FUN_004a0060();
  return;
}
}

namespace f_00487e60 {
// MATCH: golf_clean.exe 0x00487e60 ?FUN_00487e60@f_00487e60@@YAXXZ
extern int DAT_0083b000;
void __fastcall FUN_00487f80(int);
void __cdecl FUN_00487e60()
{
  FUN_00487f80((int)(((char *)&DAT_0083b000)));
  return;
}
}

namespace f_00487e90 {
// MATCH: golf_clean.exe 0x00487e90 ?FUN_00487e90@f_00487e90@@YAXXZ
extern int DAT_0083b000;
undefined4 __fastcall FUN_00488230(int);
void __cdecl FUN_00487e90()
{
  FUN_00488230((int)(((char *)&DAT_0083b000)));
  return;
}
}

namespace f_00495eb0 {
// MATCH: golf_clean.exe 0x00495eb0 ?FUN_00495eb0@f_00495eb0@@YIXH@Z
extern int DAT_0083fe78;
extern int DAT_0083fe7c;
extern int DAT_0083fe80;
extern int DAT_0083fe84;
extern int DAT_0083fe88;
extern int DAT_0083fe8c;
extern int DAT_0083fe90;
extern int DAT_0083fe94;
extern int DAT_0083fe98;
extern int DAT_0083fe9c;
extern int DAT_0083fea0;
extern int DAT_0083fea4;
extern int DAT_0083fea8;
extern int DAT_0083feac;
extern int DAT_0083feb0;
extern int DAT_0083feb4;
extern int DAT_0083feb8;
extern int DAT_0083febc;
extern int DAT_0083fec0;
extern int DAT_0083fec4;
extern int DAT_0083fec8;
extern int DAT_0083fecc;
extern int DAT_0083fed0;
extern int DAT_0083fed4;
extern int DAT_0083fed8;
extern int DAT_0083fedc;
extern int DAT_0083fee0;
extern int DAT_0083fee4;
extern int DAT_0083fee8;
extern int DAT_0083feec;
extern int DAT_0083fef0;
extern int DAT_0083fef4;
extern int DAT_0083fef8;
extern int DAT_0083fefc;
extern int DAT_0083ff00;
extern int DAT_0083ff04;
extern int DAT_0083ff08;
extern int DAT_0083ff0c;
void __fastcall FUN_00495eb0(int param_1)
{
  *(undefined4 *)(param_1 + 4) = DAT_0083fe78;
  *(undefined4 *)(param_1 + 0xc) = DAT_0083fe7c;
  *(undefined4 *)(param_1 + 0x10) = DAT_0083fe80;
  *(undefined4 *)(param_1 + 8) = DAT_0083fe84;
  *(undefined4 *)(param_1 + 0x14) = DAT_0083fe88;
  *(undefined4 *)(param_1 + 0x18) = DAT_0083fe8c;
  *(undefined4 *)(param_1 + 0x1c) = DAT_0083fe90;
  *(undefined4 *)(param_1 + 0x20) = DAT_0083fe94;
  *(undefined4 *)(param_1 + 0x24) = DAT_0083fe98;
  *(undefined4 *)(param_1 + 0x28) = DAT_0083fe9c;
  *(undefined4 *)(param_1 + 0x2c) = DAT_0083fea0;
  *(undefined4 *)(param_1 + 0x30) = DAT_0083fea4;
  *(undefined4 *)(param_1 + 0x34) = DAT_0083fea8;
  *(undefined4 *)(param_1 + 0x38) = DAT_0083feac;
  *(undefined4 *)(param_1 + 0x3c) = DAT_0083feb0;
  *(undefined4 *)(param_1 + 0x40) = DAT_0083feb4;
  *(undefined4 *)(param_1 + 0x44) = DAT_0083feb8;
  *(undefined4 *)(param_1 + 0x48) = DAT_0083febc;
  *(undefined4 *)(param_1 + 0x4c) = DAT_0083fec0;
  *(undefined4 *)(param_1 + 0x50) = DAT_0083fec4;
  *(undefined4 *)(param_1 + 0x54) = DAT_0083fec8;
  *(undefined4 *)(param_1 + 0x58) = DAT_0083fecc;
  *(undefined4 *)(param_1 + 0x5c) = DAT_0083fed0;
  *(undefined4 *)(param_1 + 0x60) = DAT_0083fed4;
  *(undefined4 *)(param_1 + 100) = DAT_0083fed8;
  *(undefined4 *)(param_1 + 0x68) = DAT_0083fedc;
  *(undefined4 *)(param_1 + 0x6c) = DAT_0083fee0;
  *(undefined4 *)(param_1 + 0x70) = DAT_0083fee4;
  *(undefined4 *)(param_1 + 0x74) = DAT_0083fee8;
  *(undefined4 *)(param_1 + 0x78) = DAT_0083feec;
  *(undefined4 *)(param_1 + 0x7c) = DAT_0083fef0;
  *(undefined4 *)(param_1 + 0x80) = DAT_0083fef4;
  *(undefined4 *)(param_1 + 0x84) = DAT_0083fef8;
  *(undefined4 *)(param_1 + 0x88) = DAT_0083fefc;
  *(undefined4 *)(param_1 + 0x8c) = DAT_0083ff00;
  *(undefined4 *)(param_1 + 0x90) = DAT_0083ff04;
  *(undefined4 *)(param_1 + 0x94) = DAT_0083ff08;
  *(undefined4 *)(param_1 + 0x98) = DAT_0083ff0c;
  return;
}
}

namespace f_0049d280 {
// MATCH: golf_clean.exe 0x0049d280 ?FUN_0049d280@f_0049d280@@YAXXZ
extern int DAT_00840710;
void __fastcall FUN_00473ae0(int);
void __cdecl FUN_0049d280()
{
  FUN_00473ae0((int)(((char *)&DAT_00840710)));
  return;
}
}

namespace f_0049ead0 {
// MATCH: golf_clean.exe 0x0049ead0 ?FUN_0049ead0@f_0049ead0@@YAXXZ
extern int DAT_00840830;
extern int DAT_00840860;
extern int DAT_00840890;
void __fastcall FUN_00473ae0(int);
void __cdecl FUN_0049ead0()
{
  FUN_00473ae0((int)(((char *)&DAT_00840860)));
  FUN_00473ae0((int)(((char *)&DAT_00840890)));
  FUN_00473ae0((int)(((char *)&DAT_00840830)));
  return;
}
}

namespace f_0049ff30 {
// MATCH: golf_clean.exe 0x0049ff30 ?FUN_0049ff30@f_0049ff30@@YAXXZ
extern int DAT_008408c8;
extern int DAT_008408f8;
void __fastcall FUN_00473ae0(int);
void __cdecl FUN_0049ff30()
{
  FUN_00473ae0((int)(((char *)&DAT_008408c8)));
  FUN_00473ae0((int)(((char *)&DAT_008408f8)));
  return;
}
}
