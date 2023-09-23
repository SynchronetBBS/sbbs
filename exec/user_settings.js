// user settings
// A javascript replacement for the built in settings which
// will allow a sysop to do a bit more configuration and
// add or remove options as they wish.

// *** NOTE - This will only work on Synchronet v3.20 or above!

// This is another example of Nigel's inability to produce
// good code and should certainly not be used as a template
// for your own project. The only guarantee I give is that it
// works, mostly, on my BBS while testing.

// If you want to customize the layout/display, you can use
// replace_text(nnn,"your text"); for any of the lines. You
// can find the number to use for nnn in the ctrl/text.dat
// file. If there's enough interest, I'll look into a template
// system, whereby you can design your page as a .asc file and
// then display it.

// Updates and suggestions to:  sysop@endofthelinebbs.com (Email)
//				sysop@1:124/5016	  (Netmail)
//				sysop@EOTLBBS		  (QWK)
//				nelgin on #synchronet	  (irc)

"use strict";

require("sbbsdefs.js", 'USER_EXPERT');
require("userdefs.js", 'USER_SPIN');
require("text.js", 'UserDefaultsTerminal');
require("nodedefs.js", 'NODE_CHAT');
var termdesc = load("termdesc.js");

function on_or_off(on)
{
	return bbs.text(on ? On : Off);
}

function display_menu(thisuser) {
	const curspin = thisuser.settings & USER_SPIN ? bbs.text(On) : thisuser.settings & USER_NOPAUSESPIN ? bbs.text(Off) : "Pause Prompt Only";
	var disp_strings = { spin: curspin };
	for (var i = 0; i < main_cfg.shell.length; i++) {
		if (main_cfg.shell[i].code === thisuser.command_shell.toUpperCase()) {
			const cmdshell = main_cfg.shell[i].name;
			break;
		}
	}
	var protname = bbs.text(None);
	for (var i = 0; i < file_cfg.protocol.length; i++) {
		if (String(file_cfg.protocol[i].key) === String(thisuser.download_protocol)) {
			protname = file_cfg.protocol[i].name;
			break;
		}
	}
	console.clear();
	console.putmsg(format(bbs.text(UserDefaultsHdr),thisuser.alias,thisuser.number));
	console.add_hotspot('T');
	console.putmsg(format(bbs.text(UserDefaultsTerminal)
		,termdesc.type(true, thisuser.number == user.number ? undefined : thisuser)));
	console.add_hotspot('L');
	console.putmsg(format(bbs.text(UserDefaultsRows), termdesc.columns(true,user), termdesc.rows(true,user)));
	if(main_cfg.shell.length > 1) {
		console.add_hotspot('K');
		console.putmsg(format(bbs.text(UserDefaultsCommandSet), cmdshell));
	}
	console.add_hotspot('E');
	console.putmsg(format(bbs.text(UserDefaultsXeditor), (thisuser.editor ? xtrn_area.editor[thisuser.editor].name:'None')));
	console.add_hotspot('A');
	console.putmsg(format(bbs.text(UserDefaultsArcType), thisuser.temp_file_ext));
	console.add_hotspot('X');
	console.putmsg(format(bbs.text(UserDefaultsMenuMode), on_or_off(thisuser.settings&USER_EXPERT)));
	console.add_hotspot('P');
	console.putmsg(format(bbs.text(UserDefaultsPause), on_or_off(thisuser.settings&USER_PAUSE)));
	console.add_hotspot('H');
	console.putmsg(format(bbs.text(UserDefaultsHotKey), on_or_off(!(thisuser.settings&USER_COLDKEYS))));
	console.add_hotspot('S');
	console.putmsg(format(bbs.text(UserDefaultsCursor), curspin));
	console.add_hotspot('C');
	console.putmsg(format(bbs.text(UserDefaultsCLS), on_or_off(thisuser.settings&USER_CLRSCRN)));
	console.add_hotspot('N');
	console.putmsg(format(bbs.text(UserDefaultsAskNScan), on_or_off(thisuser.settings&USER_ASK_NSCAN)));
	console.add_hotspot('Y');
	console.putmsg(format(bbs.text(UserDefaultsAskSScan), on_or_off(thisuser.settings&USER_ASK_SSCAN)));
	console.add_hotspot('F');
	console.putmsg(format(bbs.text(UserDefaultsANFS), on_or_off(thisuser.settings&USER_ANFSCAN)));
	console.add_hotspot('R');
	console.putmsg(format(bbs.text(UserDefaultsRemember), on_or_off(thisuser.settings&USER_CURSUB)));
	console.add_hotspot('B');
	console.putmsg(format(bbs.text(UserDefaultsBatFlag), on_or_off(thisuser.settings&USER_BATCHFLAG)));
	console.putmsg(format(bbs.text(UserDefaultsNetMail), on_or_off(thisuser.settings&USER_NETMAIL),thisuser.netmail));
	console.add_hotspot('M');
	if(bbs.startup_options&BBS_OPT_AUTO_LOGON && thisuser.security.exemptions&UFLAG_V) {
		console.add_hotspot('I');
		console.putmsg(format(bbs.text(UserDefaultsAutoLogon), on_or_off(thisuser.security.exceptions&UFLAG_V)));
	}

	if(thisuser.security.exemptions&UFLAG_Q) {
		console.add_hotspot('D');
		console.putmsg(format(bbs.text(UserDefaultsQuiet), on_or_off(thisuser.settings&USER_QUIET)));
	}

	console.add_hotspot('Z');
	console.putmsg(format(bbs.text(UserDefaultsProtocol), protname + ' ',thisuser.settings&USER_AUTOHANG ? "(Auto-Hangup)":''));
	console.add_hotspot('W');
	console.putmsg(bbs.text(UserDefaultsPassword));
	console.add_hotspot('Q');
	console.putmsg(bbs.text(UserDefaultsWhich));
}

