/* sockdefs.js */

/*
 * Constants and structures defined by the internet system,
 * Per RFC 790, September 1981, taken from the BSD file netinet/in.h.
 */

/* $Id$ */

/*
 * Protocols
 */
var IPPROTO_IP              =0               /* dummy for IP */
var IPPROTO_ICMP            =1               /* control message protocol */
var IPPROTO_IGMP            =2               /* internet group management protocol */
var IPPROTO_GGP             =3               /* gateway^2 (deprecated) */
var IPPROTO_TCP             =6               /* tcp */
var IPPROTO_PUP             =12              /* pup */
var IPPROTO_UDP             =17              /* user datagram protocol */
var IPPROTO_IDP             =22              /* xns idp */
var IPPROTO_ND              =77              /* UNOFFICIAL net disk proto */

var IPPROTO_RAW             =255             /* raw IP packet */
var IPPROTO_MAX             =256

/*
 * Port/socket numbers: network standard functions
 */
var IPPORT_ECHO             =7
var IPPORT_DISCARD          =9
var IPPORT_SYSTAT           =11
var IPPORT_DAYTIME          =13
var IPPORT_NETSTAT          =15
var IPPORT_FTP              =21
var IPPORT_TELNET           =23
var IPPORT_SMTP             =25
var IPPORT_TIMESERVER       =37
var IPPORT_NAMESERVER       =42
var IPPORT_WHOIS            =43
var IPPORT_MTP              =57

/*
 * Port/socket numbers: host specific functions
 */
var IPPORT_TFTP             =69
var IPPORT_RJE              =77
var IPPORT_FINGER           =79
var IPPORT_TTYLINK          =87
var IPPORT_SUPDUP           =95

/*
 * UNIX TCP sockets
 */
var IPPORT_EXECSERVER       =512
var IPPORT_LOGINSERVER      =513
var IPPORT_CMDSERVER        =514
var IPPORT_EFSSERVER        =520

/*
 * UNIX UDP sockets
 */
var IPPORT_BIFFUDP          =512
var IPPORT_WHOSERVER        =513
var IPPORT_ROUTESERVER      =520
                                        /* 520+1 also used */

/*
 * Ports < IPPORT_RESERVED are reserved for
 * privileged processes (e.g. root).
 */
var IPPORT_RESERVED         =1024

/*
 * Types
 */
var SOCK_STREAM     =1               /* stream socket */
var SOCK_DGRAM      =2               /* datagram socket */
var SOCK_RAW        =3               /* raw-protocol interface */
var SOCK_RDM        =4               /* reliably-delivered message */
var SOCK_SEQPACKET  =5               /* sequenced packet stream */

/* Option name parameter to Socket.getoption/setoption */
var sockopts = [
	"TYPE",
	"DEBUG",		
	"LINGER", 
	"SNDBUF",		
	"RCVBUF",		
	"SNDLOWAT",	
	"RCVLOWAT",	
	"SNDTIMEO",	
	"RCVTIMEO",
	"KEEPALIVE",
	"REUSEADDR",
	"DONTROUTE",
	"BROADCAST",
	"OOBINLINE",
	"ACCEPTCONN",
	"TCP_NODELAY",
];