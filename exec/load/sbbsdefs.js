/* sbbsdefs.js */

/* Synchronet Object Model var  ants definitions - (mostly bit-fields) */

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

/* Would rather use const than var, but end up with redeclaration errors.	*/

							    /********************************************/
							    /* system.settings							*/
							    /********************************************/
var   SYS_CLOSED	=(1<<0) 	/* System is closed to New Users		    */
var   SYS_SYSSTAT	=(1<<1) 	/* Sysops activity included in statistics	*/
var   SYS_NOBEEP	=(1<<2) 	/* No beep sound locally					*/
var   SYS_PWEDIT	=(1<<3) 	/* Allow users to change their passwords	*/
var   SYS_RA_EMU	=(1<<4) 	/* Reverse R/A commands at msg read prompt	*/
var   SYS_ANON_EM	=(1<<5) 	/* Allow anonymous e-mail					*/
var   SYS_LISTLOC	=(1<<6) 	/* Use location of caller in user lists 	*/
var   SYS_WILDCAT	=(1<<7) 	/* Expand Wildcat color codes in messages	*/
var   SYS_PCBOARD	=(1<<8) 	/* Expand PCBoard color codes in messages	*/
var   SYS_WWIV 		=(1<<9) 	/* Expand WWIV color codes in messages		*/
var   SYS_CELERITY	=(1<<10)	/* Expand Celerity color codes in messages	*/
var   SYS_RENEGADE	=(1<<11)	/* Expand Renegade color codes in messages	*/
var   SYS_ECHO_PW	=(1<<12)	/* Echo passwords locally					*/
var   SYS_REQ_PW	=(1<<13)	/* Require passwords locally				*/
var   SYS_L_SYSOP	=(1<<14)	/* Allow local sysop logon/commands 		*/
var   SYS_R_SYSOP	=(1<<15)	/* Allow remote sysop logon/commands		*/
var   SYS_QUOTE_EM	=(1<<16)	/* Allow quoting of e-mail					*/
var   SYS_EURODATE	=(1<<17)	/* Europian date format (DD/MM/YY)			*/
var   SYS_MILITARY	=(1<<18)	/* Military time format 					*/
var   SYS_TIMEBANK	=(1<<19)	/* Allow time bank functions				*/
var   SYS_FILE_EM	=(1<<20)	/* Allow file attachments in E-mail 		*/
var   SYS_SHRTPAGE	=(1<<21)	/* Short sysop page 						*/
var   SYS_TIME_EXP	=(1<<22)	/* Set to expired values if out-of-time 	*/
var   SYS_FASTMAIL	=(1<<23)	/* Fast e-mail storage mode 				*/
var   SYS_QVALKEYS	=(1<<24)	/* Quick validation keys enabled			*/
var   SYS_ERRALARM	=(1<<25)	/* Error beeps on							*/
var   SYS_FWDTONET	=(1<<26)	/* Allow forwarding of e-mail to netmail	*/
var   SYS_DELREADM	=(1<<27)	/* Delete read mail automatically			*/
var   SYS_NOCDTCVT	=(1<<28)	/* No credit to minute conversions allowed	*/
var   SYS_DELEMAIL	=(1<<29)	/* Physically remove deleted e-mail immed.	*/
var   SYS_USRVDELM	=(1<<30)	/* Users can see deleted msgs				*/
var   SYS_SYSVDELM	=(1<<31)	/* Sysops can see deleted msgs				*/
					    		/********************************************/

						    	/********************************************/
    							/* bbs.sys_status							*/
							    /********************************************/
