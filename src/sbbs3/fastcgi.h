#ifndef _FASTCGI_H_
#define _FASTCGI_H_

#if defined(_WIN32) || defined(__BORLANDC__)
    #define PRAGMA_PACK
#endif

#if defined(PRAGMA_PACK) || defined(__WATCOMC__)
    #define _PACK
#else
    #define _PACK __attribute__ ((packed))
#endif

#if defined(PRAGMA_PACK)
    #pragma pack(push,1)            /* Disk image structures must be packed */
#endif

struct fastcgi_header {
	uint8_t		ver;
#define FCGI_VERSION_1           1
	uint8_t		type;
#define FCGI_BEGIN_REQUEST       1
#define FCGI_ABORT_REQUEST       2
#define FCGI_END_REQUEST         3
#define FCGI_PARAMS              4
#define FCGI_STDIN               5
#define FCGI_STDOUT              6
#define FCGI_STDERR              7
#define FCGI_DATA                8
#define FCGI_GET_VALUES          9
#define FCGI_GET_VALUES_RESULT  10
#define FCGI_UNKNOWN_TYPE       11
#define FCGI_MAXTYPE (FCGI_UNKNOWN_TYPE)
	uint16_t	id;
#define FCGI_NULL_REQUEST_ID     0
	uint16_t	len;
	uint8_t		padlen;
	uint8_t		reserved;
} _PACK;

struct fastcgi_message {
	struct fastcgi_header	head;
	char					body[];
} _PACK;

struct fastcgi_begin_request {
	uint16_t	role;
#define FCGI_RESPONDER  1
#define FCGI_AUTHORIZER 2
#define FCGI_FILTER     3
	uint8_t		flags;
#define FCGI_KEEP_CONN  1
	uint8_t		reserved[5];
} _PACK;

struct fastcgi_end_request {
	uint32_t	status;
	uint8_t		pstatus;
#define FCGI_REQUEST_COMPLETE 0
#define FCGI_CANT_MPX_CONN    1
#define FCGI_OVERLOADED       2
#define FCGI_UNKNOWN_ROLE     3
	uint8_t		reserved[3];
} _PACK;

#if defined(PRAGMA_PACK)
#pragma pack(pop)       /* original packing */
#endif

#endif
