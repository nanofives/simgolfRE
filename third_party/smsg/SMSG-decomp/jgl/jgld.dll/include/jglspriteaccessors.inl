inline void JGLSprite::set_palette(JGLPalette *pal)
{
    m_palette = pal;
}

inline JGLPalette *JGLSprite::get_palette()
{
    return m_palette;
}

inline void JGLSprite::set_seethru(int colorKey)
{
    m_colorKey = colorKey;
}

inline int JGLSprite::get_seethru()
{
    return m_colorKey;
}

inline uchar *JGLSprite::get_data()
{
    return m_pixels;
}

inline void JGLSprite::release_data(int nFlag) {}

inline int JGLSprite::get_color_depth()
{
    return m_bpp;
}

inline int JGLSprite::get_pitch()
{
    return m_srcStride;
}

inline int JGLSprite::get_width()
{
    return m_width;
}

inline int JGLSprite::get_height()
{
    return m_height;
}

inline int JGLSprite::IsCompressed()
{
    return m_flags & 1;
}