var   SS_UNUSED		=(1<<0)		/* Unused          							*/
var   SS_INITIAL	=(1<<1)		/* The bbs data has been initialized.       */
var   SS_TMPSYSOP	=(1<<2)		/* Temporary Sysop Status					*/
var   SS_USERON		=(1<<3)		/* A User is logged on to the BBS			*/
var   SS_LCHAT		=(1<<4)		/* Local chat in progress					*/
var   SS_CAP		=(1<<5)		/* Capture is on							*/
var   SS_ANSCAP		=(1<<6)		/* Capture ANSI codes too					*/
var   SS_FINPUT		=(1<<7)		/* Using file for input 					*/
var   SS_COMISR		=(1<<8)		/* Com port ISR is installed				*/
var   SS_DAILY		=(1<<9)		/* Execute System Daily Event on logoff 	*/
var   SS_INUEDIT	=(1<<10)	/* Inside Alt-Useredit section 				*/
var   SS_ABORT		=(1<<11)	/* Global abort input or output flag		*/
var   SS_SYSPAGE	=(1<<12)	/* Paging sysop								*/
var   SS_SYSALERT	=(1<<13)	/* Notify sysop when users hangs up			*/
var   SS_GURUCHAT	=(1<<14)	/* Guru chat in progress					*/
var   SS_UNUSED2	=(1<<15)	/* not used in v3 (used to be SS_NODEDAB)	*/
var   SS_EVENT		=(1<<16)	/* Time shortened due to upcoming event		*/
var   SS_PAUSEON	=(1<<17)	/* Pause on, overriding user default		*/
var   SS_PAUSEOFF	=(1<<18)	/* Pause off, overriding user default		*/
var   SS_IN_CTRLP	=(1<<19)	/* Inside ctrl-p send node message func		*/
var   SS_NEWUSER	=(1<<20)	/* New User online 							*/
var   SS_MDMDEBUG	=(1<<21)	/* Modem debug output						*/
var   SS_NEST_PF	=(1<<22)	/* Nested in printfile function				*/
var   SS_DCDHIGH	=(1<<23)	/* Assume DCD is high always				*/
var   SS_SPLITP		=(1<<24)	/* Split-screen private chat				*/
var   SS_NEWDAY		=(1<<25)	/* Date changed while online				*/
var   SS_RLOGIN		=(1<<26)	/* Current login via BSD RLogin				*/
var   SS_FILEXFER	=(1<<27)	/* File transfer in progress, halt spy		*/
					    		/********************************************/

						    	/********************************************/
								/* bbs.startup_options						*/
						    	/********************************************/
var   BBS_OPT_KEEP_ALIVE		=(1<<0)	/* Send keep-alives					*/
var   BBS_OPT_XTRN_MINIMIZED	=(1<<1)	/* Run externals minimized			*/
var   BBS_OPT_AUTO_LOGON		=(1<<2)	/* Auto-logon via IP				*/
var   BBS_OPT_DEBUG_TELNET		=(1<<3)	/* Debug telnet commands			*/
var   BBS_OPT_SYSOP_AVAILABLE	=(1<<4)	/* Available for chat				*/
var   BBS_OPT_ALLOW_RLOGIN		=(1<<5)	/* Allow logins via BSD RLogin		*/
var   BBS_OPT_USE_2ND_RLOGIN	=(1<<6)	/* Use 2nd username in BSD RLogin	*/
var   BBS_OPT_NO_QWK_EVENTS		=(1<<7)	/* Don't run QWK-related events		*/
var   BBS_OPT_NO_HOST_LOOKUP	=(1<<11)/* Don't lookup hostname			*/
var   BBS_OPT_NO_JAVASCRIPT		=(1<<29)/* JavaScript disabled				*/
var   BBS_OPT_LOCAL_TIMEZONE	=(1<<30)/* Don't force UCT/GMT				*/
var   BBS_OPT_MUTE				=(1<<31)/* Mute sounds						*/
						    	/*******************************************/

						    	/********************************************/
								/* bbs.online								*/
						    	/********************************************/
var   ON_LOCAL		=1	 		/* Online locally (events only in v3)		*/
var   ON_REMOTE		=2 			/* Online remotely							*/
						    	/********************************************/

							    /********************************************/
							    /* console.status							*/
							    /********************************************/
var   CON_R_ECHO	=(1<<0)		/* Echo remotely							*/
var   CON_R_ECHOX	=(1<<1)		/* Echo X's to remote user					*/
var   CON_R_INPUT  	=(1<<2)		/* Accept input remotely					*/
var   CON_L_ECHO	=(1<<3)		/* Echo locally              				*/
var   CON_L_ECHOX	=(1<<4)		/* Echo X's locally							*/
var   CON_L_INPUT  	=(1<<5)		/* Accept input locally						*/
var   CON_RAW_IN   	=(1<<8)		/* Raw input mode - no editing capabilities */
var   CON_ECHO_OFF 	=(1<<10)	/* Remote & Local echo disabled for ML/MF	*/
var   CON_UPARROW  	=(1<<11)	/* Up arrow hit - move up one line			*/
var   CON_NO_INACT  =(1<<13)	/* User inactivity detection disabled		*/
					    		/********************************************/

							    /********************************************/
							    /* console.attributes, also used for ansi()	*/
							    /********************************************/
