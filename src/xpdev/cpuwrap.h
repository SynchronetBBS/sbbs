#ifdef __BIG_ENDIAN__
#define htols(hval)	((((ushort)(hval) & 0xff00)>>8)|(((ushort)(hval) & 0x00ff)<<8))
#define ltohs(lval)	((((ushort)(lval) & 0xff00)>>8)|(((ushort)(lval) & 0x00ff)<<8))
#define htoll(hval)	((((ulong)(hval) & 0xff000000)>>24)|(((ulong)(hval) & 0x00ff0000)>>8)|(((ulong)(hval) & 0x0000ff00)<<8)|(((ulong)(hval) & 0x000000ff)<<24))
#define ltohl(lval)	((((ulong)(lval) & 0xff000000)>>24)|(((ulong)(lval) & 0x00ff0000)>>8)|(((ulong)(lval) & 0x0000ff00)<<8)|(((ulong)(lval) & 0x000000ff)<<24))
#else
#define htols(x)	(x)
#define ltohs(x)	(x)
#define htoll(x)	(x)
#define ltohl(x)	(x)
#endif
