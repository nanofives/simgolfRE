#include "jgldefs.h"
#include "AssetSurface.h"
#include "TextRenderer.h"
#include "Sprite.h"

char g_layoutLine[1024];
int g_layoutLineEnd;
int g_layoutHeight;

SpriteObject AssetSurface::s_nullSprite;
int g_layoutSpare = 0;
