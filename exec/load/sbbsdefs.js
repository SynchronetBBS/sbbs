/* sbbsdefs.js */

/* Synchronet Object Model constants definitions - (mostly bit-fields) */

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

load("nodedefs.js");

							    /********************************************/
							    /* system.settings							*/
							    /********************************************/
const SYS_CLOSED	=(1<<0) 	/* System is closed to New Users		    */
const SYS_SYSSTAT	=(1<<1) 	/* Sysops activity included in statistics	*/
const SYS_NOBEEP	=(1<<2) 	/* No beep sound locally					*/
const SYS_PWEDIT	=(1<<3) 	/* Allow users to change their passwords	*/
const SYS_RA_EMU	=(1<<4) 	/* Reverse R/A commands at msg read prompt	*/
const SYS_ANON_EM	=(1<<5) 	/* Allow anonymous e-mail					*/
const SYS_LISTLOC	=(1<<6) 	/* Use location of caller in user lists 	*/
const SYS_WILDCAT	=(1<<7) 	/* Expand Wildcat color codes in messages	*/
const SYS_PCBOARD	=(1<<8) 	/* Expand PCBoard color codes in messages	*/
const SYS_WWIV 		=(1<<9) 	/* Expand WWIV color codes in messages		*/
const SYS_CELERITY	=(1<<10)	/* Expand Celerity color codes in messages	*/
const SYS_RENEGADE	=(1<<11)	/* Expand Renegade color codes in messages	*/
const SYS_ECHO_PW	=(1<<12)	/* Echo passwords locally					*/
const SYS_REQ_PW	=(1<<13)	/* Require passwords locally				*/
const SYS_L_SYSOP	=(1<<14)	/* Allow local sysop logon/commands 		*/
const SYS_R_SYSOP	=(1<<15)	/* Allow remote sysop logon/commands		*/
const SYS_QUOTE_EM	=(1<<16)	/* Allow quoting of e-mail					*/
const SYS_EURODATE	=(1<<17)	/* Europian date format (DD/MM/YY)			*/
const SYS_MILITARY	=(1<<18)	/* Military time format 					*/
const SYS_TIMEBANK	=(1<<19)	/* Allow time bank functions				*/
const SYS_FILE_EM	=(1<<20)	/* Allow file attachments in E-mail 		*/
const SYS_SHRTPAGE	=(1<<21)	/* Short sysop page 						*/
const SYS_TIME_EXP	=(1<<22)	/* Set to expired values if out-of-time 	*/
const SYS_FASTMAIL	=(1<<23)	/* Fast e-mail storage mode 				*/
const SYS_QVALKEYS	=(1<<24)	/* Quick validation keys enabled			*/
const SYS_ERRALARM	=(1<<25)	/* Error beeps on							*/
const SYS_FWDTONET	=(1<<26)	/* Allow forwarding of e-mail to netmail	*/
const SYS_DELREADM	=(1<<27)	/* Delete read mail automatically			*/
const SYS_NOCDTCVT	=(1<<28)	/* No credit to minute conversions allowed	*/
const SYS_DELEMAIL	=(1<<29)	/* Physically remove deleted e-mail immed.	*/
const SYS_USRVDELM	=(1<<30)	/* Users can see deleted msgs				*/
const SYS_SYSVDELM	=(1<<31)	/* Sysops can see deleted msgs				*/
					    		/********************************************/

						    	/********************************************/
    							/* bbs.sys_status							*/
							    /********************************************/
