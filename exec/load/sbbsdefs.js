/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

require('nodedefs.js', 'NODE_WFC');
require('smbdefs.js', 'SMB_SUCCESS');
require('userdefs.js', 'USER_DELETED');
require('cga_defs.js', 'BLACK');
require('key_defs.js', 'KEY_UP');

/* Would rather use const than var, but end up with redeclaration errors.	*/

							    /********************************************/
							    /* system.settings							*/
							    /********************************************/
var   SYS_CLOSED	=(1<<0); 	/* System is closed to New Users		    */
var   SYS_SYSSTAT	=(1<<1); 	/* Sysops activity included in statistics	*/
var   SYS_NOSYSINFO	=(1<<2); 	/* Suppress system info display at logon	*/
var   SYS_PWEDIT	=(1<<3); 	/* Allow users to change their passwords	*/
var   SYS_RA_EMU	=(1<<4); 	/* Reverse R/A commands at msg read prompt	*/
var   SYS_ANON_EM	=(1<<5); 	/* Allow anonymous e-mail					*/
var   SYS_LISTLOC	=(1<<6); 	/* Use location of caller in user lists 	*/
var   SYS_WILDCAT	=(1<<7); 	/* Expand Wildcat color codes in messages	*/
var   SYS_PCBOARD	=(1<<8); 	/* Expand PCBoard color codes in messages	*/
var   SYS_WWIV 		=(1<<9); 	/* Expand WWIV color codes in messages		*/
var   SYS_CELERITY	=(1<<10);	/* Expand Celerity color codes in messages	*/
var   SYS_RENEGADE	=(1<<11);	/* Expand Renegade color codes in messages	*/
var   SYS_ECHO_PW	=(1<<12);	/* Echo passwords locally					*/
var   SYS_AUTO_DST	=(1<<14);	/* Automatic Daylight Savings Toggle (US)	*/
var   SYS_R_SYSOP	=(1<<15);	/* Allow remote sysop logon/commands		*/
var   SYS_QUOTE_EM	=(1<<16);	/* Allow quoting of e-mail					*/
var   SYS_EURODATE	=(1<<17);	/* European date format (DD/MM/YY)			*/
var   SYS_MILITARY	=(1<<18);	/* Military time format 					*/
var   SYS_TIMEBANK	=(1<<19);	/* Allow time bank functions				*/
var   SYS_FILE_EM	=(1<<20);	/* Allow file attachments in E-mail 		*/
var   SYS_SHRTPAGE	=(1<<21);	/* Short sysop page 						*/
var   SYS_TIME_EXP	=(1<<22);	/* Set to expired values if out-of-time 	*/
var   SYS_FASTMAIL	=(1<<23);	/* Fast e-mail storage mode 				*/
var   SYS_NONODELIST=(1<<24);	/* Suppress active node list during logon	*/
var   SYS_MOUSE_HOT	=(1<<25);	/* Mouse Hot-spots in menus and prompts		*/
var   SYS_FWDTONET	=(1<<26);	/* Allow forwarding of e-mail to netmail	*/
var   SYS_DELREADM	=(1<<27);	/* Delete read mail automatically			*/
var   SYS_NOCDTCVT	=(1<<28);	/* No credit to minute conversions allowed	*/
var   SYS_DELEMAIL	=(1<<29);	/* Physically remove deleted e-mail immed.	*/
var   SYS_USRVDELM	=(1<<30);	/* Users can see deleted msgs				*/
var   SYS_SYSVDELM	=(1<<31);	/* Sysops can see deleted msgs				*/
					    		/********************************************/

								// system.login_settings
var	LOGIN_USERNUM	=(1<<0);	// Allow logins by user number
var	LOGIN_REALNAME	=(1<<1);	// Allow logins by user's real name
var	LOGIN_PWPROMPT	=(1<<2);	// Always display password prompt, even for bad login-id

						    	/********************************************/
    							/* bbs.sys_status							*/
							    /********************************************/
var   SS_TMPSYSOP	=(1<<2);	/* Temporary Sysop Status					*/
var   SS_USERON		=(1<<3);	/* A User is logged on to the BBS			*/
var   SS_LCHAT		=(1<<4);	/* Local chat in progress					*/
var   SS_DAILY		=(1<<9);	/* Execute System Daily Event on logoff 	*/
var   SS_INUEDIT	=(1<<10);	/* Inside Alt-Useredit section 				*/
var   SS_ABORT		=(1<<11);	/* Global abort input or output flag		*/
var   SS_SYSPAGE	=(1<<12);	/* Paging sysop								*/
var   SS_GURUCHAT	=(1<<14);	/* Guru chat in progress					*/
var   SS_EVENT		=(1<<16);	/* Time shortened due to upcoming event		*/
var   SS_PAUSEON	=(1<<17);	/* Pause on, overriding user default		*/
var   SS_PAUSEOFF	=(1<<18);	/* Pause off, overriding user default		*/
var   SS_IN_CTRLP	=(1<<19);	/* Inside ctrl-p send node message func		*/
var   SS_NEWUSER	=(1<<20);	/* New User online 							*/
var   SS_NEST_PF	=(1<<22);	/* Nested in printfile function				*/
var   SS_SPLITP		=(1<<24);	/* Split-screen private chat				*/
var   SS_NEWDAY		=(1<<25);	/* Date changed while online				*/
var   SS_RLOGIN		=(1<<26);	/* Current login via BSD RLogin				*/
var   SS_FILEXFER	=(1<<27);	/* File transfer in progress, halt spy		*/
var   SS_SSH		=(1<<28);	/* Current login via Secure Shell (SSH)     */
var   SS_MOFF		=(1<<29);	/* Disable node/time messages				*/
var   SS_QWKLOGON   =(1<<30);	/* QWK logon 								*/
var   SS_FASTLOGON  =(1<<31);	/* Fast logon                               */
					    		/********************************************/

						    	/********************************************/
								/* bbs.startup_options						*/
						    	/********************************************/
