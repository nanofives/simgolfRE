#include "jgldefs.h"
#include "EngineInit.h"
#include "filewin.h"
#include "Text.h"
#include "ListBox.h"
#include "crt.h"
#include "JackalUtil.h"

SpriteObject FileWin::s_subDirSymbol;
SpriteObject FileWin::s_fileSymbol;
SpriteObject FileWin::s_upDirSymbol;

char g_captionText[72];
int g_captionLength = 0;

char *FileWin::filename_text = 0;

int FileWin::prepare_sprites()
{
    AssetSurface sheet;
    int rc;
    if (sheet.load_pcx("jackal.pcx", 0, 10, 0xec, 0) != 0) {
        rc = FileWin::s_subDirSymbol.create_blank(0x10, 0x10);
        if (!rc)
            rc = FileWin::s_fileSymbol.create_blank(0x10, 0x10);
        if (!rc)
            rc = FileWin::s_upDirSymbol.create_blank(0x10, 0x10);
    } else {
        rc = FileWin::s_subDirSymbol.extract(&sheet, 1, 1, 0x10, 0x10, 1, 1);
        if (rc)
            return rc;
        rc = FileWin::s_fileSymbol.extract(&sheet, 0x12, 1, 0x10, 0x10, 1, 1);
        if (rc)
            return rc;
        rc = FileWin::s_upDirSymbol.extract(&sheet, 0x23, 1, 0x10, 0x10, 1, 1);
    }
    if (rc)
        return rc;
    return 0;
}

void FileWin::filebox_draw(GraphicWin *self, int key, int row, int x, int y, int h)
{
    if (!current_listbox)
        return;
    if (graphsys)
        graphsys->set_sprite_scale(1, 1, 1);
    switch (key) {
    case 0:
        FileWin::s_subDirSymbol.BlitScaled(self, FileWin::s_subDirSymbol.m_width / 4 + x,
            (h - FileWin::s_subDirSymbol.m_height) / 2 + y, 0);
        break;
    case 1:
        FileWin::s_fileSymbol.BlitScaled(self, FileWin::s_subDirSymbol.m_width / 4 + x,
            (h - FileWin::s_fileSymbol.m_height) / 2 + y, 0);
        break;
    case 2:
        FileWin::s_upDirSymbol.BlitScaled(self, FileWin::s_subDirSymbol.m_width / 4 + x,
            (h - FileWin::s_upDirSymbol.m_height) / 2 + y, 0);
        break;
    }
}

int FileWin::init_class()
{
    close_class();
    if (text_open("jackal", "FILEWIN"))
        return 6;
    char *line = text_get();
    if (line == 0)
        return 1;
    if (FileWin::filename_text != 0) {
        FreeIfNotNull(FileWin::filename_text);
        FileWin::filename_text = 0;
    }
    FileWin::filename_text = (char *)AllocateBytes(strlen(line) + 1);
    if (FileWin::filename_text == 0)
        return 4;
    ClearString(FileWin::filename_text);
    AppendString(FileWin::filename_text, line);
    return 0;
}

void FileWin::close_class()
{
    if (FileWin::filename_text != 0) {
        FreeIfNotNull(FileWin::filename_text);
        FileWin::filename_text = 0;
    }
}