const SS_UNUSED		=(1<<0)		/* Unused          							*/
const SS_INITIAL	=(1<<1)		/* The bbs data has been initialized.       */
const SS_TMPSYSOP	=(1<<2)		/* Temporary Sysop Status					*/
const SS_USERON		=(1<<3)		/* A User is logged on to the BBS			*/
const SS_LCHAT		=(1<<4)		/* Local chat in progress					*/
const SS_CAP		=(1<<5)		/* Capture is on							*/
const SS_ANSCAP		=(1<<6)		/* Capture ANSI codes too					*/
const SS_FINPUT		=(1<<7)		/* Using file for input 					*/
const SS_COMISR		=(1<<8)		/* Com port ISR is installed				*/
const SS_DAILY		=(1<<9)		/* Execute System Daily Event on logoff 	*/
const SS_INUEDIT	=(1<<10)	/* Inside Alt-Useredit section 				*/
const SS_ABORT		=(1<<11)	/* Global abort input or output flag		*/
const SS_SYSPAGE	=(1<<12)	/* Paging sysop								*/
const SS_SYSALERT	=(1<<13)	/* Notify sysop when users hangs up			*/
const SS_GURUCHAT	=(1<<14)	/* Guru chat in progress					*/
const SS_UNUSED2	=(1<<15)	/* not used in v3 (used to be SS_NODEDAB)	*/
const SS_EVENT		=(1<<16)	/* Time shortened due to upcoming event		*/
const SS_PAUSEON	=(1<<17)	/* Pause on, overriding user default		*/
const SS_PAUSEOFF	=(1<<18)	/* Pause off, overriding user default		*/
const SS_IN_CTRLP	=(1<<19)	/* Inside ctrl-p send node message func		*/
const SS_NEWUSER	=(1<<20)	/* New User online 							*/
const SS_MDMDEBUG	=(1<<21)	/* Modem debug output						*/
const SS_NEST_PF	=(1<<22)	/* Nested in printfile function				*/
const SS_DCDHIGH	=(1<<23)	/* Assume DCD is high always				*/
const SS_SPLITP		=(1<<24)	/* Split-screen private chat				*/
const SS_NEWDAY		=(1<<25)	/* Date changed while online				*/
const SS_RLOGIN		=(1<<26)	/* Current login via BSD RLogin				*/
const SS_FILEXFER	=(1<<27)	/* File transfer in progress, halt spy		*/
					    		/********************************************/

							    /********************************************/
							    /* console.status							*/
							    /********************************************/
const CON_R_ECHO	=(1<<0)		/* Echo remotely							*/
const CON_R_ECHOX	=(1<<1)		/* Echo X's to remote user					*/
const CON_R_INPUT  	=(1<<2)		/* Accept input remotely					*/
const CON_L_ECHO	=(1<<3)		/* Echo locally              				*/
const CON_L_ECHOX	=(1<<4)		/* Echo X's locally							*/
const CON_L_INPUT  	=(1<<5)		/* Accept input locally						*/
const CON_RAW_IN   	=(1<<8)		/* Raw input mode - no editing capabilities */
const CON_ECHO_OFF 	=(1<<10)	/* Remote & Local echo disabled for ML/MF	*/
const CON_UPARROW  	=(1<<11)	/* Up arrow hit - move up one line			*/
const CON_NO_INACT  =(1<<13)	/* User inactivity detection disabled		*/
					    		/********************************************/

							    /********************************************/
							    /* console.attributes, also used for ansi()	*/
							    /********************************************/
const BLINK			=0x80		/* blink bit */
const HIGH			=0x08		/* high intensity foreground bit */

							    /* foreground colors */
const BLACK			=0			/* dark colors (HIGH bit unset) */
const BLUE			=1
const GREEN			=2
const CYAN			=3
const RED			=4
const MAGENTA		=5
const BROWN			=6
const LIGHTGRAY		=7
const DARKGRAY		=8			/* light colors (HIGH bit set) */
const LIGHTBLUE		=9
const LIGHTGREEN	=10
const LIGHTCYAN		=11
const LIGHTRED		=12
const LIGHTMAGENTA	=13
const YELLOW		=14
const WHITE			=15

							    /* background colors */
const ANSI_NORMAL	=0x100		/* special value for ansi() */
const BG_BLACK		=0x200		/* special value for ansi() */
const BG_BLUE		=(BLUE<<4)
const BG_GREEN		=(GREEN<<4)
const BG_CYAN		=(CYAN<<4)
const BG_RED		=(RED<<4)
const BG_MAGENTA	=(MAGENTA<<4)
const BG_BROWN		=(BROWN<<4)
const BG_LIGHTGRAY	=(LIGHTGRAY<<4)
						
					    		/********************************************/
						    	/* user.settings							*/
							    /********************************************/
