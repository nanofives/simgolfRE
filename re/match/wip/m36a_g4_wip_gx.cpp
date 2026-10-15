// FLAGS sound.dll: /O2 /GX
// m36a group g4 near misses with an EH frame (not evidence: annotated `// WIP:`, never `// MATCH:`). Declarations
// come from the matched file.
#include "../m36a_g4_gx.cpp"

// Misses: `push edi` (out) is at the entry in ours and after the new-expression in the original, and the count++
// of the inlined list append is scheduled above `tail = n` in ours. Merging the success and failure exits into one
// `return r` (or moving the body after `new` into an inline member) moves `push edi` after the new as in the
// original, and count++ then matches too, but the constant 0 is then held in ebx and src in ebp, while the original
// holds 0 in ebp and keeps src on the stack (73.1% by the instruction score, register roles only).
// WIP: sound.dll 0x10012260 ?duplicateBuffer@DirectSoundDevice@@QAEHPAUChannel@@PAUDsBuffer@@PAPAU3@@Z 84.2%
int DirectSoundDevice::duplicateBuffer(Channel *ch, DsBuffer *src, DsBuffer **out)
{
    if (!(flags & 2) || ds == 0)
        return 3;
    if (dupCount == maxBuffers)
        return 0x25;
    *out = new DsBuffer;
    (*out)->owner = this;
    HRES hr = ds->DuplicateSoundBuffer(src->dsb, &(*out)->dsb);
    if (hr == 0) {
        dsListAppend(*out);
        list168.append(*out);
        (*out)->channel = ch;
        ch->registerBuffer(*out);
        if (src->hasAuxInterfaces()) {
            (*out)->query3D();
            (*out)->queryNotify();
        }
        dupCount++;
        return 0;
    }
    mapDsError(hr);
    delete *out;
    *out = 0;
    return 0x1d;
}

// Miss: the original holds the constant 0 in ebp for the whole function (first used by the fmt == 0 test) and
// keeps is3d on the stack only; ours keeps is3d in ebp and builds 0 in ecx where needed. Control flow, the
// count3d arithmetic and the failure path match createBuffer's 100% source shape.
// WIP: sound.dll 0x10012fe0 ?createBufferFmt@Device@m36a_12d10@@QAEHPAPAUDsBuf@2@PAXKK@Z 65.4%
int m36a_12d10::Device::createBufferFmt(DsBuf **out, void *fmt, unsigned long fl, unsigned long size)
{
    if (!(flags & 2))
        return 3;
    if (fmt == 0)
        return 10;
    int r;
    unsigned long is3d = fl & 4;
    if (is3d) {
        unsigned int c = count3d;
        if (c >= max3d)
            return 0x25;
        count3d = ++c;
        unsigned int f = maxAll - c;
        free3d = f;
        if (dupCount > f) {
            DsBuf *b;
            LNode *n;
            for (n = list168.head; n; n = n->next) {
                b = (DsBuf *)n->data;
                if (!b->isBusy())
                    break;
            }
            DsBuf *d;
            if (n == 0 || (b->channel->voiceListRemove(b), (d = (DsBuf *)list168.findAndUnlinkNode(b)) == 0)) {
                count3d--;
                free3d = maxAll - count3d;
                return 0x25;
            }
            remove(d);
            dupCount--;
        }
    }
    *out = new DsBuf;
    (*out)->createFlags = fl;
    (*out)->setBufferSize(size);
    (*out)->setPrimaryFormat(fmt);
    (*out)->owner = this;
    r = (*out)->create(ds, enumArg);
    if (r == 0) {
        addToList(*out);
    } else {
        if (is3d) {
            unsigned int c = count3d - 1;
            free3d = maxAll - c;
            count3d = c;
        }
        delete *out;
        *out = 0;
    }
    return r;
}