var   BBS_OPT_XTRN_MINIMIZED	=(1<<1); /* Run externals minimized			*/
var   BBS_OPT_AUTO_LOGON		=(1<<2); /* Auto-logon via IP				*/
var   BBS_OPT_DEBUG_TELNET		=(1<<3); /* Debug telnet commands			*/
var   BBS_OPT_ALLOW_RLOGIN		=(1<<5); /* Allow logins via BSD RLogin		*/
var   BBS_OPT_NO_QWK_EVENTS		=(1<<7); /* Don't run QWK-related events	*/
var   BBS_OPT_NO_TELNET_GA		=(1<<8); /* Don't send periodic Telnet GAs	*/
var   BBS_OPT_NO_EVENTS			=(1<<9); /* Don't run event thread			*/
var   BBS_OPT_NO_SPY_SOCKETS	=(1<<10);/* Don't create spy sockets		*/
var   BBS_OPT_NO_HOST_LOOKUP	=(1<<11);/* Don't lookup hostname			*/
var   BBS_OPT_ALLOW_SSH			=(1<<12);/* Allow logins via BSD SSH		*/
var   BBS_OPT_NO_DOS			=(1<<13);/* Can't to run 16-bit DOS prog	*/
var   BBS_OPT_NO_NEWDAY_EVENTS	=(1<<14);/* Don't check for a new day 		*/
var   BBS_OPT_NO_TELNET			=(1<<15);/* Don't accept incoming telnet	*/
var   BBS_OPT_HAPROXY_PROTO   	=(1<<26);/* Incoming requests via HAproxy 	*/
var   BBS_OPT_NO_RECYCLE		=(1<<27);/* Disable recycling of server		*/
var   BBS_OPT_GET_IDENT			=(1<<28);/* Get Identity (RFC 1413)			*/
								/********************************************/

						    	/********************************************/
								/* bbs.online								*/
						    	/********************************************/
var   ON_LOCAL		=1;	 		/* Online locally (events only in v3)		*/
var   ON_REMOTE		=2;			/* Online remotely							*/
						    	/********************************************/

							    /********************************************/
							    /* console.status							*/
							    /********************************************/
var CON_PASSWORD	=(1<<1);	// Password input mode, e.g. echo *'s
var CON_PAUSE		=(1<<4);	// Temporary pause over-ride (same as UPAUSE)
var CON_RAW_IN   	=(1<<8);	// Raw input mode - no editing capabilities
var CON_ECHO_OFF 	=(1<<10);	// Output disabled
var CON_UPARROW  	=(1<<11);	// Up arrow hit - move up one line
var CON_DOWNARROW 	=(1<<12);	// Down arrow hit - from getstr()
var CON_NO_INACT 	=(1<<13);	// User inactivity detection disabled
var CON_BACKSPACE 	=(1<<14);	// Backspace key - from getstr(K_LEFTEXIT)
var CON_LEFTARROW 	=(1<<15);	// Left arrow hit - from getstr(K_LEFTEXIT)
var CON_INSERT		=(1<<16);	// Insert mode - for use with getstr()
var CON_DELETELINE	=(1<<17);	// Deleted line - from getstr(K_LEFTEXIT)
var CON_NORM_FONT	=(1<<18);	// Alt normal font activated
var CON_HIGH_FONT	=(1<<19);	// Alt high-intensity font activated
var CON_BLINK_FONT	=(1<<20);	// Alt blink font activated
var CON_HBLINK_FONT	=(1<<21);	// Alt high-blink font activated
var CON_MOUSE_CLK_PASSTHRU	=(1<<24); // Pass-through unhandled mouse button-click reports
var CON_MOUSE_REL_PASSTHRU	=(1<<25); // Pass-through unhandled mouse button-release reports
var CON_MOUSE_SCROLL		=(1<<26); // Enable mouse scroll-wheel to arrow key translations
var CON_CR_CLREOL			=(1<<31); // Sending '\r', clears to end-of-line first
var CON_R_ECHOX     = CON_PASSWORD; // Legacy
var CON_L_ECHOX     = 0;            // Legacy

								// Terminal mouse reporting mode (console.mouse_mode)
var MOUSE_MODE_OFF	= 0;		// No terminal mouse reporting enabled/expected
var MOUSE_MODE_X10	= (1<<0);	// X10 compatible mouse reporting enabled
var MOUSE_MODE_NORM	= (1<<1);	// Normal tracking mode mouse reporting
var MOUSE_MODE_BTN	= (1<<2);	// Button-event tracking mode mouse reporting
var MOUSE_MODE_ANY	= (1<<3);	// Any-event tracking mode mouse reporting
var MOUSE_MODE_EXT	= (1<<4);	// SGR-encoded extended coordinate mouse reporting

						    	/********************************************/
    							/* Bits in 'mode' for getkey and getstr     */
							    /********************************************/
