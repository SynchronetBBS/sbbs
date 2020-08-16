// $Id: telnet_lib.js,v 1.1 2015/08/31 20:12:41 rswindell Exp $

const IAC	=255	/* 0xff - Interpret as command */
const DONT	=254	/* 0xfe - Don't do option */
const DO   	=253	/* 0xfd - Do option */
const WONT 	=252	/* 0xfc - Won't do option */
const WILL 	=251	/* 0xfb - Will do option */

const SB    =250	/* 0xfa - sub-negotiation */
const GA	=249	/* 0xf9 - Go ahead */
const EL	=248 	/* 0xf8 - Erase line */
const EC	=247 	/* 0xf7 - Erase char */
const AYT	=246 	/* 0xf6 - Are you there? */
const AO	=245 	/* 0xf5 - Abort output */
const IP	=244 	/* 0xf4 - Interrupt process */
const BRK	=243 	/* 0xf3 - Break */
const SYNC	=242 	/* 0xf2 - Data mark */
const NOP	=241 	/* 0xf1 - No operation */

const SE    =240 	/* 0xf0 - End of subnegotiation parameters. */

function cmdstr(cmd)
{
    switch(cmd) {
        case IAC:   return "IAC";
        case DO:    return "DO";
        case DONT:  return "DONT";
        case WILL:  return "WILL";
        case WONT:  return "WONT";
        case SB:    return "SB";
        case GA:    return "GA";
        case EL:    return "EL";
        case EC:    return "EC";
        case AYT:   return "AYT";
        case AO:    return "AO";
        case IP:    return "IP";
        case BRK:   return "BRK";
        case SYNC:  return "SYNC";
        case NOP:   return "NOP";
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