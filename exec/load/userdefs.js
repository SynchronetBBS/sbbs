// $Id: userdefs.js,v 1.4 2020/08/04 04:59:07 rswindell Exp $

//**********************************************************************
// user.settings							
//**********************************************************************
var USER_DELETED      = (1<<0);	// Deleted user slot						
var USER_ANSI		    = (1<<1);	// Supports ANSI terminal emulation			
var USER_COLOR	    = (1<<2);	// Send color codes 						
var USER_RIP 		    = (1<<3);	// Supports RIP terminal emulation			
var USER_PAUSE	    = (1<<4);	// Pause on every screen full				
var USER_SPIN		    = (1<<5);	// Spinning cursor - Same as K_SPIN			
var USER_INACTIVE	    = (1<<6);	// Inactive user slot						
var USER_EXPERT	    = (1<<7);	// Expert menu mode 						
var USER_ANFSCAN 	    = (1<<8);	// Auto New file scan						
var USER_CLRSCRN 	    = (1<<9);	// Clear screen before each message			
var USER_QUIET	    = (1<<10);	// Quiet mode upon logon					
var USER_BATCHFLAG    = (1<<11);	// File list allow batch dl flags			
var USER_NETMAIL 	    = (1<<12);	// Forward e-mail to fidonet addr			
var USER_CURSUB	    = (1<<13);	// Remember current sub-board/dir			
var USER_ASK_NSCAN    = (1<<14);	// Ask for newscanning upon logon			
var USER_NO_EXASCII   = (1<<15);	// Don't send extended ASCII				
var USER_ASK_SSCAN    = (1<<16);	// Ask for messages to you at logon			
var USER_AUTOTERM	    = (1<<17);	// Autodetect terminal type 				
var USER_COLDKEYS	    = (1<<18);	// No hot-keys								
var USER_EXTDESC 	    = (1<<19);	// Extended file descriptions				
var USER_AUTOHANG	    = (1<<20);	// Auto-hang-up after transfer				
var USER_WIP 		    = (1<<21);	// Supports WIP terminal emulation			
var USER_AUTOLOGON    = (1<<22);	// AutoLogon via IP							
var USER_HTML		    = (1<<23);	// Using Deuce's HTML terminal (*cough*)	
var USER_NOPAUSESPIN  = (1<<24);	// No spinning cursor at pause prompt		
var USER_PETSCII      = (1<<26);	// Commodore PET (e.g. C64) terminal		
var USER_SWAP_DELETE  = (1<<27);	// Swap the DEL and backspace keys			
var USER_ICE_COLOR    = (1<<28);	// Bright background color support
var USER_UTF8 		= (1<<29);	// UTF-8 terminal
var USER_MOUSE		= (1<<31);	// Terminal supports mouse reporting

//**********************************************************************
// user.qwk_settings 						
//**********************************************************************
var QWK_FILES		= (1<<0);	// Include new files list					
var QWK_EMAIL		= (1<<1);	// Include unread e-mail					
var QWK_ALLMAIL	= (1<<2);	// Include ALL e-mail						
var QWK_DELMAIL	= (1<<3);	// Delete e-mail after download 			
var QWK_BYSELF	= (1<<4);	// Include messages from self				
var QWK_UNUSED	= (1<<5);	// Currently unused 						
var QWK_EXPCTLA	= (1<<6);	// Expand ctrl-a codes to ascii 			
var QWK_RETCTLA	= (1<<7);	// Retain ctrl-a codes						
var QWK_ATTACH	= (1<<8);	// Include file attachments 				
var QWK_NOINDEX	= (1<<9);	// Do not create index files in QWK			
var QWK_TZ		= (1<<10);	// Include @TZ (time zone) in msgs			
var QWK_VIA 		= (1<<11);	// Include @VIA (path) in msgs				
var QWK_NOCTRL	= (1<<12);	// No extraneous control files				
var QWK_EXT		= (1<<13);	// QWK Extended (QWKE) format				
var QWK_MSGID		= (1<<14);	// Include @MSGID and @REPLY in msgs		
var QWK_HEADERS	= (1<<16);	// Include HEADERS.DAT file					
var QWK_VOTING	= (1<<17);	// Include VOTING.DAT
var QWK_UTF8      = (1<<18);	// Include UTF-8 characters

//**********************************************************************
// user.chat_settings						
//**********************************************************************
var CHAT_ECHO		= (1<<0);	// Multinode chat echo						
var CHAT_ACTION	= (1<<1);	// Chat actions 							
var CHAT_NOPAGE	= (1<<2);	// Can't be paged                           
var CHAT_NOACT	= (1<<3);	// No activity alerts						
var CHAT_SPLITP	= (1<<4);	// Split screen private chat				

//**********************************************************************
// Valid flags for user.security.exempt/restrict/flags					
//**********************************************************************
var UFLAG_A		= (1<<0);
var UFLAG_B		= (1<<1);
var UFLAG_C		= (1<<2);
var UFLAG_D		= (1<<3);
var UFLAG_E		= (1<<4);
var UFLAG_F		= (1<<5);
var UFLAG_G		= (1<<6);
var UFLAG_H		= (1<<7);
var UFLAG_I		= (1<<8);
var UFLAG_J		= (1<<9);
var UFLAG_K		= (1<<10);
var UFLAG_L		= (1<<11);
var UFLAG_M		= (1<<12);
var UFLAG_N		= (1<<13);
var UFLAG_O		= (1<<14);
var UFLAG_P		= (1<<15);
var UFLAG_Q		= (1<<16);
var UFLAG_R		= (1<<17);
var UFLAG_S		= (1<<18);
var UFLAG_T		= (1<<19);
var UFLAG_U		= (1<<20);
var UFLAG_V		= (1<<21);
var UFLAG_W		= (1<<22);
var UFLAG_X		= (1<<23);
var UFLAG_Y		= (1<<24);
var UFLAG_Z		= (1<<25);

