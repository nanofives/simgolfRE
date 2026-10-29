#include "Screen.h"
#include "InputPoll.h"
#include "jgldefs.h"
#include "crt.h"
#include "Golfer.h"
#include "TextWrap.h"
#include "FloatingText.h"

int g_inputLatched = 1;

int g_pendingKey = 0;
int g_pendingChar = 0;

int TakePendingKey()
{
    int key;
    if (g_pendingKey == 0)
        return 0;
    key = g_pendingKey;
    g_pendingKey = 0;
    return key;
}

int TakePendingChar()
{
    int ch;
    if (g_pendingChar == 0)
        return 0;
    ch = g_pendingChar;
    g_pendingChar = 0;
    return ch;
}

int PollInputState()
{
    if (g_inputLatched) {
        g_inputLatched = 0;
        do_key();
        g_pendingChar = 0;
        g_pendingKey = 0;
    } else {
        do_key();
    }
    do_all_non_input();
    if (g_pendingKey == 0 && g_pendingChar == 0)
        return 0;
    g_inputLatched = 1;
    return 1;
}

void DrainPendingInput()
{
    while (!ProcessInputAndCheckIdle())
        g_pendingKey = g_pendingChar = 0;
    do_all_mouse();
    g_mouseButton = 0;
}

int ProcessInputAndCheckIdle()
{
    do_key();
    do_all_non_input();
    if (g_pendingChar != 0 || g_pendingKey != 0)
        return 0;
    return 1;
}

char *g_textTokens[] = {"$TXT0", "$TXT1", "$TXT2", "$TXT3", "$TXT4", "$KID"};

int WrapText(int widthChars)
{
    char line[512];
    int breaks;
    int lastSpace;
    int trailing;
    int lineStart;
    int i;

    lineStart = 0;
    breaks = 0;
    trailing = 0;
    lastSpace = -1;
    i = 0;
    if (strlen(g_printBuffer) != 0) {
        do {
            if (g_printBuffer[i] == '^') {
                g_printBuffer[i] = ' ';
                g_printBuffer[i + 1] = '\n';
                lineStart = i + 2;
                trailing = -1;
                breaks++;
            }
            if (g_printBuffer[i] == '\n') {
                breaks++;
                trailing = -1;
                lineStart = i + 1;
            }
            if (i > lineStart) {
                strcpy(line, &g_printBuffer[lineStart]);
                line[i - lineStart] = 0;
                if (g_pMenu->text_width(line) > widthChars * 8) {
                    if (lastSpace != -1) {
                        i = lastSpace;
                        g_printBuffer[i] = '\n';
                        breaks++;
                        trailing = 0;
                        lineStart = i + 1;
                        lastSpace = -1;
                    }
                }
            }
            if (g_printBuffer[i] == '_')
                g_printBuffer[i] = ' ';
            if (g_printBuffer[i] == ' ')
                lastSpace = i;
            i++;
        } while (i < strlen(g_printBuffer));
        if (trailing > 0) {
            strcat(g_printBuffer, "\n");
            breaks++;
        }
    }
    return breaks;
}

int DrawTextWordWrapped(int widthChars, int x, int y, unsigned color)
{
    char ch[3] = " ";
    int lineStart = 0;
    int lineWidth = 0;
    int lineHeight;
    int breakAt = 0;
    unsigned i;

    lineHeight = g_pMenu->text_line_height() - 4;
    if (lineHeight < 10)
        lineHeight = 10;

    for (i = 0; i < strlen(g_printBuffer); i++) {
        ch[0] = g_printBuffer[i];
        lineWidth += g_pMenu->text_width(ch);
        if (g_printBuffer[i] == ' ' || g_printBuffer[i] == '\n' || g_printBuffer[i] == '^')
            breakAt = i;
        if (g_printBuffer[i] == '\n' || g_printBuffer[i] == '^' || lineWidth > widthChars * 8) {
            int saved = g_printBuffer[breakAt];
            g_printBuffer[breakAt] = 0;
            if (lineHeight > 16 && color == 0x80007fff)
                DrawTextClampedToScreen(&g_printBuffer[lineStart], x, y, color);
            else
                DrawTextWithColor(&g_printBuffer[lineStart], x, y, color);
            g_printBuffer[breakAt] = (char)saved;
            y += lineHeight;
            lineWidth = 0;
            lineStart = breakAt + 1;
            if (g_printBuffer[i] == '^')
                lineStart++;
            i = breakAt;
            if (y > 0x248)
                break;
        }
    }
    if (lineWidth > 0 && y <= 0x248) {
        if (lineHeight > 16 && color == 0x80007fff)
            DrawTextClampedToScreen(&g_printBuffer[lineStart], x, y, color);
        else
            DrawTextWithColor(&g_printBuffer[lineStart], x, y, color);
        y += lineHeight;
    }
    return y;
}

