"""Demo class layouts flattened over their bases (S10.11), from re/tools/cv_debug.py.
Layouts(cv).members(cls) -> [(offset, dotted name, type, owning class)]; .member_at(cls, off) -> the member that
starts at off (None if off is inside a member or no member); .vslots(cls) -> {vtable offset: method name}."""
import collections


class Layouts:
    def __init__(self, cv):
        self.by = {}
        for a in cv.classes():
            old = self.by.get(a['name'])
            if old is None or len(a['members']) + len(a['methods']) > len(old['members']) + len(old['methods']):
                self.by[a['name']] = a

    def members(self, cls, base=0, depth=0, seen=None):
        a = self.by.get(cls)
        if a is None or depth > 8:
            return []
        out = []
        for b in a['bases']:
            if 'offset' in b:
                out += self.members(b['name'], base + b['offset'], depth + 1)
        for m in a['members']:
            out.append((base + m['offset'], m['name'], m['type'], cls))
        return sorted(out)

    def member_at(self, cls, off):
        hits = [m for m in self.members(cls) if m[0] == off]
        # an embedded aggregate and its first member share an offset: keep the outermost (first listed is base-most)
        return hits[-1] if hits else None

    PRIM = {'char': 1, 'uchar': 1, 'int8': 1, 'uint8': 1, 'bool': 1, 'rchar': 1, 'short': 2, 'ushort': 2, 'int16': 2,
            'uint16': 2, 'wchar': 2, 'long': 4, 'ulong': 4, 'int': 4, 'uint': 4, 'float': 4, 'double': 8,
            'int64': 8, 'uint64': 8}

    def size_of(self, t):
        if t.endswith('*'):
            return 4
        if t.endswith(' bytes]'):
            return int(t.rsplit('[', 1)[1].split()[0], 16)
        if t.startswith('enum '):
            return 4
        if t in self.by:
            return self.by[t]['size']
        return self.PRIM.get(t)

    def deep_member_at(self, cls, off, depth=0):
        """(path, type, declaring class) of the member starting at off, descending into embedded aggregates;
        an offset inside an array of aggregates resolves to element [i]. None when nothing starts there."""
        for o, name, t, owner in reversed(self.members(cls)):
            n = self.size_of(t)
            if o == off and (t not in self.by or depth > 6):
                return name, t, owner
            if n and o <= off < o + n and depth < 6:
                if t in self.by:
                    sub = self.deep_member_at(t, off - o, depth + 1)
                    if sub and sub[1] == 'bitfield':          # bit members share the offset: name the word
                        return name, t, owner
                    if sub:
                        return name + '.' + sub[0], sub[1], sub[2]
                    if o == off:
                        return name, t, owner
                elif o == off:
                    return name, t, owner
                return None
        return None

    def deep4(self, cls, off):
        r = self.deep_member_at(cls, off)
        return (off,) + r if r else None

    def vslots(self, cls, depth=0):
        a = self.by.get(cls)
        if a is None or depth > 8:
            return {}
        out = {}
        for b in a['bases']:
            if b.get('offset') == 0:
                out.update(self.vslots(b['name'], depth + 1))
        names = {v: k for k, v in out.items()}
        for m in a['methods']:
            if 'vtable_offset' in m:
                out[m['vtable_offset']] = m['name']
        for m in a['methods']:
            if m['mprop'] in ('virtual', 'pure') and m['name'] in names:
                out[names[m['name']]] = m['name']
        return out
