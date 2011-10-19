#ifndef INT64
#ifdef _MSC_VER
#define INT64	__int64
#else
#include <inttypes.h>
#define INT64	int64_t
#endif
#endif

#ifndef INT64FORMAT
#ifdef __BORLANDC__
#define	INT64FORMAT	"Ld"
#else
#ifdef _MSC_VER
#define INT64FORMAT	"I64d"
#else
#define INT64FORMAT	PRId64
#endif
#endif
#endif

#ifndef QWORD
#ifdef _MSC_VER
#define QWORD	unsigned _int64
#else
#define	QWORD	uint64_t
#endif
#endif

#ifndef QWORDFORMAT
#ifdef __BORLANDC__
#define	QWORDFORMAT	"Lu"
#else
#ifdef _MSC_VER
#define 	QWORDFORMAT	"I64u"
#else
#define 	QWORDFORMAT	PRIu64
#endif
#endif
#endif
