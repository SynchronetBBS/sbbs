/* SMBUTIL.H */

#define NOANALYSIS		(1L<<0)

#ifdef __WATCOMC__
	#define ffblk find_t
    #define findfirst(x,y,z) _dos_findfirst(x,z,y)
	#define findnext(x) _dos_findnext(x)
#endif