var   BLINK			=0x80		/* blink bit */
var   HIGH			=0x08		/* high intensity foreground bit */

							    /* foreground colors */
var   BLACK			=0			/* dark colors (HIGH bit unset) */
var   BLUE			=1
var   GREEN			=2
var   CYAN			=3
var   RED			=4
var   MAGENTA		=5
var   BROWN			=6
var   LIGHTGRAY		=7
var   DARKGRAY		=8			/* light colors (HIGH bit set) */
var   LIGHTBLUE		=9
var   LIGHTGREEN	=10
var   LIGHTCYAN		=11
var   LIGHTRED		=12
var   LIGHTMAGENTA	=13
var   YELLOW		=14
var   WHITE			=15

							    /* background colors */
var   ANSI_NORMAL	=0x100		/* special value for ansi() */
var   BG_BLACK		=0x200		/* special value for ansi() */
var   BG_BLUE		=(BLUE<<4)
var   BG_GREEN		=(GREEN<<4)
var   BG_CYAN		=(CYAN<<4)
var   BG_RED		=(RED<<4)
var   BG_MAGENTA	=(MAGENTA<<4)
var   BG_BROWN		=(BROWN<<4)
var   BG_LIGHTGRAY	=(LIGHTGRAY<<4)
						
					    		/********************************************/
						    	/* user.settings							*/
							    /********************************************/
var   USER_DELETED  =(1<<0) 	/* Deleted user slot						*/
var   USER_ANSI		=(1<<1) 	/* Supports ANSI terminal emulation			*/
var   USER_COLOR	=(1<<2) 	/* Send color codes 						*/
var   USER_RIP 		=(1<<3) 	/* Supports RIP terminal emulation			*/
var   USER_PAUSE	=(1<<4) 	/* Pause on every screen full				*/
var   USER_SPIN		=(1<<5) 	/* Spinning cursor - Same as K_SPIN			*/
var   USER_INACTIVE	=(1<<6) 	/* Inactive user slot						*/
var   USER_EXPERT	=(1<<7) 	/* Expert menu mode 						*/
var   USER_ANFSCAN 	=(1<<8) 	/* Auto New file scan						*/
var   USER_CLRSCRN 	=(1<<9) 	/* Clear screen before each message			*/
var   USER_QUIET	=(1<<10)	/* Quiet mode upon logon					*/
var   USER_BATCHFLAG=(1<<11)	/* File list allow batch dl flags			*/
var   USER_NETMAIL 	=(1<<12)	/* Forward e-mail to fidonet addr			*/
var   USER_CURSUB	=(1<<13)	/* Remember current sub-board/dir			*/
var   USER_ASK_NSCAN=(1<<14)	/* Ask for newscanning upon logon			*/
var   USER_NO_EXASCII=(1<<15)	/* Don't send extended ASCII				*/
var   USER_ASK_SSCAN=(1<<16)	/* Ask for messages to you at logon			*/
var   USER_AUTOTERM	=(1<<17)	/* Autodetect terminal type 				*/
var   USER_COLDKEYS	=(1<<18)	/* No hot-keys								*/
var   USER_EXTDESC 	=(1<<19)	/* Extended file descriptions				*/
var   USER_AUTOHANG	=(1<<20)	/* Auto-hang-up after transfer				*/
var   USER_WIP 		=(1<<21)	/* Supports WIP terminal emulation			*/
var   USER_AUTOLOGON=(1<<22)	/* AutoLogon via IP							*/
					    		/********************************************/

					    		/********************************************/
								/* user.qwk_settings 						*/
					    		/********************************************/
var   QWK_FILES		=(1<<0) 	/* Include new files list					*/
var   QWK_EMAIL		=(1<<1) 	/* Include unread e-mail					*/
var   QWK_ALLMAIL	=(1<<2) 	/* Include ALL e-mail						*/
var   QWK_DELMAIL	=(1<<3) 	/* Delete e-mail after download 			*/
var   QWK_BYSELF	=(1<<4) 	/* Include messages from self				*/
var   QWK_UNUSED	=(1<<5) 	/* Currently unused 						*/
var   QWK_EXPCTLA	=(1<<6) 	/* Expand ctrl-a codes to ascii 			*/
var   QWK_RETCTLA	=(1<<7) 	/* Retain ctrl-a codes						*/
var   QWK_ATTACH	=(1<<8) 	/* Include file attachments 				*/
var   QWK_NOINDEX	=(1<<9) 	/* Do not create index files in QWK			*/
var   QWK_TZ		=(1<<10)	/* Include @TZ (time zone) in msgs			*/
var   QWK_VIA 		=(1<<11)	/* Include @VIA (path) in msgs				*/
var   QWK_NOCTRL	=(1<<12)	/* No extraneous control files				*/
var   QWK_EXT		=(1<<13)	/* QWK Extended (QWKE) format				*/
var   QWK_MSGID		=(1<<14)	/* Include @MSGID and @REPLY in msgs		*/
					    		/********************************************/

					    		/********************************************/
								/* user.chat_settings						*/
					    		/********************************************/
