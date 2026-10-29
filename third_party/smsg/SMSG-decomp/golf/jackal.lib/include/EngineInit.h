#ifndef ENGINEINIT_H
#define ENGINEINIT_H

#include "TextRenderer.h"

int jackal_init(const char *dll, const char *title, int flags);
void jackal_close();

extern int jackal_flags;

int InitUnusedModule();
void ShutdownUnusedModule();
struct CheckButton {
    static int init_class();
};
void close_cursor_class();

int SetDefaultFont(Font *font);
void ReleaseDefaultFont();
extern Font default_font;

int SetButtonDefaults(Font *font, Font *braceFont, Font *bracketFont);

int init_cursor_class();
extern int g_cursorWidth;
extern int g_cursorHeight;

int LoadSmallSprites();
void ReleaseSmallSprites();

#endif