var   K_NONE		=0;			/* No special behavior						*/
var   K_UPPER 		=(1<<0);	/* Converts all letters to upper case		*/
var   K_UPRLWR		=(1<<1);	/* Upper/Lower case automatically			*/
var   K_NUMBER		=(1<<2);	/* Allow numbers only						*/
var   K_WORDWRAP	=(1<<3);	/* Allows word wrap 						*/
var   K_MSG			=(1<<4);	/* Allows ANSI, ^N ^A ^G					*/
var   K_SPIN		=(1<<5);	/* Spinning cursor (same as SPIN)			*/
var   K_LINE		=(1<<6);	/* Input line (inverse color)				*/
var   K_EDIT		=(1<<7);	/* Edit string passed						*/
var   K_CHAT		=(1<<8);	/* In chat multi-chat						*/
var   K_NOCRLF		=(1<<9);	/* Don't print CRLF after string input      */
var   K_ALPHA 		=(1<<10);	/* Only allow alphabetic characters 		*/
var   K_GETSTR		=(1<<11);	/* getkey called from getstr()				*/
var   K_LOWPRIO		=(1<<12);	/* low priority input						*/
var   K_NOEXASC		=(1<<13);	/* No extended ASCII allowed				*/
var   K_E71DETECT	=(1<<14);	/* Detect E-7-1 terminal type				*/
var   K_AUTODEL		=(1<<15);	/* Auto-delete text (used with K_EDIT)		*/
var   K_COLD		=(1<<16);	/* Possible cold key mode					*/
var   K_NOECHO		=(1<<17);	/* Don't echo input                         */
var   K_TAB			=(1<<18);	/* Treat TAB key as CR						*/
var	  K_LEFTEXIT	=(1<<19);	/* Allow exit from getstr() with backspace	*/
var   K_USEOFFSET	=(1<<20);	/* Use console.getstr_offset with getstr()	*/
var   K_NOSPIN      =(1<<21);	/* Do not honor user's spinning cursor		*/
var   K_ANSI_CPR	=(1<<22);	/* ANSI Cursor Position Report expected		*/
var   K_TRIM        =(1<<23);   /* Trim white-space from both ends of str   */
var   K_CTRLKEYS	=(1<<24);	/* No control-key handling/eating in inkey()*/
var   K_NUL         =(1<<25);   /* Return null instead of "" upon timeout   */
var   K_UTF8		=(1<<26);	/* Don't translate UTF-8 input to CP437 	*/
var   K_RIGHTEXIT   =(1<<27);   /* Allow exit by arrowing right				*/
var   K_LINEWRAP    =(1<<29);   /* Allow string input to wrap the terminal  */
var   K_EXTKEYS     =(1<<30);   /* Like K_CTRLKEYS but w/extended key xlats */
					    		/********************************************/
var   K_WRAP = K_WORDWRAP;

						    	/********************************************/
    							/* Bits in 'mode' for putmsg and printfile  */
							    /********************************************/
var   P_NONE		=0;			/* No special behavior						*/
var   P_NOABORT  	=(1<<0);	/* Disallows abortion of a message          */
var   P_SAVEATR		=(1<<1);	/* Save the new current attributes after	*/
					    		/* msg has printed							*/
var   P_NOATCODES	=(1<<2);	/* Don't allow @ codes                      */
var   P_OPENCLOSE	=(1<<3);	/* Open and close the file					*/
var   P_NOPAUSE		=(1<<4);	/* Disable screen pause						*/
var   P_HTML		=(1<<5);	/* Message is HTML							*/
var   P_NOCRLF		=(1<<6);	/* Don't prepend a CRLF	in printfile()		*/
var   P_WORDWRAP	=(1<<7);	/* Word-wrap long lines for user's terminal	*/
var   P_CPM_EOF		=(1<<8);	/* Treat Ctrl-Z as End-of-file				*/
var   P_TRUNCATE    =(1<<9);    /* Truncate (don't display) long lines      */
var   P_NOERROR     =(1<<10);   /* Don't report error if file doesn't exist */
var   P_PETSCII     =(1<<11);   /* Message is native PETSCII                */
var   P_WRAP        =(1<<12);   /* Wrap/split long-lines, ungracefully      */
var   P_UTF8        =(1<<13);	/* Message is UTF-8 encoded                 */
var   P_AUTO_UTF8	=(1<<14);	/* Message may be UTF-8, auto-detect		*/
var   P_NOXATTRS	=(1<<15);	/* No "Extra Attribute Codes" supported		*/
var   P_MARKUP		=(1<<16);	/* Support StyleCodes/Rich/StructuredText	*/
var   P_HIDEMARKS	=(1<<17);	/* Hide the mark-up tags					*/
var   P_REMOTE		=(1<<18);	/* Only print when online == ON_REMOTE		*/
var   P_INDENT		=(1<<19);	/* Indent lines to current cursor column	*/
var   P_ATCODES		=(1<<20);	/* Trusted @-codes in formatted string		*/
var   P_MODS        =(1<<21);   // Display from mods/text dir, if file is there
var   P_CENTER      =(1<<22);   // Center the output based on widest line
var   P_80COLS      =(1<<23);   // Format the output for 80-column display
var   P_WILDCAT     =(1<<27);   // Support Wildcat @xx@ color codes
var   P_PCBOARD     =(1<<28);   // Support PCBoard @Xxx color codes
var   P_WWIV        =(1<<29);   // Support WWIV (^C) color codes
var   P_CELERITY    =(1<<30);   // Support Celerity (|x) color codes
var   P_Renegade    =(1<<31);   // Support Renegade (|xx) color codes
							    /********************************************/

    							/********************************************/
							    /* system.new_user_questions				*/
							    /********************************************/
