"""List object type names with their index (NtQueryObject ObjectTypesInformation) and total objects / handles."""
import ctypes, ctypes.wintypes as W
nt = ctypes.windll.ntdll
class US(ctypes.Structure):
    _fields_ = [("Length", W.USHORT), ("MaximumLength", W.USHORT), ("Buffer", ctypes.c_void_p)]
size = 1 << 20
buf = ctypes.create_string_buffer(size); ret = W.ULONG()
st = nt.NtQueryObject(None, 3, buf, size, ctypes.byref(ret)) & 0xffffffff
print("status", hex(st))
n = ctypes.c_ulong.from_buffer(buf, 0).value
off = ctypes.sizeof(ctypes.c_void_p)
base = ctypes.addressof(buf)
for i in range(n):
    name = US.from_address(base + off)
    s = ctypes.wstring_at(name.Buffer, name.Length // 2)
    tot_obj = ctypes.c_ulong.from_address(base + off + 16).value
    tot_h = ctypes.c_ulong.from_address(base + off + 20).value
    idx = ctypes.c_ubyte.from_address(base + off + 16 + 4 * 22 - 0).value  # placeholder, fixed below
    # OBJECT_TYPE_INFORMATION: TypeName(16) TotalNumberOfObjects TotalNumberOfHandles TotalPagedPoolUsage TotalNonPagedPoolUsage
    # TotalNamePoolUsage TotalHandleTableUsage HighWaterNumberOfObjects HighWaterNumberOfHandles HighWaterPagedPoolUsage
    # HighWaterNonPagedPoolUsage HighWaterNamePoolUsage HighWaterHandleTableUsage InvalidAttributes GENERIC_MAPPING(16) ValidAccessMask
    # SecurityRequired(1) MaintainHandleCount(1) TypeIndex(1) ReservedByte(1) PoolType(4) DefaultPagedPoolCharge(4) DefaultNonPagedPoolCharge(4)
    p = base + off + 16
    vals = [ctypes.c_ulong.from_address(p + 4 * k).value for k in range(13)]
    tidx = ctypes.c_ubyte.from_address(p + 4 * 13 + 16 + 4 + 2).value
    paged = vals[2]
    print(f"{tidx:3} {s:28} objects {vals[0]:8} handles {vals[1]:8} paged {paged/2**20:8.1f} MB nonpaged {vals[3]/2**20:6.1f} MB")
    size_struct = 16 + 4 * 13 + 16 + 4 + 4 + 4 + 4 + 4
    off += size_struct
    off += (name.MaximumLength + 7) & ~7
