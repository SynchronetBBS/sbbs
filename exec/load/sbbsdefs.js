/* sbbsdefs.js */

/* $id$ */

									/* User Questions						*/
UQ_ALIASES		=(1<<0) 	/* Ask for alias						*/
UQ_LOCATION		=(1<<1) 	/* Ask for location 					*/
UQ_ADDRESS		=(1<<2) 	/* Ask for address						*/
UQ_PHONE		=(1<<3) 	/* Ask for phone number 				*/
UQ_HANDLE		=(1<<4) 	/* Ask for chat handle					*/
UQ_DUPHAND		=(1<<5) 	/* Search for duplicate handles 		*/
UQ_SEX			=(1<<6) 	/* Ask for sex							*/
UQ_BIRTH		=(1<<7) 	/* Ask for birth date					*/
UQ_COMP 		=(1<<8) 	/* Ask for computer type				*/
UQ_MC_COMP		=(1<<9) 	/* Multiple choice computer type		*/
UQ_REALNAME		=(1<<10)	/* Ask for real name					*/
UQ_DUPREAL		=(1<<11)	/* Search for duplicate real names		*/
UQ_COMPANY		=(1<<12)	/* Ask for company name 				*/
UQ_NOEXASC		=(1<<13)	/* Don't allow ex-ASCII in user text    */
UQ_CMDSHELL		=(1<<14)	/* Ask for command shell				*/
UQ_XEDIT		=(1<<15)	/* Ask for external editor				*/
UQ_NODEF		=(1<<16)	/* Don't ask for default settings       */
UQ_NOCOMMAS		=(1<<17)	/* Do not require commas in location	*/
						
						
									/* Different bits in sys_misc				*/
SM_CLOSED		=(1<<0) 	/* System is clsoed to New Users			*/
SM_SYSSTAT		=(1<<1) 	/* Sysops activity included in statistics	*/
SM_NOBEEP		=(1<<2) 	/* No beep sound locally					*/
SM_PWEDIT		=(1<<3) 	/* Allow users to change their passwords	*/
SM_RA_EMU		=(1<<4) 	/* Reverse R/A commands at msg read prompt	*/
SM_ANON_EM		=(1<<5) 	/* Allow anonymous e-mail					*/
SM_LISTLOC		=(1<<6) 	/* Use location of caller in user lists 	*/
SM_WILDCAT		=(1<<7) 	/* Expand Wildcat color codes in messages	*/
SM_PCBOARD		=(1<<8) 	/* Expand PCBoard color codes in messages	*/
SM_WWIV 		=(1<<9) 	/* Expand WWIV color codes in messages		*/
SM_CELERITY		=(1<<10)	/* Expand Celerity color codes in messages	*/
SM_RENEGADE		=(1<<11)	/* Expand Renegade color codes in messages	*/
SM_ECHO_PW		=(1<<12)	/* Echo passwords locally					*/
SM_REQ_PW		=(1<<13)	/* Require passwords locally				*/
SM_L_SYSOP		=(1<<14)	/* Allow local sysop logon/commands 		*/
SM_R_SYSOP		=(1<<15)	/* Allow remote sysop logon/commands		*/
SM_QUOTE_EM		=(1<<16)	/* Allow quoting of e-mail					*/
SM_EURODATE		=(1<<17)	/* Europian date format (DD/MM/YY)			*/
SM_MILITARY		=(1<<18)	/* Military time format 					*/
SM_TIMEBANK		=(1<<19)	/* Allow time bank functions				*/
SM_FILE_EM		=(1<<20)	/* Allow file attachments in E-mail 		*/
SM_SHRTPAGE		=(1<<21)	/* Short sysop page 						*/
SM_TIME_EXP		=(1<<22)	/* Set to expired values if out-of-time 	*/
SM_FASTMAIL		=(1<<23)	/* Fast e-mail storage mode 				*/
SM_QVALKEYS		=(1<<24)	/* Quick validation keys enabled			*/
SM_ERRALARM		=(1<<25)	/* Error beeps on							*/
SM_FWDTONET		=(1<<26)	/* Allow forwarding of e-mail to netmail	*/
SM_DELREADM		=(1<<27)	/* Delete read mail automatically			*/
SM_NOCDTCVT		=(1<<28)	/* No credit to minute conversions allowed	*/
SM_DELEMAIL		=(1<<29)	/* Physically remove deleted e-mail immed.	*/
SM_USRVDELM		=(1<<30)	/* Users can see deleted msgs				*/
SM_SYSVDELM		=(1<<31)	/* Sysops can see deleted msgs				*/
						
									/* Different bits in node_misc				*/