var   UQ_ALIASES	=(1<<0);	/* Ask for alias							*/
var   UQ_LOCATION	=(1<<1);	/* Ask for location 						*/
var   UQ_ADDRESS	=(1<<2);	/* Ask for address							*/
var   UQ_PHONE		=(1<<3);	/* Ask for phone number 					*/
var   UQ_HANDLE		=(1<<4);	/* Ask for chat handle						*/
var   UQ_DUPHAND	=(1<<5);	/* Search for duplicate handles 			*/
var   UQ_SEX		=(1<<6);	/* Ask for sex								*/
var   UQ_BIRTH		=(1<<7);	/* Ask for birth date						*/
var   UQ_REALNAME	=(1<<10);	/* Ask for real name						*/
var   UQ_DUPREAL	=(1<<11);	/* Search for duplicate real names			*/
var   UQ_COMPANY	=(1<<12);	/* Ask for company name 					*/
var   UQ_NOEXASC	=(1<<13);	/* Don't allow ex-ASCII in user text		*/
var   UQ_CMDSHELL	=(1<<14);	/* Ask for command shell					*/
var   UQ_XEDIT		=(1<<15);	/* Ask for external editor					*/
var   UQ_NODEF		=(1<<16);	/* Don't ask for default settings			*/
var   UQ_NOCOMMAS	=(1<<17);	/* Do not require commas in location		*/
var   UQ_NONETMAIL	=(1<<18);	/* Don't ask for e-mail/netmail address		*/
var   UQ_NOUPRLWR   =(1<<19);   /* Don't force upper/lower case strings		*/
var   UQ_COLORTERM  =(1<<20);   /* Ask if new user has color terminal	    */
var   UQ_DUPNETMAIL =(1<<21);	/* Don't allow duplicate netmail address    */
					    		/********************************************/

							    /********************************************/
							    /* bbs.node_settings						*/
							    /********************************************/
var   NM_ANSALARM	=(1<<0);	/* Alarm locally on answer					*/
var   NM_WFCSCRN	=(1<<1);	/* Wait for call screen                     */
var   NM_WFCMSGS	=(1<<2);	/* Include total messages/files on WFC		*/
var   NM_LCL_EDIT	=(1<<3);	/* Use local editor to create messages		*/
var   NM_EMSOVL		=(1<<4);	/* Use expanded memory of overlays			*/
var   NM_WINOS2		=(1<<5);	/* Use Windows/OS2 time slice API call		*/
var   NM_INT28		=(1<<6);	/* Make int 28 DOS idle calls				*/
var   NM_NODV 		=(1<<7);	/* Don't detect and use DESQview API        */
var   NM_NO_NUM		=(1<<8);	/* Don't allow logons by user number        */
var   NM_LOGON_R	=(1<<9);	/* Allow logons by user real name			*/
var   NM_LOGON_P	=(1<<10);	/* Secure logons (always ask for password)	*/
var   NM_NO_LKBRD	=(1<<11);	/* No local keyboard (at all)				*/
var   NM_SYSPW		=(1<<12);	/* Protect WFC keys and Alt keys with SY:	*/
var   NM_NO_INACT	=(1<<13);	/* No local inactivity alert/logoff 		*/
var   NM_NOBEEP		=(1<<14);	/* Don't beep locally                       */
var   NM_LOWPRIO	=(1<<15);	/* Always use low priority input			*/
var   NM_7BITONLY	=(1<<16);	/* Except 7-bit input only (E71 terminals)	*/
var   NM_RESETVID	=(1<<17);	/* Reset video mode between callers?		*/
var   NM_NOPAUSESPIN=(1<<18);	/* No spinning cursor at pause prompt		*/
					    		/********************************************/

						    	/********************************************/
							    /* msg_area.[fido|inet]_netmail_settings	*/
							    /********************************************/
var   NMAIL_ALLOW 	=(1<<0);	/* Allow users to send NetMail				*/
var   NMAIL_CRASH 	=(1<<1);	/* Default Fido netmail to crash			*/
var   NMAIL_HOLD	=(1<<2);	/* Default Fido netmail to hold				*/
var   NMAIL_KILL	=(1<<3);	/* Default Fido netmail to kill after sent	*/
var   NMAIL_ALIAS 	=(1<<4);	/* Use Aliases when sending NetMail			*/
var   NMAIL_FILE	=(1<<5);	/* Allow file attachments in sent NetMail	*/
var   NMAIL_DIRECT	=(1<<6);	/* Default Fido netmail to direct			*/
var   NMAIL_CHSRCADDR = (1<<7);	/* Allow sender to choose source address 	*/
					    		/********************************************/

    							/********************************************/
							    /* Bit values for sub[x].settings			*/
							    /********************************************/
var   SUB_NOVOTING	=(1<<0);	/* No voting allowed in this sub-board		*/
var   SUB_TEMPLATE  =(1<<1);	/* Use this sub as template for new subs    */
var   SUB_MSGTAGS   =(1<<2);    /* Allow messages to be tagged              */
var   SUB_QNET		=(1<<3);	/* Sub-board is netted via QWK network		*/
var   SUB_PNET		=(1<<4);	/* Sub-board is netted via PostLink			*/
var   SUB_FIDO		=(1<<5);	/* Sub-board is netted via FidoNet			*/
var   SUB_PRIV		=(1<<6);	/* Allow private posts on sub				*/
var   SUB_PONLY		=(1<<7);	/* Private posts only						*/
var   SUB_ANON		=(1<<8);	/* Allow anonymous posts on sub				*/
var   SUB_AONLY		=(1<<9);	/* Anonymous only							*/
var   SUB_NAME		=(1<<10);	/* Must use real names						*/
var   SUB_DEL 		=(1<<11);	/* Allow users to delete messages			*/
var   SUB_DELLAST	=(1<<12);	/* Allow users to delete last msg only		*/
var   SUB_FORCED	=(1<<13);	/* Sub-board is forced scanning				*/
var   SUB_NOTAG		=(1<<14);	/* Don't add tag or origin lines			*/
var   SUB_TOUSER	=(1<<15);	/* Prompt for to user on posts				*/
var   SUB_ASCII		=(1<<16);	/* ASCII characters only					*/
var   SUB_QUOTE		=(1<<17);	/* Allow online quoting						*/
var   SUB_NSDEF		=(1<<18);	/* New-Scan on by default					*/
var   SUB_INET		=(1<<19);	/* Sub-board is netted via Internet			*/
var   SUB_FAST		=(1<<20);	/* Fast storage mode						*/
var   SUB_KILL		=(1<<21);	/* Kill read messages automatically			*/
var   SUB_KILLP		=(1<<22);	/* Kill read pvt messages automatically		*/
var   SUB_SYSPERM	=(1<<23);	/* Sysop messages are permanent				*/
var   SUB_GATE		=(1<<24);	/* Gateway between Network types			*/
var   SUB_LZH 		=(1<<25);	/* Use LZH compression for msgs				*/
var   SUB_SSDEF		=(1<<26);	/* Default ON for Scan for Your msgs		*/
var   SUB_HYPER		=(1<<27);	/* Hyper allocation							*/
var   SUB_EDIT		=(1<<28);	/* Users can edit msg text after posting	*/
var   SUB_EDITLAST	=(1<<29);	/* Users can edit last message only			*/
var   SUB_NOUSERSIG	=(1<<30);	/* Suppress user signatures					*/
					    		/********************************************/

    							/********************************************/
                                /* Bit values for dir[x].settings			*/
							    /********************************************/