var   CHAT_ECHO		=(1<<0)		/* Multinode chat echo						*/
var   CHAT_ACTION	=(1<<1)		/* Chat actions 							*/
var   CHAT_NOPAGE	=(1<<2)		/* Can't be paged                           */
var   CHAT_NOACT	=(1<<3)		/* No activity alerts						*/
var   CHAT_SPLITP	=(1<<4)		/* Split screen private chat				*/
					    		/********************************************/


/************************************************************************/
/* Valid flags for user.security.exempt/restrict/flags					*/
/************************************************************************/
var   UFLAG_A		=(1<<0)
var   UFLAG_B		=(1<<1)
var   UFLAG_C		=(1<<2)
var   UFLAG_D		=(1<<3)
var   UFLAG_E		=(1<<4)
var   UFLAG_F		=(1<<5)
var   UFLAG_G		=(1<<6)
var   UFLAG_H		=(1<<7)
var   UFLAG_I		=(1<<8)
var   UFLAG_J		=(1<<9)
var   UFLAG_K		=(1<<10)
var   UFLAG_L		=(1<<11)
var   UFLAG_M		=(1<<12)
var   UFLAG_N		=(1<<13)
var   UFLAG_O		=(1<<14)
var   UFLAG_P		=(1<<15)
var   UFLAG_Q		=(1<<16)
var   UFLAG_R		=(1<<17)
var   UFLAG_S		=(1<<18)
var   UFLAG_T		=(1<<19)
var   UFLAG_U		=(1<<20)
var   UFLAG_V		=(1<<21)
var   UFLAG_W		=(1<<22)
var   UFLAG_X		=(1<<23)
var   UFLAG_Y		=(1<<24)
var   UFLAG_Z		=(1<<25)

						    	/********************************************/
    							/* Bits in 'mode' for getkey and getstr     */
							    /********************************************/
var   K_UPPER 		=(1<<0)		/* Converts all letters to upper case		*/
var   K_UPRLWR		=(1<<1)		/* Upper/Lower case automatically			*/
var   K_NUMBER		=(1<<2)		/* Allow numbers only						*/
var   K_WRAP		=(1<<3)		/* Allows word wrap 						*/
var   K_MSG			=(1<<4)		/* Allows ANSI, ^N ^A ^G					*/
var   K_SPIN		=(1<<5)		/* Spinning cursor (same as SPIN)			*/
var   K_LINE		=(1<<6)		/* Input line (inverse color)				*/
var   K_EDIT		=(1<<7)		/* Edit string passed						*/
var   K_CHAT		=(1<<8)		/* In chat multi-chat						*/
var   K_NOCRLF		=(1<<9)		/* Don't print CRLF after string input      */
var   K_ALPHA 		=(1<<10)	/* Only allow alphabetic characters 		*/
var   K_GETSTR		=(1<<11)	/* getkey called from getstr()				*/
var   K_LOWPRIO		=(1<<12)	/* low priority input						*/
var   K_NOEXASC		=(1<<13)	/* No extended ASCII allowed				*/
var   K_E71DETECT	=(1<<14)	/* Detect E-7-1 terminal type				*/
var   K_AUTODEL		=(1<<15)	/* Auto-delete text (used with K_EDIT)		*/
var   K_COLD		=(1<<16)	/* Possible cold key mode					*/
var   K_NOECHO		=(1<<17)	/* Don't echo input                         */
var   K_TAB			=(1<<18)	/* Treat TAB key as CR						*/
					    		/********************************************/
						
						    	/********************************************/
    							/* Bits in 'mode' for putmsg and printfile  */
							    /********************************************/
var   P_NOABORT  	=(1<<0)		/* Disallows abortion of a message          */
var   P_SAVEATR		=(1<<1)		/* Save the new current attributres after	*/
					    		/* msg has printed */
