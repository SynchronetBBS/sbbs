/* XSDKDEFS.H */

#ifndef _XSDKDEFS_H
#define _XSDKDEFS_H

/**********/
/* Macros */
/**********/
									/* Control characters */
#define STX 	0x02				/* Start of text			^B	*/
#define ETX 	0x03				/* End of text				^C	*/
#define BS		0x08				/* Back space				^H	*/
#define TAB 	0x09				/* Horizontal tabulation	^I	*/
#define LF		0x0a				/* Line feed				^J	*/
#define FF		0x0c				/* Form feed				^L	*/
#define CR		0x0d				/* Carriage return			^M	*/
#define ESC 	0x1b				/* Escape					^[	*/
#define SP      0x20                /* Space                        */


#define DCD    0x8000		/* Data carrier detect bit in msr			*/

#define TABSIZE 	4		/* Tabs will expand to 4 chars upon input	*/

#define SAVE_LINES	 4		/* Maximum number of lines to save			*/
#define LINE_BUFSIZE 512    /* Size of line output buffer               */

#define HIGH 8				/* High intensity attribute bit */

							/* user_misc bits */
#define ANSI	(1<<0)		/* user has ANSI capability */
#define COLOR	(1<<1)		/* user has monochrome ANSI display */
#define RIP 	(1<<2)		/* RIP compatible terminal detected */
#define WIP 	(1<<3)		/* WIP compatible terminal detected */

#ifndef uchar				/* Short-hand for unsigned data types */
#define uchar unsigned char
#endif
#ifndef uint
#define uint unsigned int
#endif
#ifndef ushort
#define ushort unsigned short
#endif
#ifndef ulong
#define ulong unsigned long
#endif

							/* Bits in 'mode' for getkey and getstr     */
#define K_UPPER		(1<<0)	/* Converts all letters to upper case 		*/
#define K_UPRLWR    (1<<1)  /* Upper/Lower case automatically           */
#define K_NUMBER    (1<<2)  /* Allow numbers only                       */
#define K_WRAP	    (1<<3)  /* Allows word wrap							*/
#define K_MSG		(1<<4)  /* Allows ANSI, ^N ^A ^G					*/
#define K_SPIN      (1<<5)  /* Spinning cursor (same as SPIN)           */
#define K_LINE      (1<<6)  /* Input line (inverse color)               */
#define K_EDIT      (1<<7)  /* Edit string passed                       */
#define K_CHAT		(1<<8)	/* In chat multi-chat 						*/
#define K_NOCRLF    (1<<9)	/* Don't print CRLF after string input      */
#define K_ALPHA     (1<<10) /* Only allow alphabetic characters         */
#define K_AUTODEL	(1<<11) /* Auto-delete text (used with K_EDIT)		*/
#define K_LOWPRIO	(1<<12) /* Low priority getstr() operation			*/
#define K_GETSTR	(1<<13) /* getkey() called from getstr()			*/

							/* Miscellaneous Modem Settings (mdm_misc)	*/
#define MDM_FLOWCTRL (1<<0)	/* Use flow control with modem				*/
#define MDM_STAYHIGH (1<<1) /* Stay at highest DTE rate                 */

#define LOOP_NOPEN   50    	/* Retries before file access denied        */
#define LOOP_NODEDAB 100	/* Retries on NODE.DAB locking/unlocking	*/

#define MAX_NODES	250 	/* Maximum number of nodes					*/

enum {								/* Node Status */
	 NODE_WFC			        	/* Waiting for Call */
	,NODE_LOGON                  	/* at logon prompt */
	,NODE_NEWUSER         			/* New user applying */
	,NODE_INUSE			 			/* In Use */
	,NODE_QUIET			 			/* In Use - quiet mode */
	,NODE_OFFLINE		 			/* Offline */
	,NODE_NETTING		 			/* Networking */
	,NODE_EVENT_WAITING				/* Waiting for all nodes to be inactive */
	,NODE_EVENT_RUNNING				/* Running an external event */
	,NODE_EVENT_LIMBO				/* Allowing another node to run an event */
	};

									/* Bit values for node.misc */
