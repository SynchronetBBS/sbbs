// $Id: telnet_lib.js,v 1.1 2015/08/31 20:12:41 rswindell Exp $

const IAC	=255	/* 0xff - Interpret as command */
const DONT	=254	/* 0xfe - Don't do option */
const DO	=253	/* 0xfd - Do option */
const WONT 	=252	/* 0xfc - Won't do option */
const WILL 	=251	/* 0xfb - Will do option */

const SB	=250	/* 0xfa - sub-negotiation */
const GA	=249	/* 0xf9 - Go ahead */
const EL	=248 	/* 0xf8 - Erase line */
const EC	=247 	/* 0xf7 - Erase char */
const AYT	=246 	/* 0xf6 - Are you there? */
const AO	=245 	/* 0xf5 - Abort output */
const IP	=244 	/* 0xf4 - Interrupt process */
const BRK	=243 	/* 0xf3 - Break */
const SYNC	=242 	/* 0xf2 - Data mark */
const NOP	=241 	/* 0xf1 - No operation */
const SE	=240 	/* 0xf0 - End of subnegotiation parameters. */


			/* js doesn't like - in variables so must use underscore */


const ECHO	=1	/* 0x01 - echo opcode */
const SGA	=3	/* 0x03 - suppress go ahead */
const TERMINAL_TYPE =24	/* 0x18 - Request terminal type */
const EOR 	=25	/* 0x19 - end of record */
const NAWS	=31	/* 0x1f - NAWS - Negotiate About Window Size */
const NEW_ENVIRON =39	/* 0x27 - NEW-ENVIRON - Exchange environment vars etc. */
const LINEMODE	=42	/* 0x2a - set linemode */
const SLE	=45	/* 0x2d - suppress local echo */
const MSDP	=69	/* 0x45 - MSDP - MUD Server Data Protocl */
const MMSP	=70	/* 0x46 - MMSP - MUD Master Server Protocol */
const MCCP	=85	/* 0x55 - Text compression protocol 1 */
const COMPRESS	=85	/* 	  synonym for MCCP */
const MCCP2	=86	/* 0x56 - Text compression protocol 2 */
const COMPRESS2	=86	/* 	  synonym for MCCP2 */
const MCCP3	=87	/* 0x57 - Text compression protocol 2 */
const COMPRESS3	=87	/* 	  synonym for MCCP3 */
const MSP	=90	/* 0x5a - MSP - MUD Sound Protocol */
const MXP	=91	/* 0x5b - MXP - MUD eXtension Protocol */
const AARD	=102	/* 0x66 - AARD - Proprietary Aardwolf MUD Protocol */
const ATCP	=200	/* 0xc8 - Achaea Telnet Client Protocol */
const GCMP	=201	/* 0xc9 - GCMP - Galois/Counter Mode for Packet */

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