NM_ANSALARM		=(1<<0)		/* Alarm locally on answer					*/
NM_WFCSCRN		=(1<<1)		/* Wait for call screen                     */
NM_WFCMSGS		=(1<<2)		/* Include total messages/files on WFC		*/
NM_LCL_EDIT		=(1<<3)		/* Use local editor to create messages		*/
NM_EMSOVL		=(1<<4)		/* Use expanded memory of overlays			*/
NM_WINOS2		=(1<<5)		/* Use Windows/OS2 time slice API call		*/
NM_INT28		=(1<<6)		/* Make int 28 DOS idle calls				*/
NM_NODV 		=(1<<7)		/* Don't detect and use DESQview API        */
NM_NO_NUM		=(1<<8)		/* Don't allow logons by user number        */
NM_LOGON_R		=(1<<9)		/* Allow logons by user real name			*/
NM_LOGON_P		=(1<<10)		/* Secure logons (always ask for password)	*/
NM_NO_LKBRD		=(1<<11)		/* No local keyboard (at all)				*/
NM_SYSPW		=(1<<12)		/* Protect WFC keys and Alt keys with SY:	*/
NM_NO_INACT		=(1<<13)		/* No local inactivity alert/logoff 		*/
NM_NOBEEP		=(1<<14)		/* Don't beep locally                       */
NM_LOWPRIO		=(1<<15)		/* Always use low priority input			*/
NM_7BITONLY		=(1<<16)	/* Except 7-bit input only (E71 terminals)	*/
NM_RESETVID		=(1<<17)	/* Reset video mode between callers?		*/
NM_NOPAUSESPIN	=(1<<18)	/* No spinning cursor at pause prompt		*/

									/* Miscellaneous Modem Settings (mdm_misc)  */
MDM_CTS 		=(1<<0)		/* Use hardware send flow control			*/
MDM_RTS 		=(1<<1)		/* Use hardware recv flow control			*/
MDM_STAYHIGH	=(1<<2)		/* Stay at highest DTE rate 				*/
MDM_CALLERID	=(1<<3)		/* Supports Caller ID						*/
MDM_DUMB		=(1<<4)		/* Just watch DCD for answer - dumb modem	*/
MDM_NODTR		=(1<<5)		/* Don't drop DTR for hang-up               */
MDM_KNOWNRES	=(1<<6)		/* Allow known result codes only			*/
MDM_VERBAL		=(1<<7)		/* Use verbal result codes					*/

						
									/* Bit values for level_misc[x] 	*/
LEVEL_EXPTOLVL =(1<<0)		/* Expire to level_expireto[x]		*/
LEVEL_EXPTOVAL =(1<<1)		/* Expire to val[level_expireto[x]] */

									/* Bit values for prot[x].misc */
PROT_DSZLOG =(1<<0)          /* Supports DSZ Log */

									/* Bit values in netmail_misc */

NMAIL_ALLOW 	=(1<<0)		/* Allow NetMail */
NMAIL_CRASH 	=(1<<1)		/* Default netmail to crash */
NMAIL_HOLD		=(1<<2)		/* Default netmail to hold */
NMAIL_KILL		=(1<<3)		/* Default netmail to kill after sent */
NMAIL_ALIAS 	=(1<<4)		/* Use Aliases in NetMail */
NMAIL_FILE		=(1<<5)		/* Allow file attachments */
NMAIL_DIRECT	=(1<<6)		/* Default netmail to direct */

									/* Attribute bits for fido msg header */
FIDO_PRIVATE	=(1<<0)		/* Private message */
FIDO_CRASH		=(1<<1)		/* Crash-mail (send immediately) */
FIDO_RECV		=(1<<2)		/* Received successfully */
FIDO_SENT		=(1<<3)		/* Sent successfully */
FIDO_FILE		=(1<<4)		/* File attached */
FIDO_INTRANS	=(1<<5)		/* In transit */
FIDO_ORPHAN 	=(1<<6)		/* Orphan */
FIDO_KILLSENT	=(1<<7)		/* Kill it after sending it */
FIDO_LOCAL		=(1<<8)		/* Created locally - on this system */
FIDO_HOLD		=(1<<9)		/* Hold - don't send it yet */
FIDO_FREQ		=(1<<11) 	/* File request */
FIDO_RRREQ		=(1<<12) 	/* Return receipt request */
FIDO_RR 		=(1<<13) 	/* This is a return receipt */
FIDO_AUDIT		=(1<<14) 	/* Audit request */
FIDO_FUPREQ 	=(1<<15) 	/* File update request */

									/* Bit values for sub_cfg and sav_sub_cfg	*/
