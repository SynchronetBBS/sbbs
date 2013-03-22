// ax25defs.js
// AX.25 & KISS protocol-related constants

const AX25_FLAG = (1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5)|(1<<6);	// Unused, but included for non-KISS implementations.

// Address field - SSID subfield bitmasks
const A_CRH		= (1<<7);						// Command/Response or Has-Been-Repeated bit of an SSID octet
const A_RR		= (1<<5)|(1<<6);				// The "R" (reserved) bits of an SSID octet
const A_SSID	= (1<<1)|(1<<2)|(1<<3)|(1<<4);	// The SSID portion of an SSID octet

// Control field bitmasks
const PF = (1<<4);					// Poll/Final
const NR = (1<<5)|(1<<6)|(1<<7);	// N(R) - receive sequence number
const NS = (1<<1)|(1<<2)|(1<<3);	// N(S) - send sequence number
// 	Information frame
const I_FRAME		= 0;
const I_FRAME_MASK	= 1;
// 	Supervisory frame and subtypes
const S_FRAME		= 1;
const S_FRAME_RR	= S_FRAME;			// Receive Ready
const S_FRAME_RNR	= S_FRAME|(1<<2);	// Receive Not Ready
const S_FRAME_REJ	= S_FRAME|(1<<3);	// Reject
const S_FRAME_MASK	= S_FRAME|S_FRAME_RR|S_FRAME_RNR|S_FRAME_REJ
// 	Unnumbered frame and subtypes
const U_FRAME		= 3;
const U_FRAME_SABM	= U_FRAME|(1<<2)|(1<<3)|(1<<5);	// Set Asynchronous Balanced Mode
const U_FRAME_DISC	= U_FRAME|(1<<6);				// Disconnect
const U_FRAME_DM	= U_FRAME|(1<<2)|(1<<3);		// Disconnected Mode
const U_FRAME_UA	= U_FRAME|(1<<5)|(1<<6);		// Acknowledge
const U_FRAME_FRMR	= U_FRAME|(1<<2)|(1<<7);		// Frame Reject
const U_FRAME_UI	= U_FRAME;						// Information
const U_FRAME_MASK	= U_FRAME|U_FRAME_SABM|U_FRAME_DISC|U_FRAME_DM|U_FRAME_UA|U_FRAME_FRMR;

// Protocol ID field bitmasks (most are unlikely to be used, but are here for the sake of completeness.)
const PID_X25		= 1;											// ISO 8208/CCITT X.25 PLP
const PID_CTCPIP	= (1<<1)|(1<<2);								// Compressed TCP/IP packet. Van Jacobson (RFC 1144)
const PID_UCTCPIP	= (1<<0)|(1<<1)|(1<<2);							// Uncompressed TCP/IP packet. Van Jacobson (RFC 1144)
const PID_SEGF		= (1<<4);										// Segmentation fragment
const PID_TEXNET	= (1<<0)|(1<<1)|(1<<6)|(1<<7);					// TEXNET datagram protocol
const PID_LQP		= (1<<2)|(1<<6)|(1<<7);							// Link Quality Protocol
const PID_ATALK		= (1<<1)|(1<<3)|(1<<6)|(1<<7);					// Appletalk
const PID_ATALKARP	= (1<<0)|(1<<1)|(1<<3)|(1<<6)|(1<<7);			// Appletalk ARP
const PID_ARPAIP	= (1<<2)|(1<<3)|(1<<6)|(1<<7);					// ARPA Internet Protocol
const PID_ARPAAR	= (1<<0)|(1<<2)|(1<<3)|(1<<6)|(1<<7);			// ARPA Address Resolution
const PID_FLEXNET	= (1<<1)|(1<<2)|(1<<3)|(1<<6)|(1<<7);			// FlexNet
const PID_NETROM	= (1<<0)|(1<<1)|(1<<2)|(1<<3)|(1<<6)|(1<<7);	// Net/ROM
const PID_NONE		= (1<<4)|(1<<5)|(1<<6)|(1<<7);					// No layer 3 protocol implemented
const PID_ESC		= 255;											// Escape character. Next octet contains more Level 3 protocol information.

// KISS protocol-related constants

// 	FEND and transpositions
const KISS_FEND		= (1<<6)|(1<<7);								// Frame end
const KISS_FESC		= (1<<0)|(1<<1)|(1<<3)|(1<<4)|(1<<6)|(1<<7);	// Frame escape
const KISS_TFEND	= (1<<2)|(1<<3)|(1<<4)|(1<<6)|(1<<7);			// Transposed frame end
const KISS_TFESC	= (1<<0)|(1<<2)|(1<<3)|(1<<4)|(1<<6)|(1<<7);	// Transposed frame escape

// 	Commands
const KISS_DATAFRAME	= 0;	// Data frame
const KISS_TXDELAY	 	= 1;	// TX delay
const KISS_PERSISTENCE	= 2;	// Persistence
const KISS_SLOTTIME		= 3;	// Slot time
const KISS_TXTAIL		= 4;	// TX tail
const KISS_FULLDUPLEX	= 5;	// Full Duplex
const KISS_SETHARDWARE	= 6;	// Set Hardware
const KISS_RETURN		= 255;	// Exit KISS mode