var   P_NOATCODES	=(1<<2)		/* Don't allow @ codes                      */
var   P_OPENCLOSE	=(1<<3)		/* Open and close the file					*/
							    /********************************************/

    							/********************************************/
							    /* system.new_user_questions				*/
							    /********************************************/
var   UQ_ALIASES	=(1<<0) 	/* Ask for alias							*/
var   UQ_LOCATION	=(1<<1) 	/* Ask for location 						*/
var   UQ_ADDRESS	=(1<<2) 	/* Ask for address							*/
var   UQ_PHONE		=(1<<3) 	/* Ask for phone number 					*/
var   UQ_HANDLE		=(1<<4) 	/* Ask for chat handle						*/
var   UQ_DUPHAND	=(1<<5) 	/* Search for duplicate handles 			*/
var   UQ_SEX		=(1<<6) 	/* Ask for sex								*/
var   UQ_BIRTH		=(1<<7) 	/* Ask for birth date						*/
var   UQ_COMP 		=(1<<8) 	/* Ask for computer type					*/
var   UQ_MC_COMP	=(1<<9) 	/* Multiple choice computer type			*/
var   UQ_REALNAME	=(1<<10)	/* Ask for real name						*/
var   UQ_DUPREAL	=(1<<11)	/* Search for duplicate real names			*/
var   UQ_COMPANY	=(1<<12)	/* Ask for company name 					*/
var   UQ_NOEXASC	=(1<<13)	/* Don't allow ex-ASCII in user text		*/
var   UQ_CMDSHELL	=(1<<14)	/* Ask for command shell					*/
var   UQ_XEDIT		=(1<<15)	/* Ask for external editor					*/
var   UQ_NODEF		=(1<<16)	/* Don't ask for default settings			*/
var   UQ_NOCOMMAS	=(1<<17)	/* Do not require commas in location		*/
					    		/********************************************/
				
							    /********************************************/
							    /* node.settings							*/
							    /********************************************/
var   NM_ANSALARM	=(1<<0)		/* Alarm locally on answer					*/
var   NM_WFCSCRN	=(1<<1)		/* Wait for call screen                     */
var   NM_WFCMSGS	=(1<<2)		/* Include total messages/files on WFC		*/
var   NM_LCL_EDIT	=(1<<3)		/* Use local editor to create messages		*/
var   NM_EMSOVL		=(1<<4)		/* Use expanded memory of overlays			*/
var   NM_WINOS2		=(1<<5)		/* Use Windows/OS2 time slice API call		*/
var   NM_INT28		=(1<<6)		/* Make int 28 DOS idle calls				*/
var   NM_NODV 		=(1<<7)		/* Don't detect and use DESQview API        */
var   NM_NO_NUM		=(1<<8)		/* Don't allow logons by user number        */
var   NM_LOGON_R	=(1<<9)		/* Allow logons by user real name			*/
var   NM_LOGON_P	=(1<<10)	/* Secure logons (always ask for password)	*/
var   NM_NO_LKBRD	=(1<<11)	/* No local keyboard (at all)				*/
var   NM_SYSPW		=(1<<12)	/* Protect WFC keys and Alt keys with SY:	*/
var   NM_NO_INACT	=(1<<13)	/* No local inactivity alert/logoff 		*/
var   NM_NOBEEP		=(1<<14)	/* Don't beep locally                       */
var   NM_LOWPRIO	=(1<<15)	/* Always use low priority input			*/
var   NM_7BITONLY	=(1<<16)	/* Except 7-bit input only (E71 terminals)	*/
var   NM_RESETVID	=(1<<17)	/* Reset video mode between callers?		*/
var   NM_NOPAUSESPIN=(1<<18)	/* No spinning cursor at pause prompt		*/
					    		/********************************************/

						    	/********************************************/
							    /* netmail_misc								*/
							    /********************************************/
var   NMAIL_ALLOW 	=(1<<0)		/* Allow NetMail							*/
var   NMAIL_CRASH 	=(1<<1)		/* Default netmail to crash					*/
var   NMAIL_HOLD	=(1<<2)		/* Default netmail to hold					*/
var   NMAIL_KILL	=(1<<3)		/* Default netmail to kill after sent		*/
var   NMAIL_ALIAS 	=(1<<4)		/* Use Aliases in NetMail					*/
var   NMAIL_FILE	=(1<<5)		/* Allow file attachments					*/
var   NMAIL_DIRECT	=(1<<6)		/* Default netmail to direct				*/
					    		/********************************************/

    							/********************************************/
							    /* Bit values for sub[x].settings			*/
							    /********************************************/