var   DIR_FCHK		=(1<<0);	/* Check for file existence					*/
var   DIR_RATE		=(1<<1);	/* Force uploads to be rated G,R, or X		*/
var   DIR_MULT		=(1<<2);	/* Ask for multi-disk numbering				*/
var   DIR_DUPES		=(1<<3);	/* Search this dir for upload dupes			*/
var   DIR_FREE		=(1<<4);	/* Free downloads							*/
var   DIR_TFREE		=(1<<5);	/* Time to download is free					*/
var   DIR_CDTUL		=(1<<6);	/* Credit Uploads							*/
var   DIR_CDTDL		=(1<<7);	/* Credit Downloads							*/
var   DIR_ANON		=(1<<8);	/* Anonymous uploads						*/
var   DIR_AONLY		=(1<<9);	/* Anonymous only							*/
var   DIR_ULDATE	=(1<<10);	/* Include upload date in listing			*/
var   DIR_DIZ 		=(1<<11);	/* FILE_ID.DIZ and DESC.SDI support			*/
var   DIR_NOSCAN	=(1<<12);	/* Don't new-scan this directory			*/
var   DIR_NOAUTO	=(1<<13);	/* Don't auto-add this directory			*/
var   DIR_ULTIME	=(1<<14);	/* Deduct time during uploads				*/
var   DIR_CDTMIN	=(1<<15);	/* Give uploader minutes instead of cdt		*/
var   DIR_SINCEDL	=(1<<16);	/* Purge based on days since last dl		*/
var   DIR_MOVENEW	=(1<<17);	/* Files marked as new when moved			*/
var   DIR_QUIET     =(1<<18);	/* Do not notify uploader of downloads 		*/
var   DIR_NOSTAT    =(1<<19);	/* Do not include transfers in system stats */
var   DIR_TEMPLATE  =(1<<21);	/* Use this dir as template for new dirs	*/
var   DIR_NOHASH    =(1<<22);	/* Don't auto calculate/store file hashes 	*/
var   DIR_FILETAGS  =(1<<23);	/* Allow files to have user-specified tags 	*/
					    		/********************************************/

					    		/********************************************/
								/* Bits in xtrn[x] and xedit[x].settings	*/
					    		/********************************************/
var XTRN_MULTIUSER	=(1<<0);	/* allow multi simultaneous users			*/
var XTRN_ANSI		=(1<<1);	/* user must have ANSI, same as ^^^			*/
var XTRN_IO_INTS 	=(1<<2);	/* Intercept I/O interrupts 				*/
var XTRN_MODUSERDAT	=(1<<3);	/* Program can modify user data 			*/
var XTRN_WWIVCOLOR	=(1<<4);	/* Program uses WWIV color codes			*/
var XTRN_EVENTONLY	=(1<<5);	/* Program executes as event only			*/
var XTRN_STARTUPDIR	=(1<<6);	/* Create drop file in start-up dir			*/
var XTRN_REALNAME	=(1<<7);	/* Use real name in drop file				*/
var XTRN_SWAP		=(1<<8);	/* Swap for this door						*/
var XTRN_FREETIME	=(1<<9);	/* Free time while in this door 			*/
var XTRN_QUICKBBS	=(1<<10);	/* QuickBBS style editor					*/
var XTRN_EXPANDLF	=(1<<11);	/* Expand LF to CRLF editor 				*/
var XTRN_QUOTEALL	=(1<<12);	/* Automatically quote all of msg			*/
var XTRN_QUOTENONE	=(1<<13);	/* Automatically quote none of msg			*/
var XTRN_NATIVE		=(1<<14);	/* Native application (EX_NATIVE)			*/
var XTRN_STRIPKLUDGE=(1<<15);	/* Strip FTN Kludge lines from msg			*/
var XTRN_CHKTIME	=(1<<16);	/* Check time online (EX_CHKTIME)			*/
var XTRN_LWRCASE	=(1<<17);	/* Use lowercase drop-file names			*/
var XTRN_SH			=(1<<18);	/* Use command shell to execute				*/
var XTRN_PAUSE		=(1<<19);	/* Force a screen pause on exit				*/
var XTRN_NOECHO		=(1<<20);	/* Don't echo stdin to stdout				*/
var XTRN_QUOTEWRAP	=(1<<21);	/* Word-wrap quoted message text			*/
var XTRN_SAVECOLUMNS=(1<<22);	/* Save/share current terminal width to msg	*/
var XTRN_UTF8		=(1<<23);	/* External program supports UTF-8			*/
var XTRN_TEMP_DIR	=(1<<24);	/* Place drop files in temp dir				*/
					    		/********************************************/

								/********************************************/
								/* Xtrn Drop file representations			*/
								/********************************************/