const USER_DELETED  =(1<<0) 	/* Deleted user slot						*/
const USER_ANSI		=(1<<1) 	/* Supports ANSI terminal emulation			*/
const USER_COLOR	=(1<<2) 	/* Send color codes 						*/
const USER_RIP 		=(1<<3) 	/* Supports RIP terminal emulation			*/
const USER_PAUSE	=(1<<4) 	/* Pause on every screen full				*/
const USER_SPIN		=(1<<5) 	/* Spinning cursor - Same as K_SPIN			*/
const USER_INACTIVE	=(1<<6) 	/* Inactive user slot						*/
const USER_EXPERT	=(1<<7) 	/* Expert menu mode 						*/
const USER_ANFSCAN 	=(1<<8) 	/* Auto New file scan						*/
const USER_CLRSCRN 	=(1<<9) 	/* Clear screen before each message			*/
const USER_QUIET	=(1<<10)	/* Quiet mode upon logon					*/
const USER_BATCHFLAG=(1<<11)	/* File list allow batch dl flags			*/
const USER_NETMAIL 	=(1<<12)	/* Forward e-mail to fidonet addr			*/
const USER_CURSUB	=(1<<13)	/* Remember current sub-board/dir			*/
const USER_ASK_NSCAN=(1<<14)	/* Ask for newscanning upon logon			*/
const USER_NO_EXASCII=(1<<15)	/* Don't send extended ASCII				*/
const USER_ASK_SSCAN=(1<<16)	/* Ask for messages to you at logon			*/
const USER_AUTOTERM	=(1<<17)	/* Autodetect terminal type 				*/
const USER_COLDKEYS	=(1<<18)	/* No hot-keys								*/
const USER_EXTDESC 	=(1<<19)	/* Extended file descriptions				*/
const USER_AUTOHANG	=(1<<20)	/* Auto-hang-up after transfer				*/
const USER_WIP 		=(1<<21)	/* Supports WIP terminal emulation			*/
const USER_AUTOLOGON=(1<<22)	/* AutoLogon via IP							*/
					    		/********************************************/

/************************************************************************/
/* Valid flags for user.security.exempt/restrict/flags					*/
/************************************************************************/
const UFLAG_A		=(1<<0)
const UFLAG_B		=(1<<1)
const UFLAG_C		=(1<<2)
const UFLAG_D		=(1<<3)
const UFLAG_E		=(1<<4)
const UFLAG_F		=(1<<5)
const UFLAG_G		=(1<<6)
const UFLAG_H		=(1<<7)
const UFLAG_I		=(1<<8)
const UFLAG_J		=(1<<9)
const UFLAG_K		=(1<<10)
const UFLAG_L		=(1<<11)
const UFLAG_M		=(1<<12)
const UFLAG_N		=(1<<13)
const UFLAG_O		=(1<<14)
const UFLAG_P		=(1<<15)
const UFLAG_Q		=(1<<16)
const UFLAG_R		=(1<<17)
const UFLAG_S		=(1<<18)
const UFLAG_T		=(1<<19)
const UFLAG_U		=(1<<20)
const UFLAG_V		=(1<<21)
const UFLAG_W		=(1<<22)
const UFLAG_X		=(1<<23)
const UFLAG_Y		=(1<<24)
const UFLAG_Z		=(1<<25)

						    	/********************************************/
    							/* Bits in 'mode' for getkey and getstr     */
							    /********************************************/
const K_UPPER 		=(1<<0)		/* Converts all letters to upper case		*/
const K_UPRLWR		=(1<<1)		/* Upper/Lower case automatically			*/
const K_NUMBER		=(1<<2)		/* Allow numbers only						*/
const K_WRAP		=(1<<3)		/* Allows word wrap 						*/
const K_MSG			=(1<<4)		/* Allows ANSI, ^N ^A ^G					*/
const K_SPIN		=(1<<5)		/* Spinning cursor (same as SPIN)			*/
const K_LINE		=(1<<6)		/* Input line (inverse color)				*/
const K_EDIT		=(1<<7)		/* Edit string passed						*/
const K_CHAT		=(1<<8)		/* In chat multi-chat						*/
const K_NOCRLF		=(1<<9)		/* Don't print CRLF after string input      */
const K_ALPHA 		=(1<<10)	/* Only allow alphabetic characters 		*/
const K_GETSTR		=(1<<11)	/* getkey called from getstr()				*/
const K_LOWPRIO		=(1<<12)	/* low priority input						*/
const K_NOEXASC		=(1<<13)	/* No extended ASCII allowed				*/
const K_E71DETECT	=(1<<14)	/* Detect E-7-1 terminal type				*/
const K_AUTODEL		=(1<<15)	/* Auto-delete text (used with K_EDIT)		*/
const K_COLD		=(1<<16)	/* Possible cold key mode					*/
const K_NOECHO		=(1<<17)	/* Don't echo input                         */
const K_TAB			=(1<<18)	/* Treat TAB key as CR						*/
					    		/********************************************/
						
						    	/********************************************/
    							/* Bits in 'mode' for putmsg and printfile  */
							    /********************************************/