var   SUB_QNET		=(1<<3) 	/* Sub-board is netted via QWK network		*/
var   SUB_PNET		=(1<<4) 	/* Sub-board is netted via PostLink			*/
var   SUB_FIDO		=(1<<5) 	/* Sub-board is netted via FidoNet			*/
var   SUB_PRIV		=(1<<6) 	/* Allow private posts on sub				*/
var   SUB_PONLY		=(1<<7) 	/* Private posts only						*/
var   SUB_ANON		=(1<<8) 	/* Allow anonymous posts on sub				*/
var   SUB_AONLY		=(1<<9) 	/* Anonymous only							*/
var   SUB_NAME		=(1<<10)	/* Must use real names						*/
var   SUB_DEL 		=(1<<11)	/* Allow users to delete messages			*/
var   SUB_DELLAST	=(1<<12)	/* Allow users to delete last msg only		*/
var   SUB_FORCED	=(1<<13)	/* Sub-board is forced scanning				*/
var   SUB_NOTAG		=(1<<14)	/* Don't add tag or origin lines			*/
var   SUB_TOUSER	=(1<<15)	/* Prompt for to user on posts				*/
var   SUB_ASCII		=(1<<16)	/* ASCII characters only					*/
var   SUB_QUOTE		=(1<<17)	/* Allow online quoting						*/
var   SUB_NSDEF		=(1<<18)	/* New-Scan on by default					*/
var   SUB_INET		=(1<<19)	/* Sub-board is netted via Internet			*/
var   SUB_FAST		=(1<<20)	/* Fast storage mode						*/
var   SUB_KILL		=(1<<21)	/* Kill read messages automatically			*/
var   SUB_KILLP		=(1<<22)	/* Kill read pvt messages automatically		*/
var   SUB_SYSPERM	=(1<<23)	/* Sysop messages are permament				*/
var   SUB_GATE		=(1<<24)	/* Gateway between Network types			*/
var   SUB_LZH 		=(1<<25)	/* Use LZH compression for msgs				*/
var   SUB_SSDEF		=(1<<26)	/* Default ON for Scan for Your msgs		*/
var   SUB_HYPER		=(1<<27)	/* Hyper allocation							*/
					    		/********************************************/

    							/********************************************/
                                /* Bit values for dir[x].settings			*/
							    /********************************************/
var   DIR_FCHK		=(1<<0) 	/* Check for file existance					*/
var   DIR_RATE		=(1<<1) 	/* Force uploads to be rated G,R, or X		*/
var   DIR_MULT		=(1<<2) 	/* Ask for multi-disk numbering				*/
var   DIR_DUPES		=(1<<3) 	/* Search this dir for upload dupes			*/
var   DIR_FREE		=(1<<4) 	/* Free downloads							*/
var   DIR_TFREE		=(1<<5) 	/* Time to download is free					*/
var   DIR_CDTUL		=(1<<6) 	/* Credit Uploads							*/
var   DIR_CDTDL		=(1<<7) 	/* Credit Downloads							*/
var   DIR_ANON		=(1<<8) 	/* Anonymous uploads						*/
var   DIR_AONLY		=(1<<9) 	/* Anonymous only							*/
var   DIR_ULDATE	=(1<<10)	/* Include upload date in listing			*/
var   DIR_DIZ 		=(1<<11)	/* FILE_ID.DIZ and DESC.SDI support			*/
var   DIR_NOSCAN	=(1<<12)	/* Don't new-scan this directory			*/
var   DIR_NOAUTO	=(1<<13)	/* Don't auto-add this directory			*/
var   DIR_ULTIME	=(1<<14)	/* Deduct time during uploads				*/
var   DIR_CDTMIN	=(1<<15)	/* Give uploader minutes instead of cdt		*/
var   DIR_SINCEDL	=(1<<16)	/* Purge based on days since last dl		*/
var   DIR_MOVENEW	=(1<<17)	/* Files marked as new when moved			*/
					    		/********************************************/

					    		/********************************************/
								/* Bits in xtrn[x] and xedit[x].settings	*/
					    		/********************************************/
