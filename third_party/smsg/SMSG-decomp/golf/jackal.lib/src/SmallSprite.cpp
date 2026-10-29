#include "jgldefs.h"
#include "EngineInit.h"
#include "SmallSprite.h"
#include "ImageSet.h"

SpriteObject SmallSprite::s_tile;

int LoadSmallSprites()
{
    AssetSurface sheet;
    int rc;
    if (sheet.load_pcx("jackal.pcx", 0, 10, 0xec, 0) != 0)
        rc = SmallSprite::s_tile.create_blank(0x10, 0x10);
    else
        rc = SmallSprite::s_tile.extract(&sheet, 1, 0x12, 0x10, 0x10, 1, 1);
    if (rc)
        return rc;
    return 0;
}

void ReleaseSmallSprites()
{
    SmallSprite::s_tile.close();
}