var XTRN_NONE		= 0;		/* No data file needed						*/
var XTRN_SBBS		= 1;		/* Synchronet external						*/
var XTRN_WWIV		= 2;		/* WWIV external							*/
var XTRN_GAP		= 3;		/* Gap door 								*/
var XTRN_RBBS		= 4;		/* RBBS, QBBS, or Remote Access 			*/
var XTRN_WILDCAT	= 5;		/* Wildcat									*/
var XTRN_PCBOARD	= 6;		/* PCBoard									*/
var XTRN_SPITFIRE	= 7;		/* SpitFire 								*/
var XTRN_UTI		= 8;		/* UTI Doors - MegaMail 					*/
var XTRN_SR			= 9;		/* Solar Realms 							*/
var XTRN_RBBS1 		= 10;		/* DORINFO1.DEF always						*/
var XTRN_TRIBBS		= 11;		/* TRIBBS.SYS								*/
var XTRN_DOOR32		= 12;		/* DOOR32.SYS								*/
								/********************************************/

								/********************************************/
								/* Transfer protocol 'settings' flags		*/
								/********************************************/
var PROT_DSZLOG		= (1<<0);   /* Supports DSZ Log 						*/
var PROT_NATIVE		= (1<<1);   /* Native (32-bit) executable 				*/
var PROT_SOCKET		= (1<<2);   /* Use socket I/O, not stdio on *nix 		*/
								/********************************************/

    							/********************************************/
				                /* Bit values for file.settings				*/
							    /********************************************/
var   FILE_EXTDESC	=(1<<0);    /* Extended description exists				*/
var   FILE_ANON 	=(1<<1);	/* Anonymous upload							*/
					    		/********************************************/

					    		/********************************************/
								/* Bits in the mode of bbs.exec()           */
					    		/********************************************/
var	  EX_NONE		=0;			/* No special behavior						*/
var   EX_SH			=(1<<0);	/* Use system shell to load other process   */
var   EX_STDOUT		=(1<<1);	/* Copy DOS output to remote                */
var   EX_STDIN		=(1<<3);	/* Trap int 16h keyboard input requests     */
var   EX_WWIV 		=(1<<4);	/* Expand WWIV color codes to ANSI sequence */
var   EX_POPEN		=(1<<7);	/* Leave COM port open						*/
var   EX_OFFLINE	=(1<<8);	/* Run this program offline					*/
var   EX_BG			=(1<<10);	/* Back-ground/detached process				*/
var   EX_BIN		=(1<<11);	/* Binary mode (no Unix LF to CR/LF)		*/
var   EX_NATIVE		=(1<<14);	/* Native 32-bit application (XTRN_NATIVE)	*/
var   EX_CHKTIME	=(1<<16);	/* Check time left (XTRN_CHKTIME)			*/
var   EX_NOLOG      =(1<<30);	/* Don't log intercepted stdio              */
					    		/********************************************/

var   EX_STDIO      =(EX_STDIN|EX_STDOUT);
// For backwards compatibility:
var   EX_OUTR = EX_STDOUT;
var   EX_INR = EX_STDIN;
					    		/********************************************/
								/* Values for bbs.user_event()				*/
					    		/********************************************/
var   EVENT_LOGON	=1;			/* Execute during logon sequence			*/
var   EVENT_LOGOFF	=2;			/* Execute during logoff sequence			*/
var   EVENT_NEWUSER	=3;			/* Execute during newuser app.				*/
var   EVENT_BIRTHDAY=4;			/* Execute on birthday						*/
var   EVENT_POST	=5;			/* Execute after message posted				*/
var   EVENT_UPLOAD	=6;			/* Execute after file uploaded				*/
var   EVENT_DOWNLOAD=7;			/* Execute after file downloaded			*/
					    		/********************************************/

								/********************************************/
								/* Bits for xtrn_area.event[].settings		*/
								/********************************************/
var EVENT_EXCL		=(1<<0);	/* Exclusive execution required				*/
var EVENT_FORCE		=(1<<1);	/* Force users off-line for event			*/
var EVENT_INIT		=(1<<2);	/* Always run event after BBS init/re-init	*/
var EVENT_DISABLED	=(1<<3);	/* Disabled									*/
								/********************************************/

					    		/********************************************/
								/* Bits in mode of bbs.telnet_gate()		*/
					    		/********************************************/
var   TG_NONE		=0;			/* No special behavior						*/
var   TG_ECHO		=(1<<0);	/* Turn on telnet echo						*/
var   TG_CRLF		=(1<<1);	/* Expand sole CR to CRLF					*/
var   TG_LINEMODE	=(1<<2);	/* Send entire lines only					*/
var   TG_NODESYNC	=(1<<3);	/* Call Nodesync, get msgs, etc.			*/
var   TG_CTRLKEYS	=(1<<4);	/* Interpret ^P ^U ^T, etc locally			*/
var   TG_PASSTHRU	=(1<<5);	/* Pass-through telnet commands/responses	*/
var   TG_RLOGIN		=(1<<6);	/* Use BSD RLogin protocol					*/
var   TG_NOCHKTIME	=(1<<7);	/* Don't check time left online				*/
var   TG_NOTERMTYPE =(1<<8);	/* Request client "DONT TERM_TYPE"			*/
var   TG_SENDPASS	=(1<<9);	/* DEPRECATED								*/
var   TG_NOLF		=(1<<10);	/* Do not send line-feeds					*/
var   TG_RLOGINSWAP	=(1<<11);	/* Swap the RLogin alias/real-names			*/
var   TG_RAW        =(1<<12);	/* Connecting to a raw TCP server			*/
var   TG_EXPANDLF   =(1<<13);	/* Expand incoming LF to CRLF				*/
					    		/********************************************/

					    		/********************************************/
								/* Bits in console.telnet_mode				*/
					    		/********************************************/
var TELNET_MODE_GATE=(1<<2);	/* Pass-through telnet commands/responses	*/
var TELNET_MODE_OFF	=(1<<3);	/* Not a telnet connection					*/
					    		/********************************************/

								/* Telnet standard commands and options		*/
								/* for use with console.telnet_cmd()		*/
