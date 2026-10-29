
#include "jgldefs.h"
#include "FLCPalette.h"
#include "WidgetBase.h"
#include "PaletteList.h"
#include "Timer.h"
#include "crt.h"
#include "JackalUtil.h"

FLCPalette g_startupPalette;
FLCPalette g_identityAltPalette;
PaletteList PaletteList::s_ourPalettes;

FLCPalette::FLCPalette()
{
    for (int i = 0; i < 5; i++) {
        m_entries[i].id = -1;
        m_entries[i].timer = 0;
        m_entries[i].first = 0;
        m_entries[i].length = 0;
        m_entries[i].colors = 0;
    }
    pal = 0;
}

FLCPalette::~FLCPalette()
{
    close();
}

FLCPalette &FLCPalette::operator=(const FLCPalette &)
{
    return *this;
}

int FLCPalette::init()
{
    close();
    pal = graphsys->get_palette_data(this);
    pal->init();
    return 0;
}

int FLCPalette::set()
{
    if (pal)
        pal->set();
    return 0;
}

void FLCPalette::close()
{
    if (pal && graphsys) {
        graphsys->delete_palette_data(pal);
        pal = 0;
    }
    for (int i = 0; i < 5; i++) {
        m_entries[i].id = -1;
        if (m_entries[i].timer) {
            delete m_entries[i].timer;
            m_entries[i].timer = 0;
        }
        if (m_entries[i].colors) {
            FreeIfNotNull(m_entries[i].colors);
            m_entries[i].colors = 0;
        }
        m_entries[i].first = 0;
        m_entries[i].length = 0;
    }
}

int FLCPalette::set_from_dib(const Dib *bmi, int first, int count)
{
    unsigned char rgb[256][3];
    if (!bmi)
        return 3;
    if (!pal)
        init();
    for (int i = first; i < first + count; i++) {
        rgb[i - first][0] = bmi->bmiColors[i].rgbRed;
        rgb[i - first][1] = bmi->bmiColors[i].rgbGreen;
        rgb[i - first][2] = bmi->bmiColors[i].rgbBlue;
    }
    return pal->set_entries(&rgb[0][0], first, count);
}

int FLCPalette::init_cycle(int id, int first, int length, int periodMs)
{
    first &= 0xff;
    if (first < 10)
        return 3;
    if (length > 246 - first)
        return 3;
    if (id == -1)
        return 3;
    int i = get_pos(id);
    if (m_entries[i].id == id) {
        if (m_entries[i].timer) {
            delete m_entries[i].timer;
            m_entries[i].timer = 0;
        }
        if (m_entries[i].colors) {
            FreeIfNotNull(m_entries[i].colors);
            m_entries[i].colors = 0;
        }
    }
    m_entries[i].id = id;
    m_entries[i].length = length;
    m_entries[i].first = first;
    m_entries[i].colors = AllocateBytes(length * 4);
    if (!m_entries[i].colors)
        return 4;
    m_entries[i].timer = new Time;
    if (!m_entries[i].timer)
        return 4;
    m_entries[i].timer->SetCallback(&timer_callback, id, this, periodMs, 5);
    return 0;
}

void __cdecl FLCPalette::timer_callback(int, void *)
{
}

int FLCPalette::start_cycle(int id)
{
    int i = get_pos(id);
    if (m_entries[i].id != id)
        return 11;
    m_entries[i].timer->start();
    return 0;
}

void InitIdentityPalette(int)
{
    ReleaseAllPalettes();
    graphsys->init_palette_system(&g_startupPalette, &g_identityAltPalette);
}

void ReleaseAllPalettes()
{
    PaletteList::s_ourPalettes.remove_all();
    if (graphsys)
        graphsys->close_palette_system();
}

int FLCPalette::get_pos(int id)
{
    int i;
    for (i = 0; i < 5; i++) {
        if (id == m_entries[i].id || m_entries[i].id == -1)
            break;
    }
    return i;
}

int FLCPalette::get_nearest_palette_index(unsigned char r, unsigned char g, unsigned char b, int avoidCycled)
{
    int best = 200000;
    int bestIndex;
    unsigned char rgb[768];
    int cycled[256];
    int i;
    unsigned char *p;

    if (!pal)
        return 7;
    pal->get_entries(rgb, 0, 256);
    if (!avoidCycled) {
        i = 0;
        p = &rgb[2];
        for (; i < 256; i++, p += 3) {
            int d = (p[-2] - r) * (p[-2] - r) + (p[-1] - g) * (p[-1] - g) + (p[0] - b) * (p[0] - b);
            if (d < best) {
                best = d;
                bestIndex = i;
            }
        }
    } else {
        for (i = 0; i < 256; i++)
            cycled[i] = 0;
        for (int e = 0; e < 5; e++) {
            if (m_entries[e].id != -1) {
                for (int k = m_entries[e].first; k < m_entries[e].first + m_entries[e].length; k++)
                    cycled[k] = 1;
            }
        }
        p = &rgb[10 * 3 + 2];
        for (i = 10; i < 246; i++, p += 3) {
            if (!cycled[i]) {
                int d = (p[-2] - r) * (p[-2] - r) + (p[-1] - g) * (p[-1] - g) + (p[0] - b) * (p[0] - b);
                if (d < best) {
                    best = d;
                    bestIndex = i;
                }
            }
        }
    }
    return bestIndex;
}

FLCPalette *g_activePalettePtr;
PaletteBase *g_widgetPalette = 0;
static int s_paletteSpare = 0;
FLCPalette *g_textPalette = 0;

int g_savedSpareWord;