SUB_CFG_NSCAN	=0x0005		/* bits 0 and 2								*/
SUB_CFG_SSCAN	=0x0002		/* bit 1									*/
SUB_CFG_YSCAN	=0x0100		/* bit 9 (bits 9-15 default to OFF)			*/

									/* Bit values for sub[x].misc */
SUB_QNET	=(1<<3) 		/* Sub-board is netted via QWK network */
SUB_PNET	=(1<<4) 		/* Sub-board is netted via PostLink */
SUB_FIDO	=(1<<5) 		/* Sub-board is netted via FidoNet */
SUB_PRIV	=(1<<6) 		/* Allow private posts on sub */
SUB_PONLY	=(1<<7) 		/* Private posts only */
SUB_ANON	=(1<<8) 		/* Allow anonymous posts on sub */
SUB_AONLY	=(1<<9) 		/* Anonymous only */
SUB_NAME	=(1<<10)		/* Must use real names */
SUB_DEL 	=(1<<11)		/* Allow users to delete messages */
SUB_DELLAST =(1<<12)		/* Allow users to delete last msg only */
SUB_FORCED	=(1<<13)		/* Sub-board is forced scanning */
SUB_NOTAG	=(1<<14)		/* Don't add tag or origin lines */
SUB_TOUSER	=(1<<15)		/* Prompt for to user on posts */
SUB_ASCII	=(1<<16)		/* ASCII characters only */
SUB_QUOTE	=(1<<17)		/* Allow online quoting */
SUB_NSDEF	=(1<<18)		/* New-Scan on by default */
SUB_INET	=(1<<19)		/* Sub-board is netted via Internet */
SUB_FAST	=(1<<20)		/* Fast storage mode */
SUB_KILL	=(1<<21)		/* Kill read messages automatically */
SUB_KILLP	=(1<<22)		/* Kill read pvt messages automatically */
SUB_SYSPERM =(1<<23)		/* Sysop messages are permament */
SUB_GATE	=(1<<24)		/* Gateway between Network types */
SUB_LZH 	=(1<<25)		/* Use LZH compression for msgs */
SUB_SSDEF	=(1<<26)		/* Default ON for Scan for Your msgs */
SUB_HYPER	=(1<<27)		/* Hyper allocation */

                                    /* Bit values for dir[x].misc */
DIR_FCHK	=(1<<0) 		/* Check for file existance */
DIR_RATE	=(1<<1) 		/* Force uploads to be rated G,R, or X */
DIR_MULT	=(1<<2) 		/* Ask for multi-disk numbering */
DIR_DUPES	=(1<<3) 		/* Search this dir for upload dupes */
DIR_FREE	=(1<<4) 		/* Free downloads */
DIR_TFREE	=(1<<5) 		/* Time to download is free */
DIR_CDTUL	=(1<<6) 		/* Credit Uploads */
DIR_CDTDL	=(1<<7) 		/* Credit Downloads */
DIR_ANON	=(1<<8) 		/* Anonymous uploads */
DIR_AONLY	=(1<<9) 		/* Anonymous only */
DIR_ULDATE	=(1<<10)		/* Include upload date in listing */
DIR_DIZ 	=(1<<11)		/* FILE_ID.DIZ and DESC.SDI support */
DIR_NOSCAN	=(1<<12)		/* Don't new-scan this directory */
DIR_NOAUTO	=(1<<13)		/* Don't auto-add this directory */
DIR_ULTIME	=(1<<14)		/* Deduct time during uploads */
DIR_CDTMIN	=(1<<15)		/* Give uploader minutes instead of cdt */
DIR_SINCEDL =(1<<16)		/* Purge based on days since last dl */
DIR_MOVENEW =(1<<17)		/* Files marked as new when moved */

                                    /* Bit values for file_t.misc */
FM_EXTDESC  =(1<<0)          /* Extended description exists */
FM_ANON 	=(1<<1)			/* Anonymous upload */