var TELNET_DONT		= 254;		/* 0xfe - Don't do option */
var TELNET_DO   	= 253;		/* 0xfd - Do option */
var TELNET_WONT 	= 252;		/* 0xfc - Won't do option */
var TELNET_WILL 	= 251;		/* 0xfb - Will do option */
var TELNET_BINARY_TX = 0;		/* option: binary transmission */

					    		/********************************************/
								/* Bits in mode of bbs.scan_subs()			*/
                                /*                 bbs.scan_msgs()			*/
                                /*             and bbs.list_msgs()			*/
					    		/********************************************/
var SCAN_READ		=0;			/* Just normal read prompt (all messages)	*/
var	SCAN_CONT		=(1<<0);	/* Continuous message scanning				*/
var	SCAN_NEW		=(1<<1);	/* Display messages newer than pointer 	    */
var	SCAN_BACK		=(1<<2);	/* Display most recent message if none new  */
var	SCAN_TOYOU		=(1<<3);	/* Display messages to you only				*/
var	SCAN_FIND		=(1<<4);	/* Find text in messages				    */
var	SCAN_UNREAD		=(1<<5);	/* Display un-read messages to you only		*/
var SCAN_MSGSONLY	=(1<<6);	/* Do not do a new file scan even if the    */
var SCAN_POLLS		=(1<<7);	/* Scan for polls only (no messages)		*/
var SCAN_INDEX		=(1<<8);	// List the msg index or exec listmsgs_mod
var	SCAN_CONST		=SCAN_CONT;	// For backwards compatibility
								/* user enabled Automatic New File Scan		*/
					    		/********************************************/

								/* Bit values for msg_area.settings */
var MM_REALNAME		=(1<<16);	/* Allow receipt of e-mail using real names	*/
var MM_EMAILSIG		=(1<<17);	/* Include user signatures in e-mail msgs */

								/********************************************/
								/* Bits in msg_area.sub[].scan_cfg			*/
								/********************************************/
var SCAN_CFG_NEW	=5;	        /* Auto-scan for new messages				*/
var SCAN_CFG_TOYOU	=(1<<1);	/* Auto-scan for unread messages to you		*/
var SCAN_CFG_YONLY	=(1<<8);	/* Auto-scan for new messages to you only	*/
								/********************************************/

								/********************************************/
								/********************************************/
								/* Bits in mode for bbs.scan_dirs()			*/
								/* bbs.list_files() & bbs.list_file_info()	*/
								/********************************************/
var FL_NONE			=0;			/* No special behavior						*/
var FL_ULTIME		=(1<<0);	/* List files by upload time                */
var FL_DLTIME		=(1<<1);	/* List files by download time              */
var FL_NO_HDR		=(1<<2);	/* Don't list directory header              */
var FL_FINDDESC		=(1<<3);	/* Find text in description                 */
var FL_EXFIND		=(1<<4);	/* Find text in description - extended info */
var FL_VIEW			=(1<<5);	/* View ZIP/ARC/GIF etc. info               */
								/********************************************/

					    		/********************************************/
								/* Values of mode for bbs.list_users()		*/
					    		/********************************************/
var UL_ALL			=0;			/* List all active users in user database	*/
var UL_SUB			=1;   		/* List all users with access to cursub     */
var UL_DIR			=2;			/* List all users with access to curdir 	*/
					    		/********************************************/

					    		/********************************************/
								/* Values of mode in bbs.list_file_info()	*/
					    		/********************************************/
var FI_INFO         =0;  		/* Just list file information               */
var FI_REMOVE       =1;   		/* Remove/Move/Edit file information        */
var FI_DOWNLOAD     =2;   		/* Download files                           */
var FI_OLD          =3;   		/* Search/Remove files not downloaded since */
var FI_OLDUL	 	=4;			/* Search/Remove files uploaded before      */
var FI_OFFLINE   	=5;			/* Search/Remove files not online			*/
var FI_USERXFER  	=6;			/* User Xfer Download                       */
var FI_CLOSE 	  	=7;			/* Close any open records					*/
					    		/********************************************/

if(this.LOG_EMERG===undefined) {	/* temporary backward compatibility kludge	*/
	                            /********************************************/
                                /* Log "levels" supported by log() function */
                                /********************************************/
var LOG_EMERG       =0;			/* system is unusable                       */
var LOG_ALERT       =1;			/* action must be taken immediately         */
var LOG_CRIT        =2;			/* critical conditions                      */
var LOG_ERR         =3;			/* error conditions                         */
var LOG_WARNING     =4;			/* warning conditions                       */
var LOG_NOTICE      =5;			/* normal but significant condition         */
var LOG_INFO        =6;			/* informational                            */
var LOG_DEBUG       =7;			/* debug-level messages                     */
                                /********************************************/
}

								/* "flags" bits for directory() */
var GLOB_MARK		=(1<<1);	/* Append a slash to each name.  */
var GLOB_NOSORT		=(1<<2);	/* Don't sort the names.  */
var GLOB_APPEND		=(1<<5);	/* Append to results of a previous call.  */
var GLOB_NOESCAPE   =(1<<6);	/* Backslashes don't quote metacharacters.  */
var GLOB_PERIOD     =(1<<7); 	/* Leading `.' can be matched by metachars.  */
var GLOB_ONLYDIR    =(1<<13);	/* Match only directories.  */

								/* Bits in the lm_mode of bbs.read_mail()	*/
var LM_UNREAD		=(1<<0);	/* Include un-read mail only				*/
var LM_INCDEL		=(1<<1);	/* Include deleted mail		 				*/
var LM_NOSPAM		=(1<<2);	/* Exclude SPAM								*/
var LM_SPAMONLY		=(1<<3);	/* Load SPAM only							*/
var LM_REVERSE		=(1<<4);	/* Reverse the index order (newest-first)	*/

								/********************************************/
								/* Values for which in bbs.read_mail()		*/
								/********************************************/