var XTRN_MULTIUSER	=(1<<0) 	/* allow multi simultaneous users			*/
var XTRN_ANSI		=(1<<1) 	/* user must have ANSI, same as ^^^			*/
var XTRN_IO_INTS 	=(1<<2) 	/* Intercept I/O interrupts 				*/
var XTRN_MODUSERDAT	=(1<<3) 	/* Program can modify user data 			*/
var XTRN_WWIVCOLOR	=(1<<4) 	/* Program uses WWIV color codes			*/
var XTRN_EVENTONLY	=(1<<5) 	/* Program executes as event only			*/
var XTRN_STARTUPDIR	=(1<<6) 	/* Create drop file in start-up dir			*/
var XTRN_REALNAME	=(1<<7) 	/* Use real name in drop file				*/
var XTRN_SWAP		=(1<<8) 	/* Swap for this door						*/
var XTRN_FREETIME	=(1<<9) 	/* Free time while in this door 			*/
var XTRN_QUICKBBS	=(1<<10)	/* QuickBBS style editor					*/
var XTRN_EXPANDLF	=(1<<11)	/* Expand LF to CRLF editor 				*/
var XTRN_QUOTEALL	=(1<<12)	/* Automatically quote all of msg			*/
var XTRN_QUOTENONE	=(1<<13)	/* Automatically quote none of msg			*/
var XTRN_NATIVE		=(1<<14)	/* Native application (EX_NATIVE)			*/
var XTRN_STRIPKLUDGE=(1<<15)	/* Strip FTN Kludge lines from msg			*/
var XTRN_CHKTIME	=(1<<16)	/* Check time online (EX_CHKTIME)			*/
					    		/********************************************/

    							/********************************************/
				                /* Bit values for file.settings				*/
							    /********************************************/
var   FILE_EXTDESC	=(1<<0)     /* Extended description exists				*/
var   FILE_ANON 	=(1<<1)		/* Anonymous upload							*/
					    		/********************************************/

					    		/********************************************/
								/* Bits in the mode of bbs.exec()           */
					    		/********************************************/
var   EX_SH			=(1<<0)		/* Use system shell to load other process   */
var   EX_OUTR		=(1<<1)		/* Copy DOS output to remote                */
var   EX_OUTL 		=(1<<2)		/* Use _lputc() for local DOS output		*/
var   EX_INR		=(1<<3)		/* Trap int 16h keyboard input requests     */
var   EX_WWIV 		=(1<<4)		/* Expand WWIV color codes to ANSI sequence */
var   EX_SWAP 		=(1<<5)		/* Swap out for this external				*/
var   EX_POPEN		=(1<<7)		/* Leave COM port open						*/
var   EX_OFFLINE	=(1<<8)		/* Run this program offline					*/
var   EX_BG			=(1<<10)	/* Back-ground/detached process				*/
var   EX_BIN		=(1<<11)	/* Binary mode (no Unix LF to CR/LF)		*/
var   EX_NATIVE		=(1<<14)	/* Native 32-bit application (XTRN_NATIVE)	*/
var   EX_CHKTIME	=(1<<16)	/* Check time left (XTRN_CHKTIME)			*/
					    		/********************************************/

					    		/********************************************/
								/* Values for bbs.user_event()				*/
					    		/********************************************/
var   EVENT_LOGON	=1			/* Execute during logon sequence			*/
var   EVENT_LOGOFF	=2			/* Execute during logoff sequence			*/
var   EVENT_NEWUSER	=3			/* Execute during newuser app.				*/
var   EVENT_BIRTHDAY=4			/* Execute on birthday						*/
					    		/********************************************/

					    		/********************************************/
								/* Bits in mode of bbs.telnet_gate()		*/
					    		/********************************************/
var   TG_ECHO		=(1<<0)		/* Turn on telnet echo						*/
var   TG_CRLF		=(1<<1)		/* Expand sole CR to CRLF					*/
var   TG_LINEMODE	=(1<<2)		/* Send entire lines only					*/
var   TG_NODESYNC	=(1<<3)		/* Call Nodesync, get msgs, etc.			*/
var   TG_CTRLKEYS	=(1<<4)		/* Interpret ^P ^U ^T, etc locally			*/
var   TG_PASSTHRU	=(1<<5)		/* Pass-through telnet commands/responses	*/
var   TG_RLOGIN		=(1<<6)		/* Use BSD RLogin protocol					*/
					    		/********************************************/

					    		/********************************************/
								/* Bits in console.telnet_mode				*/
					    		/********************************************/
