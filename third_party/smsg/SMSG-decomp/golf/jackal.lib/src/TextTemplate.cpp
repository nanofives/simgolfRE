#include "jgldefs.h"
#include "crt.h"
#include "AssetSurface.h"
#include "ComboBox.h"
#include "MiscUtil.h"
#include "TextTemplate.h"
#include "BasePop.h"
#include "PullDown.h"
#include "JackalUtil.h"

char tokens[6][20] = {" de ", " le ", " la ", "De ", "Le ", "La "};

int parse_string(char *src, char *out)
{
    char bufLink[80];
    char *outStart;
    char *q;
    int mode;
    int digit;
    char ch[2];

    ch[0] = 0;
    ch[1] = 0;
    if (!src || !out)
        return 3;
    outStart = out;

    while (1) {
        char *p = strstr(src, "$");
        int n;
        if (!p)
            break;
        q = p + 1;
        switch (*q) {

        case 'N':
            if (strncmp(p, "$NUMBER", 7) == 0) {
                digit = p[7] - '0';
                if (digit > 9)
                    return 14;
                n = p - src;
                memcpy(out, src, n);
                out += n;
                p += 8;
                ClearString(out);
                src = p;
                AppendInt(out, parse_numbers[digit]);
                out += strlen(out);
                continue;
            }
            if (strncmp(p, "$NUM", 4) != 0)
                break;
            digit = p[4] - '0';
            if (digit > 9)
                return 14;
            n = p - src;
            memcpy(out, src, n);
            out += n;
            p += 5;
            ClearString(out);
            src = p;
            AppendInt(out, parse_numbers[digit]);
            out += strlen(out);
            continue;

        case 'H':
            if (strncmp(p, "$HEX", 4) != 0)
                break;
            digit = p[4] - '0';
            if (digit > 9)
                return 14;
            n = p - src;
            memcpy(out, src, n);
            out += n;
            p += 5;
            ClearString(out);
            src = p;
            AppendInt(out, parse_numbers[digit]);
            out += strlen(out);
            continue;

        case '$':
            n = p - src;
            memcpy(out, src, n + 1);
            out += n + 1;
            p += 2;
            ClearString(out);
            src = p;
            continue;

        case '<': {
            char *end;
            char c;
            int flag;
            int cnt;
            n = p - src;
            memcpy(out, src, n);
            out += n;
            ClearString(out);
            end = strstr(p, ">");
            if (!end) {
                src = q;
                continue;
            }
            p += 2;
            src = end + 1;
            while (*p == ' ')
                p++;
            c = *p;
            digit = -1;
            flag = 0;
            if (c == 'M' || c == 'm') {
                mode = 0;
            } else if (c == 'F' || c == 'f') {
                mode = 1;
            } else if (c == 'N' || c == 'n') {
                mode = 2;
            } else if (c == '#') {
                digit = *++p - '0';
                flag = (parse_numbers[digit] != 1);
            } else {
                digit = c - '0';
                if (digit >= 10)
                    continue;
                mode = parse_gender[digit];
                flag = parse_plural[digit];
            }
            p++;
            if (digit < 0) {
                char d = *p;
                if (d >= '0' && d <= '9') {
                    flag = d - '1';
                    p++;
                }
            }
            while (*p == ' ')
                p++;
            if (*p == ':')
                p++;
            cnt = 0;
            {
                char *r = p;
                if (p < src) {
                    do {
                        if (*r == ':')
                            cnt++;
                        r++;
                    } while (r < src);
                }
            }
            if (cnt == 0 && findnum(p)) {
                char *t = findnum(p);
                int slotIndex = *t - '0';
                char *u;
                char cc;
                if (slotIndex >= 10)
                    continue;
                p = parse_strings[slotIndex];
                u = p;
                if (*u) {
                    do {
                        if (*u == ':')
                            cnt++;
                        u++;
                    } while (*u);
                }
            }
            if (cnt == 1) {
                cnt = (flag != 0);
            } else if (cnt == 2) {
                cnt = mode;
            } else if (cnt == 3) {
                cnt = (mode != 0) + (flag ? 2 : 0);
            } else if (cnt == 5) {
                cnt = mode + (flag ? 3 : 0);
            }
            while (cnt) {
                if (*p++ == ':')
                    cnt--;
            }
            {
                while (*p != 0 && *p != ':' && *p != '>') {
                    *out = *p;
                    out++;
                    p++;
                }
                *out = 0;
            }
            continue;
        }

        case 'L':
            if (strncmp(p, "$LINK<", 6) == 0) {
                n = p - src;
                memcpy(out, src, n + 1);
                out += n + 1;
                ClearString(out);
                src = q;
                continue;
            }
            if (strncmp(p, "$LINK", 5) != 0)
                break;
            digit = p[5] - '0';
            if (digit >= 10)
                return 14;
            n = p - src;
            memcpy(out, src, n + 5);
            out += n + 5;
            p += 6;
            src = p;
            *out = 0;
            strcat(out, "<");
            strcat(out, g_parse_links[digit].name);
            strcat(out, "=");
            itoa(g_parse_links[digit].value, bufLink, 10);
            strcat(out, bufLink);
            strcat(out, ">");
            out += strlen(out);
            continue;

        case 'D': {
            const char *name;
            if (strncmp(p, "$DROPDOWN", 9) != 0 && strncmp(p, "$DROPLINK", 9) != 0)
                break;
            digit = p[9] - '0';
            if (digit >= 10)
                return 14;
            n = p - src;
            memcpy(out, src, n);
            out += n;
            ch[0] = '[';
            *out = 0;
            strcat(out, ch);
            out++;
            memcpy(out, p, 10);
            out += 10;
            p += 10;
            src = p;
            *out = 0;
            if (g_parse_cb_dropdowns[digit].list) {
                int sel = g_parse_cb_dropdowns[digit].list->m_selId;
                name = g_parse_cb_dropdowns[digit].list->getItemString(sel);
                strcat(out, name);
            } else {
                int sel = g_parse_dropdowns[digit].selId;
                PullDown *table = g_parse_dropdowns[digit].table;
                int r = table->id_to_index(sel);
                if (r < 0x100)
                    name = g_parse_dropdowns[digit].table->m_entries[r].text;
                else
                    name = 0;
                strcat(out, name);
            }
            ch[0] = '^';
            strcat(out, ch);
            ch[0] = ']';
            strcat(out, ch);
            out += strlen(out);
            continue;
        }
        }

        {
            char *t = findnum(p);
            q = t;
            if (!t)
                return 14;
            digit = *t - '0';
            if (digit > 9)
                return 14;
            n = p - src;
            memcpy(out, src, n);
            out += n;
            ClearString(out);
            src = q + 1;
            AppendString(out, parse_strings[digit]);
            out += strlen(out);
        }
    }

    AppendString(out, src);
    if (language == 1) {
        char *best = outStart;
        do {
            char *scan = best;
            int off;
            int i;
            best = 0;
            for (i = 0; i < 6; i++) {
                char *f = strstr(scan, tokens[i]);
                if (f) {
                    if (!best || f < best) {
                        best = f;
                        off = (i < 3) ? 2 : 1;
                    }
                }
            }
            if (!best)
                break;
            {
                char c = best[off + 2];
                if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y' || c == 'h' || c == 'H'
                    || c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'Y') {
                    char *q = best + off;
                    char d;
                    *q = '\'';
                    do {
                        d = q[2];
                        ++q;
                        *q = d;
                    } while (d);
                }
            }
            best++;
        } while (best);
    }
    return 0;
}

int parse_says(int slot, const char *text, int gender, int plural)
{
    if (text == 0 || slot > 9)
        return 3;
    int a = gender;
    if (a < 0)
        a = parsing_gender;
    parse_gender[slot] = a;
    int b = plural;
    if (b < 0)
        b = parsing_plural;
    parse_plural[slot] = b;
    ClearString(parse_strings[slot]);
    AppendString(parse_strings[slot], text);
    return 0;
}

int parse_plural[10] = {0};
static int s_unused1[0xa78 / 4] = {0};
int parse_numbers[10] = {0};
static int s_unused2[0xa28 / 4] = {0};
int parse_gender[10] = {0};
char parse_strings[10][0x100] = {0};
static int s_unused3[0x7c / 4] = {0};
int parsing_gender = 0;
int parsing_plural = 0;
TemplateLink g_parse_links[10] = {0};
ChoiceSlot g_parse_dropdowns[10] = {0};
DropDownSlot g_parse_cb_dropdowns[10] = {0};
static int s_unused4 = 0;
int language = 0;

int g_templateSpare[6] = {0};