var MAIL_YOUR			=0;		/* mail sent to you							*/
var MAIL_SENT			=1;		/* mail you have sent						*/
var MAIL_ANY			=2;		/* mail sent to or from you					*/
var MAIL_ALL			=3;		/* all mail (ignores usernumber arg)		*/
								/********************************************/

								/********************************************/
								/* 'mode' bits for bbs.email()/netmail()	*/
								/********************************************/
var WM_NONE			=0;			/* No special behavior						*/
var WM_EXTDESC		=(1<<0);	/* Writing extended file description		*/
var WM_EMAIL		=(1<<1);	/* Writing e-mail							*/
var WM_NETMAIL		=(1<<2);	/* Writing NetMail							*/
var WM_ANON 		=(1<<3);	/* Writing anonymous message				*/
var WM_FILE 		=(1<<4);	/* Attaching a file to the message			*/
var WM_NOTOP		=(1<<5);	/* Don't add top because we need top line   */
var WM_QUOTE		=(1<<6);	/* Quote file available 					*/
var WM_QWKNET		=(1<<7);	/* Writing QWK NetMail (25 char title)		*/
var WM_PRIVATE		=(1<<8);	/* Private (for creating MSGINF file)		*/
var WM_SUBJ_RO		=(1<<9);	/* Subject/title is read-only				*/
var WM_EDIT			=(1<<10);	/* Editing existing message					*/
var WM_FORCEFWD		=(1<<11);	/* Force "yes" to ForwardMailQ for email	*/
var WM_NOFWD		=(1<<12);	/* Don't forward email to netmail			*/
var WM_EXPANDLF     =(1<<13);   /* Insure CRLF-terminated lines             */
var WM_STRIP_CR     =(1<<14);   /* Insure no carriage-returns in output     */
								/********************************************/

								/************************************************/
								/* string length values 						*/
								/************************************************/
var LEN_ALIAS			=25;	/* User alias									*/
var LEN_NAME			=25;	/* User name									*/
var LEN_HANDLE			=8;		/* User chat handle 							*/
var LEN_NOTE			=30;	/* User note									*/
var LEN_COMP			=30;	/* User computer description					*/
var LEN_COMMENT 		=60;	/* User comment 								*/
var LEN_NETMAIL 		=60;	/* NetMail forwarding address					*/
var LEN_OLDPASS			=8;		/* User password (old/short location)			*/
var LEN_PASS			=40;	/* User password								*/
var LEN_PHONE			=12;	/* User phone number							*/
var LEN_BIRTH			=8;		/* Birthday in MM/DD/YY format					*/
var LEN_ADDRESS 		=30;	/* User address 								*/
var LEN_LOCATION		=30;	/* Location (City, State)						*/
var LEN_ZIPCODE 		=10;	/* Zip/Postal code								*/
var LEN_MODEM			=8;		/* User modem type description					*/
var LEN_FDESC			=58;	/* File description 							*/
var LEN_FCDT			=9;		/* 9 digits for file credit values				*/
var LEN_TITLE			=70;	/* Message title								*/
var LEN_MAIN_CMD		=34;	/* Storage in user.dat for custom commands		*/
var LEN_SCAN_CMD		=35;
var LEN_IPADDR			=45;
var LEN_CID 			=45;	/* Caller ID (phone number or IP address) 		*/
var LEN_ARSTR			=40;	/* Max length of Access Requirement string		*/
var LEN_CHATACTCMD		=9;		/* Chat action command							*/
var LEN_CHATACTOUT		=65;	/* Chat action output string					*/
								/************************************************/


/********************************************/
/* field values for system.matchuserdata()  */
/* synchronized with userfields.h			*/
/********************************************/
var U_ALIAS 		=1;
var U_NAME			=2;
var U_HANDLE		=3;
var U_NOTE			=4;
var U_IPADDR		=5;
var U_HOST			=6;
var U_NETMAIL		=7;
var U_ADDRESS		=8;
var U_LOCATION		=9;
var U_ZIPCODE		=10;
var U_PHONE  		=11;
var U_BIRTH  		=12;
var U_GENDER		=13;
var U_COMMENT		=14;
var U_CONNECTION	=15;
var U_MISC			=16;
var U_QWK			=17;
var U_CHAT			=18;
var U_ROWS			=19;
var U_COLS			=20;
var U_XEDIT			=21;
var U_SHELL			=22;
var U_TMPEXT		=23;
var U_PROT			=24;
var U_CURSUB		=25;
var U_CURDIR		=26;
var U_CURXTRN		=27;
var U_LOGONTIME		=28;
var U_NS_TIME		=29;
var U_LASTON		=30;
var U_FIRSTON		=31;
var U_LOGONS    	=32;
var U_LTODAY    	=33;
var U_TIMEON    	=34;
var U_TTODAY    	=35;
var U_TLAST     	=36;
var U_POSTS     	=37;
var U_EMAILS    	=38;
var U_FBACKS    	=39;
var U_ETODAY		=40;
var U_PTODAY		=41;
var U_ULB       	=42;
var U_ULS       	=43;
var U_DLB       	=44;
var U_DLS       	=45;
var U_LEECH 		=46;
var U_PASS			=47;
var U_PWMOD     	=48;
var U_LEVEL 		=49;
var U_FLAGS1		=50;
var U_FLAGS2		=51;
var U_FLAGS3		=52;
var U_FLAGS4		=53;
var U_EXEMPT		=54;
var U_REST			=55;
var U_CDT			=56;
var U_FREECDT		=57;
var U_MIN			=58;
var U_TEXTRA  		=59;
var U_EXPIRE    	=60;
