// ax25defs.js
// AX.25 & KISS protocol-related constants

var AX25_FLAG = (1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5)|(1<<6);	// Unused, but included for non-KISS implementations.

// Address field - SSID subfield bitmasks
var A_CRH		= (1<<7);						// Command/Response or Has-Been-Repeated bit of an SSID octet
var A_RR		= (1<<5)|(1<<6);				// The "R" (reserved) bits of an SSID octet
var A_SSID	= (1<<1)|(1<<2)|(1<<3)|(1<<4);	// The SSID portion of an SSID octet

// Control field bitmasks
var PF = (1<<4);					// Poll/Final
var NR = (1<<5)|(1<<6)|(1<<7);	// N(R) - receive sequence number
var NS = (1<<1)|(1<<2)|(1<<3);	// N(S) - send sequence number
// 	Information frame
var I_FRAME		= 0;
var I_FRAME_MASK	= 1;
// 	Supervisory frame and subtypes
var S_FRAME		= 1;
var S_FRAME_RR	= S_FRAME;			// Receive Ready
var S_FRAME_RNR	= S_FRAME|(1<<2);	// Receive Not Ready
var S_FRAME_REJ	= S_FRAME|(1<<3);	// Reject
var S_FRAME_MASK	= S_FRAME|S_FRAME_RR|S_FRAME_RNR|S_FRAME_REJ
// 	Unnumbered frame and subtypes
var U_FRAME		= 3;
var U_FRAME_SABM	= U_FRAME|(1<<2)|(1<<3)|(1<<5);	// Set Asynchronous Balanced Mode
var U_FRAME_DISC	= U_FRAME|(1<<6);				// Disconnect
var U_FRAME_DM	= U_FRAME|(1<<2)|(1<<3);		// Disconnected Mode
var U_FRAME_UA	= U_FRAME|(1<<5)|(1<<6);		// Acknowledge
var U_FRAME_FRMR	= U_FRAME|(1<<2)|(1<<7);		// Frame Reject
var U_FRAME_UI	= U_FRAME;						// Information
var U_FRAME_MASK	= U_FRAME|U_FRAME_SABM|U_FRAME_DISC|U_FRAME_DM|U_FRAME_UA|U_FRAME_FRMR;

// Protocol ID field bitmasks (most are unlikely to be used, but are here for the sake of completeness.)
var PID_X25		= 1;											// ISO 8208/CCITT X.25 PLP
var PID_CTCPIP	= (1<<1)|(1<<2);								// Compressed TCP/IP packet. Van Jacobson (RFC 1144)
var PID_UCTCPIP	= (1<<0)|(1<<1)|(1<<2);							// Uncompressed TCP/IP packet. Van Jacobson (RFC 1144)
var PID_SEGF		= (1<<4);										// Segmentation fragment
var PID_TEXNET	= (1<<0)|(1<<1)|(1<<6)|(1<<7);					// TEXNET datagram protocol
var PID_LQP		= (1<<2)|(1<<6)|(1<<7);							// Link Quality Protocol
var PID_ATALK		= (1<<1)|(1<<3)|(1<<6)|(1<<7);					// Appletalk
var PID_ATALKARP	= (1<<0)|(1<<1)|(1<<3)|(1<<6)|(1<<7);			// Appletalk ARP
var PID_ARPAIP	= (1<<2)|(1<<3)|(1<<6)|(1<<7);					// ARPA Internet Protocol
var PID_ARPAAR	= (1<<0)|(1<<2)|(1<<3)|(1<<6)|(1<<7);			// ARPA Address Resolution
var PID_FLEXNET	= (1<<1)|(1<<2)|(1<<3)|(1<<6)|(1<<7);			// FlexNet
var PID_NETROM	= (1<<0)|(1<<1)|(1<<2)|(1<<3)|(1<<6)|(1<<7);	// Net/ROM
var PID_NONE		= (1<<4)|(1<<5)|(1<<6)|(1<<7);					// No layer 3 protocol implemented
var PID_ESC		= 255;											// Escape character. Next octet contains more Level 3 protocol information.

// KISS protocol-related constants

// 	FEND and transpositions
var KISS_FEND		= (1<<6)|(1<<7);								// Frame end
var KISS_FESC		= (1<<0)|(1<<1)|(1<<3)|(1<<4)|(1<<6)|(1<<7);	// Frame escape
var KISS_TFEND	= (1<<2)|(1<<3)|(1<<4)|(1<<6)|(1<<7);			// Transposed frame end
var KISS_TFESC	= (1<<0)|(1<<2)|(1<<3)|(1<<4)|(1<<6)|(1<<7);	// Transposed frame escape

// 	Commands
var KISS_DATAFRAME	= 0;	// Data frame
var KISS_TXDELAY	 	= 1;	// TX delay
var KISS_PERSISTENCE	= 2;	// Persistence
var KISS_SLOTTIME		= 3;	// Slot time
var KISS_TXTAIL		= 4;	// TX tail
var KISS_FULLDUPLEX	= 5;	// Full Duplex
var KISS_SETHARDWARE	= 6;	// Set Hardware
var KISS_RETURN		= 255;	// Exit KISS mode