// $Id: userdefs.js,v 1.4 2020/08/04 04:59:07 rswindell Exp $

//**********************************************************************
// user.settings							
//**********************************************************************
const USER_DELETED      = (1<<0);	// Deleted user slot						
const USER_ANSI		    = (1<<1);	// Supports ANSI terminal emulation			
const USER_COLOR	    = (1<<2);	// Send color codes 						
const USER_RIP 		    = (1<<3);	// Supports RIP terminal emulation			
const USER_PAUSE	    = (1<<4);	// Pause on every screen full				
const USER_SPIN		    = (1<<5);	// Spinning cursor - Same as K_SPIN			
const USER_INACTIVE	    = (1<<6);	// Inactive user slot						
const USER_EXPERT	    = (1<<7);	// Expert menu mode 						
const USER_ANFSCAN 	    = (1<<8);	// Auto New file scan						
const USER_CLRSCRN 	    = (1<<9);	// Clear screen before each message			
const USER_QUIET	    = (1<<10);	// Quiet mode upon logon					
const USER_BATCHFLAG    = (1<<11);	// File list allow batch dl flags			
const USER_NETMAIL 	    = (1<<12);	// Forward e-mail to fidonet addr			
const USER_CURSUB	    = (1<<13);	// Remember current sub-board/dir			
const USER_ASK_NSCAN    = (1<<14);	// Ask for newscanning upon logon			
const USER_NO_EXASCII   = (1<<15);	// Don't send extended ASCII				
const USER_ASK_SSCAN    = (1<<16);	// Ask for messages to you at logon			
const USER_AUTOTERM	    = (1<<17);	// Autodetect terminal type 				
const USER_COLDKEYS	    = (1<<18);	// No hot-keys								
const USER_EXTDESC 	    = (1<<19);	// Extended file descriptions				
const USER_AUTOHANG	    = (1<<20);	// Auto-hang-up after transfer				
const USER_WIP 		    = (1<<21);	// Supports WIP terminal emulation			
const USER_AUTOLOGON    = (1<<22);	// AutoLogon via IP							
const USER_HTML		    = (1<<23);	// Using Deuce's HTML terminal (*cough*)	
const USER_NOPAUSESPIN  = (1<<24);	// No spinning cursor at pause prompt		
const USER_PETSCII      = (1<<26);	// Commodore PET (e.g. C64) terminal		
const USER_SWAP_DELETE  = (1<<27);	// Swap the DEL and backspace keys			
const USER_ICE_COLOR    = (1<<28);	// Bright background color support
const USER_UTF8 		= (1<<29);	// UTF-8 terminal
const USER_MOUSE		= (1<<31);	// Terminal supports mouse reporting

//**********************************************************************
// user.qwk_settings 						
//**********************************************************************
const QWK_FILES		= (1<<0);	// Include new files list					
const QWK_EMAIL		= (1<<1);	// Include unread e-mail					
const QWK_ALLMAIL	= (1<<2);	// Include ALL e-mail						
const QWK_DELMAIL	= (1<<3);	// Delete e-mail after download 			
const QWK_BYSELF	= (1<<4);	// Include messages from self				
const QWK_UNUSED	= (1<<5);	// Currently unused 						
const QWK_EXPCTLA	= (1<<6);	// Expand ctrl-a codes to ascii 			
const QWK_RETCTLA	= (1<<7);	// Retain ctrl-a codes						
const QWK_ATTACH	= (1<<8);	// Include file attachments 				
const QWK_NOINDEX	= (1<<9);	// Do not create index files in QWK			
const QWK_TZ		= (1<<10);	// Include @TZ (time zone) in msgs			
const QWK_VIA 		= (1<<11);	// Include @VIA (path) in msgs				
const QWK_NOCTRL	= (1<<12);	// No extraneous control files				
const QWK_EXT		= (1<<13);	// QWK Extended (QWKE) format				
const QWK_MSGID		= (1<<14);	// Include @MSGID and @REPLY in msgs		
const QWK_HEADERS	= (1<<16);	// Include HEADERS.DAT file					
const QWK_VOTING	= (1<<17);	// Include VOTING.DAT
const QWK_UTF8      = (1<<18);	// Include UTF-8 characters

//**********************************************************************
// user.chat_settings						
//**********************************************************************
const CHAT_ECHO		= (1<<0);	// Multinode chat echo						
const CHAT_ACTION	= (1<<1);	// Chat actions 							
const CHAT_NOPAGE	= (1<<2);	// Can't be paged                           
const CHAT_NOACT	= (1<<3);	// No activity alerts						
const CHAT_SPLITP	= (1<<4);	// Split screen private chat				

//**********************************************************************
// Valid flags for user.security.exempt/restrict/flags					
//**********************************************************************
const UFLAG_A		= (1<<0);
const UFLAG_B		= (1<<1);
const UFLAG_C		= (1<<2);
const UFLAG_D		= (1<<3);
const UFLAG_E		= (1<<4);
const UFLAG_F		= (1<<5);
const UFLAG_G		= (1<<6);
const UFLAG_H		= (1<<7);
const UFLAG_I		= (1<<8);
const UFLAG_J		= (1<<9);
const UFLAG_K		= (1<<10);
const UFLAG_L		= (1<<11);
const UFLAG_M		= (1<<12);
const UFLAG_N		= (1<<13);
const UFLAG_O		= (1<<14);
const UFLAG_P		= (1<<15);
const UFLAG_Q		= (1<<16);
const UFLAG_R		= (1<<17);
const UFLAG_S		= (1<<18);
const UFLAG_T		= (1<<19);
const UFLAG_U		= (1<<20);
const UFLAG_V		= (1<<21);
const UFLAG_W		= (1<<22);
const UFLAG_X		= (1<<23);
const UFLAG_Y		= (1<<24);
const UFLAG_Z		= (1<<25);

