inline int Sound::is_loaded()
{
    return m_sf.loaded;
}

inline Sound *Sound::get_next()
{
    return m_pnext;
}

inline Sound *Sound::get_prev()
{
    return m_pprev;
}

inline SOUNDTYPE Sound::get_type()
{
    return m_type;
}