#define	NODE_ANON	(1<<0)			/* Anonymous User */
#define	NODE_LOCK	(1<<1)			/* Locked for sysops only */
#define	NODE_INTR	(1<<2)			/* Interrupted - hang up */
#define NODE_MSGW   (1<<3)			/* Message is waiting */
#define NODE_POFF   (1<<4)        	/* Page disabled */
#define NODE_AOFF	(1<<5)			/* Activity Alert disabled */
#define NODE_UDAT	(1<<6)			/* User data has been updated */
#define NODE_RRUN	(1<<7)			/* Re-run this node when log off */
#define NODE_EVENT	(1<<8)			/* Must run node event after log off */
#define NODE_DOWN	(1<<9)			/* Down this node after logoff */
#define NODE_RPCHT	(1<<10) 		/* Reset private chat */
#define NODE_NMSG   (1<<11)         /* Node message waiting (new way) */


enum {								/* Node Action */
	 NODE_MAIN						/* Main Prompt */
	,NODE_RMSG			 			/* Reading Messages */
	,NODE_RMAL						/* Reading Mail */
	,NODE_SMAL			 			/* Sending Mail */
	,NODE_RTXT			 			/* Reading G-Files */
	,NODE_RSML			 			/* Reading Sent Mail */
	,NODE_PMSG			 			/* Posting Message */
	,NODE_AMSG			 			/* Auto-message */
	,NODE_XTRN			  			/* Running External Program */
	,NODE_DFLT			  			/* Main Defaults Section */
	,NODE_XFER			   			/* Transfer Prompt */
	,NODE_DLNG		    			/* Downloading File */
	,NODE_ULNG			    		/* Uploading File */
	,NODE_BXFR						/* Bidirectional Transfer */
	,NODE_LFIL			    		/* Listing Files */
	,NODE_LOGN			    		/* Logging on */
	,NODE_LCHT			    		/* In Local Chat with Sysop */
	,NODE_MCHT		     			/* In Multi-Chat with Other Nodes */
	,NODE_GCHT			   			/* In Local Chat with Guru */
	,NODE_CHAT			   			/* In Chat Section */
	,NODE_SYSP		     			/* Sysop Activity */
	,NODE_TQWK						/* Transferring QWK packet */
	,NODE_PCHT						/* Private node-to-node chat */
	,NODE_PAGE						/* Paging node for Private Chat */
	,NODE_RFSD						/* Retrieving file from sequential dev */
	};

							/* Different bits in node_misc				*/
#define NM_ANSALARM (1<<0)	/* Alarm locally on answer					*/
#define NM_WFCSCRN  (1<<1)	/* Wait for call screen                     */
#define NM_WFCMSGS	(1<<2)	/* Include total messages/files on WFC		*/
#define NM_LCL_EDIT (1<<3)	/* Use local editor to create messages		*/
#define NM_EMSOVL	(1<<4)	/* Use expanded memory of overlays			*/
#define NM_WINOS2	(1<<5)	/* Use Windows/OS2 time slice API call		*/
#define NM_INT28	(1<<6)	/* Make int 28 DOS idle calls				*/
#define NM_NODV 	(1<<7)	/* Don't detect and use DESQview API        */
#define NM_NO_NUM	(1<<8)	/* Don't allow logons by user number        */
#define NM_LOGON_R	(1<<9)	/* Allow logons by user real name			*/
#define NM_LOGON_P	(1<<10) /* Secure logons (always ask for password)	*/
#define NM_NO_LKBRD (1<<11) /* No local keyboard (at all)				*/
#define NM_SYSPW	(1<<12) /* Protect WFC keys and Alt keys with SY:	*/
#define NM_NO_INACT (1<<13) /* No local inactivity alert/logoff 		*/
#define NM_NOBEEP	(1<<14) /* Don't beep locally                       */
#define NM_LOWPRIO	(1<<15) /* Always use low priority input			*/
#define NM_7BITONLY (1L<<16) /* Except 7-bit input only (E71 terminals)  */