const P_NOABORT  	=(1<<0)		/* Disallows abortion of a message          */
const P_SAVEATR		=(1<<1)		/* Save the new current attributres after	*/
					    		/* msg has printed */
const P_NOATCODES	=(1<<2)		/* Don't allow @ codes                      */
const P_OPENCLOSE	=(1<<3)		/* Open and close the file					*/
							    /********************************************/

    							/********************************************/
							    /* system.new_user_questions				*/
							    /********************************************/
const UQ_ALIASES	=(1<<0) 	/* Ask for alias							*/
const UQ_LOCATION	=(1<<1) 	/* Ask for location 						*/
const UQ_ADDRESS	=(1<<2) 	/* Ask for address							*/
const UQ_PHONE		=(1<<3) 	/* Ask for phone number 					*/
const UQ_HANDLE		=(1<<4) 	/* Ask for chat handle						*/
const UQ_DUPHAND	=(1<<5) 	/* Search for duplicate handles 			*/
const UQ_SEX		=(1<<6) 	/* Ask for sex								*/
const UQ_BIRTH		=(1<<7) 	/* Ask for birth date						*/
const UQ_COMP 		=(1<<8) 	/* Ask for computer type					*/
const UQ_MC_COMP	=(1<<9) 	/* Multiple choice computer type			*/
const UQ_REALNAME	=(1<<10)	/* Ask for real name						*/
const UQ_DUPREAL	=(1<<11)	/* Search for duplicate real names			*/
const UQ_COMPANY	=(1<<12)	/* Ask for company name 					*/
const UQ_NOEXASC	=(1<<13)	/* Don't allow ex-ASCII in user text		*/
const UQ_CMDSHELL	=(1<<14)	/* Ask for command shell					*/
const UQ_XEDIT		=(1<<15)	/* Ask for external editor					*/
const UQ_NODEF		=(1<<16)	/* Don't ask for default settings			*/
const UQ_NOCOMMAS	=(1<<17)	/* Do not require commas in location		*/
					    		/********************************************/
				
							    /********************************************/
							    /* node.settings							*/
							    /********************************************/
const NM_ANSALARM	=(1<<0)		/* Alarm locally on answer					*/
const NM_WFCSCRN	=(1<<1)		/* Wait for call screen                     */
const NM_WFCMSGS	=(1<<2)		/* Include total messages/files on WFC		*/
const NM_LCL_EDIT	=(1<<3)		/* Use local editor to create messages		*/
const NM_EMSOVL		=(1<<4)		/* Use expanded memory of overlays			*/
const NM_WINOS2		=(1<<5)		/* Use Windows/OS2 time slice API call		*/
const NM_INT28		=(1<<6)		/* Make int 28 DOS idle calls				*/
const NM_NODV 		=(1<<7)		/* Don't detect and use DESQview API        */
const NM_NO_NUM		=(1<<8)		/* Don't allow logons by user number        */
const NM_LOGON_R	=(1<<9)		/* Allow logons by user real name			*/
const NM_LOGON_P	=(1<<10)	/* Secure logons (always ask for password)	*/
const NM_NO_LKBRD	=(1<<11)	/* No local keyboard (at all)				*/
const NM_SYSPW		=(1<<12)	/* Protect WFC keys and Alt keys with SY:	*/
const NM_NO_INACT	=(1<<13)	/* No local inactivity alert/logoff 		*/
const NM_NOBEEP		=(1<<14)	/* Don't beep locally                       */
const NM_LOWPRIO	=(1<<15)	/* Always use low priority input			*/
const NM_7BITONLY	=(1<<16)	/* Except 7-bit input only (E71 terminals)	*/
const NM_RESETVID	=(1<<17)	/* Reset video mode between callers?		*/
const NM_NOPAUSESPIN=(1<<18)	/* No spinning cursor at pause prompt		*/
					    		/********************************************/

						    	/********************************************/
							    /* netmail_misc								*/
							    /********************************************/
const NMAIL_ALLOW 	=(1<<0)		/* Allow NetMail							*/
const NMAIL_CRASH 	=(1<<1)		/* Default netmail to crash					*/
const NMAIL_HOLD	=(1<<2)		/* Default netmail to hold					*/
const NMAIL_KILL	=(1<<3)		/* Default netmail to kill after sent		*/
const NMAIL_ALIAS 	=(1<<4)		/* Use Aliases in NetMail					*/
const NMAIL_FILE	=(1<<5)		/* Allow file attachments					*/
const NMAIL_DIRECT	=(1<<6)		/* Default netmail to direct				*/
					    		/********************************************/

    							/********************************************/
							    /* Bit values for sub[x].settings			*/
							    /********************************************/
