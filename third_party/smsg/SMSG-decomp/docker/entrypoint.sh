#!/usr/bin/env bash

C=~/.wine/drive_c
T=/simgolf/toolchain

link() { if [ -d "$1" ]; then ln -sfn "$1" "$3"; else ln -sfn "$2" "$3"; fi; }

mkdir -p "$C/VS6SP2/VC98"
link /vc6 "$T/vc6" "$C/Program Files/Microsoft Visual Studio"
link /vc6sp5 "$T/vc6sp5" "$C/VS6SP5"
link /vc6sp2lib "$T/vc6sp2/VC98/lib/lib" "$C/VS6SP2/VC98/LIB"
link /dxsdk /simgolf/sound/DXSDK "$C/DXSDK"
link /clang-format /opt/clang-format "$C/clang-format"
mkdir -p "$C/SimGolf"
ln -sfn /simgolf "$C/SimGolf/decompilation"

R="$T/vc6sp2/VC98/redist"
cp --remove-destination "$R/msvcrt.dll" "$C/windows/system32/msvcrt.dll"
if [ -d /vcsystem ]; then M=/vcsystem/MFC42.DLL; else M="$R/mfc42.dll"; fi
cp --remove-destination "$M" "$C/windows/system32/mfc42.dll"
export WINEDLLOVERRIDES=msvcrt=n

if [ "$EXACT" = "1" ]; then
	mkdir -p "$C/Projects" "$C/SimGolf/external"
	ln -sfn /simgolf/golf "$C/SimGolf/golf"
	ln -sfn /simgolf/terrain/Terrain.dll "$C/Projects/3DTerrainLowPoly"
	ln -sfn /simgolf/jgl/jgld.dll "$C/JackDev"
	ln -sfn /simgolf/jgl/libpng.lib "$C/libpng.lib"
	ln -sfn /simgolf/jgl/zlib.lib "$C/zlib.lib"
	ln -sfn /simgolf/golf/jpeg.lib/jpeg-6a "$C/SimGolf/external/jpeg-6a"
	ln -sfn /simgolf/sound/lpc10.lib/lpc10/lpc10-1.5/lpc55-C "$C/SimGolf/external/lpc10-1.5"
	ln -sfn /simgolf/golf/binkw32.lib "$C/SimGolf/external/bink"
fi

case "$*" in
	"" | terrain | jgl | sound | golf | check | format) ;;
	*) echo "entrypoint.sh: unknown verb '$*' -- one of: terrain jgl sound golf check format, or none for all four" >&2; exit 2 ;;
esac

xvfb-run -a wine cmd /c "set CLANGFORMAT=C:\\clang-format\\clang-format.exe&& set VC6=C:\\Program Files\\Microsoft Visual Studio&& set VC6SP5=C:\\VS6SP5&& set VC6SP2LIB=C:\\VS6SP2\\VC98\\LIB&& set DXSDK=C:\\DXSDK&& set SIMGOLF_FORMAT_FIX=docker run --rm -v PATH-TO-SOURCE:/simgolf:rw simgolf format&& C:\\SimGolf\\decompilation\\build.bat $*"
rc=$?

exit $rc