var TELNET_MODE_BIN_RX	=(1<<0) /* Binary receive (no CR to CRLF xlat)		*/
var TELNET_MODE_ECHO	=(1<<1)	/* Echo remotely							*/
var TELNET_MODE_GATE	=(1<<2)	/* Pass-through telnet commands/responses	*/
					    		/********************************************/

					    		/********************************************/
								/* Bits in mode of bbs.scan_posts()			*/
					    		/********************************************/
var	SCAN_CONST		=(1<<0)		/* Continuous message scanning				*/
var	SCAN_NEW		=(1<<1)		/* New scanning								*/
var	SCAN_BACK		=(1<<2)		/* Scan the last message if no new			*/
var	SCAN_TOYOU		=(1<<3)		/* Scan for messages to you 				*/
var	SCAN_FIND		=(1<<4)		/* Scan for text in messages				*/
var	SCAN_UNREAD		=(1<<5)		/* Find un-read messages to you 			*/
					    		/********************************************/

					    		/********************************************/
								/* Values of mode for bbs.list_users()		*/
					    		/********************************************/
var UL_ALL			=0			/* List all active users in user database	*/
var UL_SUB			=1    		/* List all users with access to cursub     */
var UL_DIR			=2			/* List all users with access to curdir 	*/
					    		/********************************************/

					    		/********************************************/
								/* Values of mode in bbs.list_file_info()	*/
					    		/********************************************/
var FI_INFO         =0   		/* Just list file information               */
var FI_REMOVE       =1    		/* Remove/Move/Edit file information        */
var FI_DOWNLOAD     =2    		/* Download files                           */
var FI_OLD          =3    		/* Search/Remove files not downloaded since */
var FI_OLDUL	 	=4			/* Search/Remove files uploaded before      */
var FI_OFFLINE   	=5			/* Search/Remove files not online			*/
var FI_USERXFER  	=6			/* User Xfer Download                       */
var FI_CLOSE 	  	=7			/* Close any open records					*/
					    		/********************************************/

								/* Message attributes */
var MSG_PRIVATE 		=(1<<0)
var MSG_READ			=(1<<1)
var MSG_PERMANENT		=(1<<2)
var MSG_LOCKED			=(1<<3)
var MSG_DELETE			=(1<<4)
var MSG_ANONYMOUS		=(1<<5)
var MSG_KILLREAD		=(1<<6)
var MSG_MODERATED		=(1<<7)
var MSG_VALIDATED		=(1<<8)
var MSG_REPLIED			=(1<<9)	// User replied to this message

								/* Auxillary header attributes */
var MSG_FILEREQUEST 	=(1<<0)	// File request
var MSG_FILEATTACH		=(1<<1)	// File(s) attached to Msg
var MSG_TRUNCFILE		=(1<<2)	// Truncate file(s) when sent
var MSG_KILLFILE		=(1<<3)	// Delete file(s) when sent
var MSG_RECEIPTREQ		=(1<<4)	// Return receipt requested
var MSG_CONFIRMREQ		=(1<<5)	// Confirmation receipt requested
var MSG_NODISP			=(1<<6)	// Msg may not be displayed to user

								/* Message network attributes */
var MSG_LOCAL			=(1<<0)	// Msg created locally
var MSG_INTRANSIT		=(1<<1)	// Msg is in-transit
var MSG_SENT			=(1<<2)	// Sent to remote
var MSG_KILLSENT		=(1<<3)	// Kill when sent
var MSG_ARCHIVESENT 	=(1<<4)	// Archive when sent
var MSG_HOLD			=(1<<5)	// Hold for pick-up
var MSG_CRASH			=(1<<6)	// Crash
var MSG_IMMEDIATE		=(1<<7)	// Send Msg now, ignore restrictions
var MSG_DIRECT			=(1<<8)	// Send directly to destination
var MSG_GATE			=(1<<9)	// Send via gateway
var MSG_ORPHAN			=(1<<10)// Unknown destination
var MSG_FPU 			=(1<<11)// Force pickup
var MSG_TYPELOCAL		=(1<<12)// Msg is for local use only
var MSG_TYPEECHO		=(1<<13)// Msg is for conference distribution
var MSG_TYPENET 		=(1<<14)// Msg is direct network mail

								/* Net types */
var NET_NONE			=0		// Local message	
var NET_UNKNOWN			=1		// Networked, but unknown type
var NET_FIDO			=2		// FidoNet
var NET_POSTLINK		=3		// PostLink
var NET_QWK				=4		// QWK
var NET_INTERNET		=5		// NNTP
var NET_WWIV			=6		// WWIV
var NET_MHS				=7		// MHS