const SUB_QNET		=(1<<3) 	/* Sub-board is netted via QWK network		*/
const SUB_PNET		=(1<<4) 	/* Sub-board is netted via PostLink			*/
const SUB_FIDO		=(1<<5) 	/* Sub-board is netted via FidoNet			*/
const SUB_PRIV		=(1<<6) 	/* Allow private posts on sub				*/
const SUB_PONLY		=(1<<7) 	/* Private posts only						*/
const SUB_ANON		=(1<<8) 	/* Allow anonymous posts on sub				*/
const SUB_AONLY		=(1<<9) 	/* Anonymous only							*/
const SUB_NAME		=(1<<10)	/* Must use real names						*/
const SUB_DEL 		=(1<<11)	/* Allow users to delete messages			*/
const SUB_DELLAST	=(1<<12)	/* Allow users to delete last msg only		*/
const SUB_FORCED	=(1<<13)	/* Sub-board is forced scanning				*/
const SUB_NOTAG		=(1<<14)	/* Don't add tag or origin lines			*/
const SUB_TOUSER	=(1<<15)	/* Prompt for to user on posts				*/
const SUB_ASCII		=(1<<16)	/* ASCII characters only					*/
const SUB_QUOTE		=(1<<17)	/* Allow online quoting						*/
const SUB_NSDEF		=(1<<18)	/* New-Scan on by default					*/
const SUB_INET		=(1<<19)	/* Sub-board is netted via Internet			*/
const SUB_FAST		=(1<<20)	/* Fast storage mode						*/
const SUB_KILL		=(1<<21)	/* Kill read messages automatically			*/
const SUB_KILLP		=(1<<22)	/* Kill read pvt messages automatically		*/
const SUB_SYSPERM	=(1<<23)	/* Sysop messages are permament				*/
const SUB_GATE		=(1<<24)	/* Gateway between Network types			*/
const SUB_LZH 		=(1<<25)	/* Use LZH compression for msgs				*/
const SUB_SSDEF		=(1<<26)	/* Default ON for Scan for Your msgs		*/
const SUB_HYPER		=(1<<27)	/* Hyper allocation							*/
					    		/********************************************/

    							/********************************************/
                                /* Bit values for dir[x].settings			*/
							    /********************************************/
const DIR_FCHK		=(1<<0) 	/* Check for file existance					*/
const DIR_RATE		=(1<<1) 	/* Force uploads to be rated G,R, or X		*/
const DIR_MULT		=(1<<2) 	/* Ask for multi-disk numbering				*/
const DIR_DUPES		=(1<<3) 	/* Search this dir for upload dupes			*/
const DIR_FREE		=(1<<4) 	/* Free downloads							*/
const DIR_TFREE		=(1<<5) 	/* Time to download is free					*/
const DIR_CDTUL		=(1<<6) 	/* Credit Uploads							*/
const DIR_CDTDL		=(1<<7) 	/* Credit Downloads							*/
const DIR_ANON		=(1<<8) 	/* Anonymous uploads						*/
const DIR_AONLY		=(1<<9) 	/* Anonymous only							*/
const DIR_ULDATE	=(1<<10)	/* Include upload date in listing			*/
const DIR_DIZ 		=(1<<11)	/* FILE_ID.DIZ and DESC.SDI support			*/
const DIR_NOSCAN	=(1<<12)	/* Don't new-scan this directory			*/
const DIR_NOAUTO	=(1<<13)	/* Don't auto-add this directory			*/
const DIR_ULTIME	=(1<<14)	/* Deduct time during uploads				*/
const DIR_CDTMIN	=(1<<15)	/* Give uploader minutes instead of cdt		*/
const DIR_SINCEDL	=(1<<16)	/* Purge based on days since last dl		*/
const DIR_MOVENEW	=(1<<17)	/* Files marked as new when moved			*/
					    		/********************************************/
				
    							/********************************************/
				                /* Bit values for file.settings				*/
							    /********************************************/
const FILE_EXTDESC	=(1<<0)     /* Extended description exists				*/
const FILE_ANON 	=(1<<1)		/* Anonymous upload							*/
					    		/********************************************/
