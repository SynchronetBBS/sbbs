// $Id: portdefs.js,v 1.7 2019/01/11 01:11:17 rswindell Exp $
// vi: tabstop=4

// Standard Unix service TCP and UDP port numbers

// These are only the default port numbers
// Mainly used for outgoing connections to URIs without a specified port

// Duplicates port numbers for service name aliases are included
// (e.g. both "nttp" and "news")

var standard_service_port = {
	"systat":		11,		// Active Users
	"users":		11,		// Active Users
	"qotd":			17,		// Quote Of The Day
	"msp":			18,		// Message send protocol
	"ftp-data":		20,
	"ftp":			21,
	"ssh": 			22,
	"telnet": 		23,
	"smtp":			25,
	"mail":			25,
	"gopher":		70,
	"finger":		79,
	"http":			80,
	"pop3":			110,
	"auth":			113,
	"ident":		113,
	"nntp":			119,
	"news":			119,
	"imap":			143,
	"imap2":		143,
	"https":		443,	// HTTP/TLS
	"submissions":	465,	// SMTP/TLS
	"login":		513,
	"rlogin":		513,
	"talk":			517,
	"ntalk":		518,
	"nntps":		563,	// NNTP/TLS
	"submission":	587,	// SMTP
	"ftps":			990,	// FTP/TLS
	"telnets":		992,	// Telnet/TLS
	"imaps":		993,	// IMAP/TLS
	"ircs":			994,	// IRC/TLS
	"pop3s":		995,	// POP3/TLS
	"ws":			1123,	// SBBS
	"hotline":		5500,
	"irc":			6667,
	"http-alt":		8080,
	"json":			10088,	// SBBS
	"wss":			11235,	// SBBS
	"binkp":		24554,
};
