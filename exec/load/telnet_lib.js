// $Id: telnet_lib.js,v 1.1 2015/08/31 20:12:41 rswindell Exp $

var IAC	=255	/* 0xff - Interpret as command */
var DONT	=254	/* 0xfe - Don't do option */
var DO	=253	/* 0xfd - Do option */
var WONT 	=252	/* 0xfc - Won't do option */
var WILL 	=251	/* 0xfb - Will do option */

var SB	=250	/* 0xfa - sub-negotiation */
var GA	=249	/* 0xf9 - Go ahead */
var EL	=248 	/* 0xf8 - Erase line */
var EC	=247 	/* 0xf7 - Erase char */
var AYT	=246 	/* 0xf6 - Are you there? */
var AO	=245 	/* 0xf5 - Abort output */
var IP	=244 	/* 0xf4 - Interrupt process */
var BRK	=243 	/* 0xf3 - Break */
var SYNC	=242 	/* 0xf2 - Data mark */
var NOP	=241 	/* 0xf1 - No operation */
var SE	=240 	/* 0xf0 - End of subnegotiation parameters. */


			/* js doesn't like - in variables so must use underscore */


var ECHO	=1	/* 0x01 - echo opcode */
var SGA	=3	/* 0x03 - suppress go ahead */
var TERMINAL_TYPE =24	/* 0x18 - Request terminal type */
var EOR 	=25	/* 0x19 - end of record */
var NAWS	=31	/* 0x1f - NAWS - Negotiate About Window Size */
var NEW_ENVIRON =39	/* 0x27 - NEW-ENVIRON - Exchange environment vars etc. */
var LINEMODE	=42	/* 0x2a - set linemode */
var SLE	=45	/* 0x2d - suppress local echo */
var MSDP	=69	/* 0x45 - MSDP - MUD Server Data Protocl */
var MMSP	=70	/* 0x46 - MMSP - MUD Master Server Protocol */
var MCCP	=85	/* 0x55 - Text compression protocol 1 */
var COMPRESS	=85	/* 	  synonym for MCCP */
var MCCP2	=86	/* 0x56 - Text compression protocol 2 */
var COMPRESS2	=86	/* 	  synonym for MCCP2 */
var MCCP3	=87	/* 0x57 - Text compression protocol 2 */
var COMPRESS3	=87	/* 	  synonym for MCCP3 */
var MSP	=90	/* 0x5a - MSP - MUD Sound Protocol */
var MXP	=91	/* 0x5b - MXP - MUD eXtension Protocol */
var AARD	=102	/* 0x66 - AARD - Proprietary Aardwolf MUD Protocol */
var ATCP	=200	/* 0xc8 - Achaea Telnet Client Protocol */
var GCMP	=201	/* 0xc9 - GCMP - Galois/Counter Mode for Packet */

function cmdstr(cmd)
{
    switch(cmd) {
        case IAC:	return "IAC";
        case DO:	return "DO";
        case DONT:	return "DONT";
        case WILL:	return "WILL";
        case WONT:	return "WONT";
        case SB:	return "SB";
        case SE:	return "SE";
        case GA:	return "GA";
        case EL:	return "EL";
        case EC:	return "EC";
        case AYT:	return "AYT";
        case AO:	return "AO";
        case IP:	return "IP";
        case BRK:	return "BRK";
        case SYNC:	return "SYNC";
        case NOP:	return "NOP";
        case NAWS:	return "NAWS";
        case MSDP:	return "MSDP";
        case MMSP:	return "MMSP";
        case COMPRESS:	return "COMPRESS";
        case COMPRESS2:	return "COMPRESS2";
        case COMPRESS3:	return "COMPRESS3";
        case MSP:	return "MSP";
        case MXP:	return "MXP";
        case AARD:	return "AARD";
        case GCMP:	return "GCMP";
        case NEW_ENVIRON: return "NEW-ENVIRON";
        case LINEMODE:	 return "LINEMODE";
        case TERMINAL_TYPE:	 return "TERMINAL-TYPE";
        case ECHO:	 return "ECHO";
        case SGA:	 return "SGA";
        case ATCP:	 return "ATCP";
        case MCCP:	 return "MCCP";
        case MCCP2:	 return "MCCP2";
        case MCCP3:	 return "MCCP3";
        case EOR:	 return "EOR";
        case SLE:	 return "SLE";
    }
    return cmd;
}

function ack(cmd)
{
    switch(cmd) {
        case DO:
            return WILL;
        case DONT:
            return WONT;
        case WILL:
            return DO;
        case WONT:
            return DONT;
    }
}

/* Leave as last line for convenient load() usage: */
this;