void ShowTextInputDialog(const char *prompt, int x, int y, int maxLen)
{
    char undo[160];
    char title[128];
    int w;
    int len;

    strcpy(title, prompt);
    strcpy(undo, g_printBuffer);
    len = strlen(g_printBuffer);
    int cursor = len;
    w = maxLen * 12;

    for (;;) {
        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        g_pMenu->fill(x - 1, y - 1, w + 0x20, 0x30, 0x80007fff);
        g_pMenu->fill(x + 1, y + 1, w + 0x20, 0x30, 0x80000000);
        g_pMenu->fill(x, y, w + 0x20, 0x30, 0x800035b3);
        DrawTextWithColor(title, x + 4, y + 6, COLOR_WHITE);
        g_pMenu->fill(x + 0x10, y + 0x17, w, 0x16, 0x80007fff);
        g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
        DrawTextWithColor(g_printBuffer, x + 0x14, y + 0x1c, 0x80000000);

        char atCursor = g_printBuffer[cursor];
        g_printBuffer[cursor] = 0;
        int tw = g_pMenu->text_width(g_printBuffer);
        g_printBuffer[cursor] = atCursor;
        g_simScreen.line(x + tw + 0x14, y + 0x1a, x + tw + 0x14, y + 0x2a, 0x80006000);

        tw = g_pMenu->text_width(g_printBuffer);
        while (tw > w - 0x10) {
            g_printBuffer[strlen(g_printBuffer) - 1] = 0;
            if (cursor > strlen(g_printBuffer))
                cursor = strlen(g_printBuffer);
            tw = g_pMenu->text_width(g_printBuffer);
        }
        g_simScreen.fill(x + tw + 0x15, y + 0x29, 8, 2, 0x80004210);
        g_simScreen.RenderFullSurface(0);

        do {
            WaitWithTimeout(1, 0);
            ProcessMouseMessages();
        } while (g_mouseButton == 0 && PollInputState() == 0);

        int ch = TakePendingChar();
        int vk = TakePendingKey();

        if (ch == 0x1b) {
            g_printBuffer[0] = 0;
            return;
        }
        if (ch == 0x0d) {
            g_printBuffer[len] = 0;
            return;
        }
        if (ch == 8) {
            if (cursor != 0) {
                for (int i = cursor; i <= len; i++)
                    g_printBuffer[i] = g_printBuffer[i + 1];
                cursor--;
                len--;
                g_printBuffer[len] = 0;
            }
        } else if (ch == 0) {
            if (vk == 0x27) {
                if (cursor < len)
                    cursor++;
            } else if (vk == 0x25) {
                if (cursor != 0)
                    cursor--;
            } else if (vk == 0x23) {
                cursor = len;
            } else if (vk == 0x24) {
                cursor = 0;
            } else if (vk == 0x2e) {
                if (cursor < len) {
                    for (int i = cursor; i <= len; i++)
                        g_printBuffer[i] = g_printBuffer[i + 1];
                    len--;
                    g_printBuffer[len] = 0;
                }
            }
        }

        if (cursor < maxLen - 1 && isprint(ch)) {
            for (int i = len; i >= cursor; i--)
                g_printBuffer[i + 1] = g_printBuffer[i];
            g_printBuffer[cursor] = (char)ch;
            cursor++;
            len++;
            g_printBuffer[len] = 0;
            g_printBuffer[maxLen] = 0;
        }
    }
}
