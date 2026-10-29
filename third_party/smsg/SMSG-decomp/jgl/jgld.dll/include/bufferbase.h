#ifndef BUFFERBASE_H
#define BUFFERBASE_H

class JGLBuffer;

struct BufferBase {
    BufferBase()
    {
        m_surface = 0;
    }
    virtual int  init_buffer(int w, int h, int bpp, int memType) = 0;
    virtual int  resize_buffer(int cx, int cy, int bpp) = 0;
    virtual void close_buffer() = 0;
    JGLBuffer   *m_surface;
};

#endif