#ifdef __WATCOMC__

	#if !defined(__COLORS)
	#define __COLORS

	enum COLORS {
		BLACK,			/* dark colors */
		BLUE,
		GREEN,
		CYAN,
		RED,
		MAGENTA,
		BROWN,
		LIGHTGRAY,
		DARKGRAY,		/* light colors */
		LIGHTBLUE,
		LIGHTGREEN,
		LIGHTCYAN,
		LIGHTRED,
		LIGHTMAGENTA,
		YELLOW,
		WHITE
	};
	#endif

	#define BLINK		128 /* blink bit */

	#define ffblk find_t
    #define findfirst(x,y,z) _dos_findfirst(x,z,y)
	#define findnext(x) _dos_findnext(x)
	#pragma aux mswait "_*"
#endif

/****************************************************************************/
/* MALLOC/FREE Macros for various compilers and environments				*/
/* MALLOC is used for allocations of 64k or less							*/
/* FREE is used to free buffers allocated with MALLOC						*/
/* LMALLOC is used for allocations of possibly larger than 64k				*/
/* LFREE is used to free buffers allocated with LMALLOC 					*/
/* REALLOC is used to re-size a previously MALLOCed or LMALLOCed buffer 	*/
/****************************************************************************/
#if defined(__COMPACT__) || defined(__LARGE__) || defined(__HUGE__)
	#if defined(__TURBOC__)
		#define REALLOC(x,y) farrealloc(x,y)
		#define LMALLOC(x) farmalloc(x)
		#define MALLOC(x) farmalloc(x)
		#define LFREE(x) farfree(x)
		#define FREE(x) farfree(x)
	#elif defined(__WATCOMC__)
		#define REALLOC realloc
		#define LMALLOC(x) halloc(x,1)	/* far heap, but slow */
		#define MALLOC malloc			/* far heap, but 64k max */
		#define LFREE hfree
		#define FREE free
	#else	/* Other 16-bit Compiler */
		#define REALLOC realloc
		#define LMALLOC malloc
		#define MALLOC malloc
		#define LFREE free
		#define FREE free
	#endif
#else		/* 32-bit Compiler or Small Memory Model */
	#define REALLOC realloc
	#define LMALLOC malloc
	#define MALLOC malloc
	#define LFREE free
	#define FREE free
#endif

#define KEY_BUFSIZE 256

#define CRLF  { outchar(CR); outchar(LF); }

#define SYSOP (user_level>=sysop_level) /* Is current user sysop t/f macro	 */

#define bgotoxy(x,y)	printf("\x1b[%d;%dH",y,x)   /* place the cursor at   */
													/* location x,y via ANSI */

#define SAVELINE		{ slatr[slcnt]=latr; \
							sprintf(slbuf[slcnt<SAVE_LINES ? slcnt++ : slcnt] \
							,"%.*s",lbuflen,lbuf); \
							lbuflen=0; }
#define RESTORELINE 	{ lbuflen=0; attr(slatr[--slcnt]); \
							bputs(slbuf[slcnt]); }

/************/
/* Typedefs */
/************/

typedef struct {						/* Node information kept in NODE.DAB */
	uchar	status,						/* Current Status of Node */
			errors,						/* Number of Critical Errors */
			action;						/* Action User is doing on Node */
	uint	useron,						/* User on Node */
			connection,					/* Connection rate of Node */
			misc,						/* Miscellaneous bits for node */
			aux;						/* Auxillary word for node */
	ulong	extaux;						/* Extended aux dword for node */
            } node_t;

#endif	/* Don't add anything after this endif */
