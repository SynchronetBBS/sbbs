// $Id$
// vi: tabstop=4

// Standard Unix service TCP and UDP port numbers

// These are only the default port numbers
// Mainly used for outgoing connections to URIs without a specified port

// Duplicates port numbers for service name aliases are included
// (e.g. both "nttp" and "news")

standard_service_port = {
	"systat":	11,
	"users":	11,
	"qotd":		17,
	"msp":		18,	// Message send protocol
	"ftp-data":	20,
	"ftp":		21,
	"ssh": 		22,
	"telnet": 	23,
	"smtp":		25,
	"mail":		25,
	"gopher":	70,
	"finger":	79,
	"http":		80,
	"pop3":		110,
    "auth":     113,
    "ident":    113,
	"nntp":		119,
	"news":		119,
	"imap":		143,
	"imap2":	143,
	"https":	443,
	"submissions":	465,	// SMTPS
	"login":	513,
	"rlogin":	513,
	"talk":		517,
	"ntalk":	518,
	"nntps":	563,
	"submission":	587,	// SMTP
	"ftps":		990,
	"telnets":	992,
	"imaps":	993,
	"ircs":		994,
	"pop3s":	995,
	"hotline":	5500,
	"irc":		6667,
	"http-alt":	8080,
	"json":		10088,
	"binkp":	24554,
};
