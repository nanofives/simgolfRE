#ifndef FILEWIN_H
#define FILEWIN_H

#include "Sprite.h"

struct FileWin {
    static SpriteObject s_subDirSymbol;
    static SpriteObject s_fileSymbol;
    static SpriteObject s_upDirSymbol;
    static int prepare_sprites();
    static void filebox_draw(GraphicWin *self, int key, int row, int x, int y, int h);
    static int init_class();
    static void close_class();
    static char *filename_text;
};


#endif
