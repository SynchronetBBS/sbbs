#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#if defined(_WIN32)
	#ifdef SBBS_EXPORTS
		#define DLLEXPORT	__declspec(dllexport)
	#else
		#define DLLEXPORT	__declspec(dllimport)
	#endif
#endif
#ifndef DLLEXPORT
#define DLLEXPORT
#endif
