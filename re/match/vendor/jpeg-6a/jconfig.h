/* jconfig.h for Microsoft Visual C++ (written for this project: libjpeg 6a ships no jconfig.vc; the settings
   follow jconfig.doc for a flat 32-bit Win32 compiler). */
#define HAVE_PROTOTYPES
#define HAVE_UNSIGNED_CHAR
#define HAVE_UNSIGNED_SHORT
#undef CHAR_IS_UNSIGNED
#define HAVE_STDDEF_H
#define HAVE_STDLIB_H
#undef NEED_BSD_STRINGS
#undef NEED_SYS_TYPES_H
#undef NEED_FAR_POINTERS
#undef NEED_SHORT_EXTERNAL_NAMES
#undef INCOMPLETE_TYPES_BROKEN
/* INLINE functions were inlined: flush_bits 0x4b1b70 contains emit_bits, which VC6 /O2 (/Ob1) only expands
   for functions marked inline. */
#define INLINE __inline
#ifdef JPEG_INTERNALS
#undef RIGHT_SHIFT_IS_UNSIGNED
/* golf_clean.exe was built with the 16-bit allocation limit: alloc_small 0x4aef70 compares with 0xffe0 and
   stores 0xfff0 (65520), not the default 1000000000. */
#define MAX_ALLOC_CHUNK  65520L
#endif
