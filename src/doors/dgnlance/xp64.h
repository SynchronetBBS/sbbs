#ifndef INT64
#ifdef _MSC_VER
#define INT64	__int64
#else
#define INT64	long long int
#endif
#endif

#ifndef INT64FORMAT
#ifdef __BORLANDC__
#define	INT64FORMAT	"Ld"
#else
#ifdef _MSC_VER
#define INT64FORMAT	"I64d"
#else
#define INT64FORMAT		"lld"
#endif
#endif
#endif

#ifndef QWORD
#ifdef _MSC_VER
#define QWORD	unsigned _int64
#else
#define	QWORD	unsigned long long int
#endif
#endif

#ifndef QWORDFORMAT
#ifdef __BORLANDC__
#define	QWORDFORMAT	"Lu"
#else
#ifdef _MSC_VER
#define 	QWORDFORMAT	"I64u"
#else
#define 	QWORDFORMAT	"llu"
#endif
#endif
#endif