var cfglib = load({}, "cfglib.js");
var file_cfg = cfglib.read("file.ini");
var main_cfg = cfglib.read("main.ini");

var thisuser = new User(argv[0] || user.number);

const userSigFilename = system.data_dir + "user" + format("%04d.sig", thisuser.number);
const PETSCII_DELETE = '\x14';
const PETSCII_UPPERLOWER = '\x1d';

while(bbs.online && !js.terminated) {
	bbs.node_action = NODE_DFLT;
	bbs.nodesync();
	console.aborted = false;
	display_menu(thisuser);

	var keys = 'ABCFHKLNPQRSTXYZ?\r';
	if(thisuser.security.exemptions&UFLAG_Q)
		keys += 'D';
	if(Object.getOwnPropertyNames(xtrn_area.editor).length > 0)
		keys += 'E';
	if(bbs.startup_options&BBS_OPT_AUTO_LOGON && thisuser.security.exemptions&UFLAG_V)
		keys += 'I';
	if(system.settings&SYS_FWDTONET)
		keys += 'M';
	if(system.settings&SYS_PWEDIT && !(thisuser.security.restrictions&UFLAG_G))
		keys += 'W';

	switch(console.getkeys(keys, K_UPPER)) {
		case 'A':
			var defaultext = 0;
			var archivetypes = [ "zip", "7z", "tgz" ];
			for (var code in file_cfg.compressor) {
				if(thisuser.compare_ars(file_cfg.compressor[code].ars) && archivetypes.indexOf(file_cfg.compressor[code].extension) === -1)
					archivetypes.push(file_cfg.compressor[code].extension);
			}
					
			for(var i=0; i<archivetypes.length; i++) {
                                console.uselect(i,bbs.text(ArchiveTypeHeading),archivetypes[i]);
				if(archivetypes[i] === thisuser.temp_file_ext)
					defaultext = i;
			}
                        if((i=console.uselect(defaultext))>=0)
                                thisuser.temp_file_ext = archivetypes[i];
			if(console.aborted)
				console.aborted = false;
			break;
		case 'B':
			thisuser.settings ^= USER_BATCHFLAG;
			break;
		case 'C':
			thisuser.settings ^= USER_CLRSCRN;
			break;
		case 'D':
			thisuser.settings ^= USER_QUIET;
			break;
		case 'E':
			if(console.noyes(bbs.text(UseExternalEditorQ))) {
				if(console.aborted)
					break;
				thisuser.editor = '';
			}
			else {
				var editors=[];
				var defaulteditor=0;
				for(var code in xtrn_area.editor)
					editors.push(code);
				for(var i=0; i<editors.length; i++) {
					console.uselect(i,bbs.text(ExternalEditorHeading),xtrn_area.editor[editors[i]].name,xtrn_area.editor[editors[i]].ars);
					if(editors[i] === thisuser.editor)
						defaulteditor = i;
				}
				if((i=console.uselect(defaulteditor))>=0)
					thisuser.editor = editors[i];
			}
			break;
		case 'F':
			thisuser.settings ^= USER_ANFSCAN;
			break;
		case 'H':
			thisuser.settings ^= USER_COLDKEYS;
			break;
		case 'I':
			thisuser.settings ^= USER_AUTOLOGON;
			break;
		case 'K':
			var defaultshell=0;
			for (var i=0; i<main_cfg.shell.length; i++) {
				if(!thisuser.compare_ars(main_cfg.shell[i].ars))
					continue;
				console.uselect(i,bbs.text(CommandShellHeading),main_cfg.shell[i].name,main_cfg.shell[i].ars);
				if(main_cfg.shell[i].code === user.command_shell.toUpperCase())
					defaultshell=i;
			}
			if((i=console.uselect(defaultshell))>=0) {
				thisuser.command_shell = main_cfg.shell[i].code;
			}
			break;
		case 'L':
			console.putmsg(bbs.text(HowManyColumns));
			thisuser.screen_columns = console.getnum(999,0);
			console.putmsg(bbs.text(HowManyRows));
			thisuser.screen_rows = console.getnum(999,0);
			if (user.number === thisuser.number)
				console.getdimensions();
			break;
		case 'M':
			console.putmsg(bbs.text(EnterNetMailAddress));
			var email = console.getstr(thisuser.netmail,LEN_NETMAIL,K_EDIT|K_AUTODEL|K_LINE|K_TRIM)
			if(email === "" || email === null || console.aborted) {
				break;
			}
			thisuser.netmail = email;
			
			if(thisuser.netmail.length > 0 && (system.settings & SYS_FWDTONET) && bbs.text(ForwardMailQ).length > 0 && console.yesno(bbs.text(ForwardMailQ)))
				thisuser.settings |= USER_NETMAIL;
			else
				thisuser.settings &= ~USER_NETMAIL;
			break;
		case 'N':
			thisuser.settings ^= USER_ASK_NSCAN;
			break;
		case 'P':
			thisuser.settings ^= USER_PAUSE;
			break;
		case 'R':
			thisuser.settings ^= USER_CURSUB;
			break;
		case 'S':
			thisuser.settings ^= USER_SPIN;
			if(!(thisuser.settings&USER_SPIN)) {
				if(console.yesno(bbs.text(SpinningCursorOnPauseQ)))
					thisuser.settings &= ~USER_NOPAUSESPIN;
				else
					thisuser.settings |= USER_NOPAUSESPIN;
			}
			break;
		case 'T':
			if(console.yesno(bbs.text(AutoTerminalQ))) {
				thisuser.settings |= USER_AUTOTERM;
				thisuser.settings &= ~(USER_ANSI|USER_RIP|USER_WIP|USER_HTML|USER_PETSCII|USER_UTF8);
			}
			else
				thisuser.settings &= ~USER_AUTOTERM;
			if(console.aborted)
				break;
			if(!(thisuser.settings&USER_AUTOTERM)) {
				if(!console.noyes(bbs.text(Utf8TerminalQ)))
					thisuser.settings |= USER_UTF8;
				else
					thisuser.settings &= ~USER_UTF8;
				if(console.yesno(bbs.text(AnsiTerminalQ))) {
					thisuser.settings |= USER_ANSI;
					thisuser.settings &= ~USER_PETSCII;
				} else if(!(thisuser.settings&USER_UTF8)) {
					thisuser.settings &= ~(USER_ANSI|USER_COLOR|USER_ICE_COLOR);
					if(!console.noyes(bbs.text(PetTerminalQ)))
						thisuser.settings |= USER_PETSCII|USER_COLOR;
					else
						thisuser.settings &= ~USER_PETSCII;
				}
			}
			if(console.aborted)
				break;
			var term = (user.number == thisuser.number) ? console.term_supports() : thisuser.settings;
			
			if(term&(USER_AUTOTERM|USER_ANSI) && !(term&USER_PETSCII)) {
				thisuser.settings |= USER_COLOR;
				thisuser.settings &= ~USER_ICE_COLOR;
				if((thisuser.settings&USER_AUTOTERM) || console.yesno(bbs.text(ColorTerminalQ))) {
					if(!(console.status&(CON_BLINK_FONT|CON_HBLINK_FONT))
						&& !console.noyes(bbs.text(IceColorTerminalQ)))
						thisuser.settings |= USER_ICE_COLOR;
				} else
					thisuser.settings &= ~USER_COLOR;
			}
			if(console.aborted)
				break;
			if(term&USER_ANSI) {
				if(bbs.text(MouseTerminalQ) && console.yesno(bbs.text(MouseTerminalQ)))
					thisuser.settings |= USER_MOUSE;
				else
					thisuser.settings &= ~USER_MOUSE;
			}
			if(console.aborted)
				break;
			if(!(term&USER_PETSCII)) {
				if(!(term&USER_UTF8) && !console.yesno(bbs.text(ExAsciiTerminalQ)))
					thisuser.settings |= USER_NO_EXASCII;
				else
					thisuser.settings &= ~USER_NO_EXASCII;
				thisuser.settings &= ~USER_SWAP_DELETE;
				while(bbs.text(HitYourBackspaceKey) && !(thisuser.settings&(USER_PETSCII|USER_SWAP_DELETE)) && bbs.online) {
					console.putmsg(bbs.text(HitYourBackspaceKey));
					var key = console.getkey(K_CTRLKEYS);
					console.putmsg(format(bbs.text(CharacterReceivedFmt), ascii(key), ascii(key)));
					if(key == '\b')
						break;
					if(key == '\x7f') {
						if(bbs.text(SwapDeleteKeyQ) || console.yesno(bbs.text(SwapDeleteKeyQ)))
							thisuser.settings |= USER_SWAP_DELETE;
					}
					else if(key == PETSCII_DELETE) {
						console.autoterm |= USER_PETSCII;
						thisuser.settings |= USER_PETSCII;
						console.putbyte(PETSCII_UPPERLOWER);
						console.putmsg(bbs.text(PetTerminalDetected));
					}
					else
						console.putmsg(format(bbs.text(InvalidBackspaceKeyFmt), ascii(key), ascii(key)));
				}
			}
			if(console.aborted)
				break;
			if(!(thisuser.settings&USER_AUTOTERM) && (term&(USER_ANSI|USER_NO_EXASCII)) == USER_ANSI) {
				if(!console.noyes(bbs.text(RipTerminalQ)))
					thisuser.settings |= USER_RIP;
				else
					thisuser.settings &= ~USER_RIP;
			}
			if(console.aborted)
				break;
			break;
		case 'W':
			if(console.yesno(bbs.text(NewPasswordQ))){
				console.putmsg(bbs.text(CurrentPassword));
				console.status |= CON_R_ECHOX;
				var str = console.getstr(LEN_PASS*2,K_UPPER);
				console.status &= ~(CON_R_ECHOX|CON_L_ECHOX);
				bbs.user_sync();
				if(str !== thisuser.security.password) {
					console.putmsg(bbs.text(WrongPassword));
					break;
				}
				console.putmsg(format(bbs.text(NewPasswordPromptFmt),system.min_password_length,system.max_password_length));
				str = console.getstr('',LEN_PASS,K_UPPER|K_LINE|K_TRIM);
				if(!bbs.good_password(str)) {
					console.crlf();
					console.pause();
					break;
				}
				console.putmsg(bbs.text(VerifyPassword));
				console.status |= CON_R_ECHOX;
				var pw = console.getstr(LEN_PASS,K_UPPER|K_LINE|K_TRIM);
				console.status &= ~(CON_R_ECHOX|CON_L_ECHOX);
				if(str !== pw) {
					console.putmsg(bbs.text(WrongPassword));
					break;
				}
				thisuser.security.password = str;
				console.putmsg(bbs.text(PasswordChanged));
				log(LOG_NOTICE,'changed password');
			}
			if(file_exists(userSigFilename)) {
				if(console.yesno(bbs.text(ViewSignatureQ)))
					console.printfile(userSigFilename);
			}
			if(console.yesno(bbs.text(CreateEditSignatureQ)))
				console.editfile(userSigFilename);
			else {
				if(file_exists(userSigFilename)) {
					if(console.yesno(bbs.text(DeleteSignatureQ)))
						file_remove(userSigFilename);
				}
			}
			break;
		case 'X':
			thisuser.settings ^= USER_EXPERT;
			break;
		case 'Y':
			thisuser.settings ^= USER_ASK_SSCAN;
			break;
		case 'Z':
			var c=0;
			var keylist = 'Q';
			for (var code in file_cfg.protocol) {
				if(!thisuser.compare_ars(file_cfg.protocol[code].ars) || file_cfg.protocol[code].dlcmd.length === 0)
					continue;
				console.putmsg(format(bbs.text(TransferProtLstFmt),String(file_cfg.protocol[code].key),file_cfg.protocol[code].name));

				keylist += String(file_cfg.protocol[code].key);
				if(c%2===1)
					console.crlf();
				c++;
			}
			console.mnemonics(bbs.text(ProtocolOrQuit));
			var kp = console.getkeys(keylist);
			if(kp==='Q' || console.aborted)
				break;
			thisuser.download_protocol = kp;
			if(console.yesno(bbs.text(HangUpAfterXferQ)))
				thisuser.settings |=USER_AUTOHANG;
			else
				thisuser.settings &=~USER_AUTOHANG;
			break;
		case 'Q':
		case '\r':
			console.clear_hotspots();
			exit();
	}
}

