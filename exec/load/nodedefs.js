/* nodedefs.js */

/* Synchronet NODE.DAB constants definitions - (mostly bit-fields) */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2001 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

								/********************************************/
 								/* Legal values for Node.status				*/
								/********************************************/
const NODE_WFC			    =0	/* Waiting for Call							*/
const NODE_LOGON            =1	/* at logon prompt							*/
const NODE_NEWUSER          =2	/* New user applying						*/
const NODE_INUSE            =3	/* In Use									*/
const NODE_QUIET            =4	/* In Use - quiet mode						*/
const NODE_OFFLINE          =5	/* Offline									*/
const NODE_NETTING          =6	/* Networking								*/
const NODE_EVENT_WAITING    =7	/* Waiting for all nodes to be inactive		*/
const NODE_EVENT_RUNNING    =8	/* Running an external event				*/
const NODE_EVENT_LIMBO      =9	/* Allowing another node to run an event	*/		
const NODE_LAST_STATUS	    =10	/* Must be last								*/
								/********************************************/

								/********************************************/
var NodeStatus		=[			/* Node.status value descriptions			*/
	 "Waiting for call"			/********************************************/
	,"At logon prompt"
	,"New user"
	,"In-use"
	,"Waiting for call"
	,"Offline"
	,"Networking"
	,"Waiting for all nodes to become inactive"
	,"Running external event"
	,"Waiting for node %d to finish external event"
	];			

								/********************************************/
								/* Legal values for Node.action				*/
								/********************************************/
const NODE_MAIN			    =0  /* Main Prompt								*/
const NODE_RMSG             =1  /* Reading Messages							*/
const NODE_RMAL             =2  /* Reading Mail								*/
const NODE_SMAL             =3  /* Sending Mail								*/
const NODE_RTXT             =4  /* Reading G-Files							*/
const NODE_RSML             =5  /* Reading Sent Mail						*/
const NODE_PMSG             =6  /* Posting Message							*/
const NODE_AMSG             =7  /* Auto-message								*/
const NODE_XTRN             =8  /* Running External Program					*/
const NODE_DFLT             =9  /* Main Defaults Section					*/
const NODE_XFER             =10 /* Transfer Prompt							*/
const NODE_DLNG             =11 /* Downloading File							*/
const NODE_ULNG             =12 /* Uploading File							*/
const NODE_BXFR             =13 /* Bidirectional Transfer					*/
const NODE_LFIL             =14 /* Listing Files							*/
const NODE_LOGN             =15 /* Logging on								*/
const NODE_LCHT             =16 /* In Local Chat with Sysop					*/
const NODE_MCHT             =17 /* In Multi-Chat with Other Nodes			*/
const NODE_GCHT             =18 /* In Local Chat with Guru					*/
const NODE_CHAT             =19 /* In Chat Section							*/
const NODE_SYSP             =20 /* Sysop Activity							*/
const NODE_TQWK             =21 /* Transferring QWK packet					*/
const NODE_PCHT             =22 /* In Private Chat							*/
const NODE_PAGE             =23 /* Paging another node for Private Chat		*/
const NODE_RFSD             =24 /* Retrieving file from seq dev (aux=dev)	*/
const NODE_LAST_ACTION	    =25	/* Must be last								*/
								/********************************************/

								/********************************************/
var NodeAction		=[			/* Node.action value descriptions			*/
	 "at main menu"				/********************************************/
	,"reading messages"
	,"reading mail"
	,"sending mail"
	,"reading text files"
	,"reading sent mail"
	,"posting message"
	,"posting auto-message"
	,"at external program menu"
	,"changing defaults"
	,"at transfer menu"
	,"downloading"
	,"uploading"
	,"transferring bidirectional"
	,"listing files"
	,"logging on"
	,"in local chat with %s"
	,"in multinode chat"
	,"chatting with %s"
	,"in chat section"
	,"performing sysop activities"
	,"transferring QWK packet"
	,"in private chat with node %u"
	,"paging node %u for private chat"
	,"retrieving from device #%d"
	];

								/********************************************/
                                /* Bit values for Node.misc					*/
								/********************************************/
const NODE_ANON		=(1<<0)     /* Anonymous User							*/
const NODE_LOCK   	=(1<<1)     /* Locked for sysops only					*/
const NODE_INTR   	=(1<<2)     /* Interrupted - hang up					*/
const NODE_MSGW   	=(1<<3)     /* Message is waiting (old way)				*/
const NODE_POFF   	=(1<<4)     /* Page disabled							*/
const NODE_AOFF   	=(1<<5)     /* Activity Alert disabled					*/
const NODE_UDAT   	=(1<<6)     /* User data has been updated				*/
const NODE_RRUN   	=(1<<7)     /* Re-run this node when log off			*/
const NODE_EVENT  	=(1<<8)     /* Must run node event after log off		*/
const NODE_DOWN   	=(1<<9)     /* Down this node after logoff				*/
const NODE_RPCHT  	=(1<<10)    /* Reset private chat						*/
const NODE_NMSG   	=(1<<11)    /* Node message waiting (new way)			*/
const NODE_EXT    	=(1<<12)    /* Extended info on node action				*/
const NODE_LCHAT	=(1<<13)    /* Being pulled into local chat				*/
								/********************************************/
