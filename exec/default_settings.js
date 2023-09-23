// default user settings
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

for(var i=474;i<495;i++)
	bbs.revert_text(i);

function on_or_off(on)
{
	return bbs.text(on ? On : Off);
}



function parsemenu() {
	const curspin = useron.settings & USER_SPIN ? bbs.text(On) : useron.settings & USER_NOPAUSESPIN ? bbs.text(Off) : "Pause Prompt Only";
	var disp_strings = { spin: curspin };
	for (var i = 0; i < main_cfg.shell.length; i++) {
		if (main_cfg.shell[i].code === useron.command_shell.toUpperCase()) {
			const cmdshell = main_cfg.shell[i].name;
			break;
		}
	}
	for (var i = 0; i < file_cfg.protocol.length; i++) {
		if (String(file_cfg.protocol[i].key) === String(useron.download_protocol)) {
			const protname = file_cfg.protocol[i].name;
			break;
		}
	}
	console.clear();
	console.putmsg(format(bbs.text(UserDefaultsHdr),useron.name,useron.number));
	console.add_hotspot('T');
	console.putmsg(format(bbs.text(UserDefaultsTerminal), termdesc.type(true,useron)));
	console.add_hotspot('L');
	console.putmsg(format(bbs.text(UserDefaultsRows), termdesc.columns(true,useron), termdesc.rows(true,useron)));
	console.add_hotspot('K');
	console.putmsg(format(bbs.text(UserDefaultsCommandSet), cmdshell));
	console.add_hotspot('E');
	console.putmsg(format(bbs.text(UserDefaultsXeditor), (useron.editor ? xtrn_area.editor[useron.editor].name:'None')));
	console.add_hotspot('A');
	console.putmsg(format(bbs.text(UserDefaultsArcType), useron.temp_file_ext));
	console.add_hotspot('X');
	console.putmsg(format(bbs.text(UserDefaultsMenuMode), on_or_off(useron.settings&USER_EXPERT)));
	console.add_hotspot('P');
	console.putmsg(format(bbs.text(UserDefaultsPause), on_or_off(useron.settings&USER_PAUSE)));
	console.add_hotspot('H');
	console.putmsg(format(bbs.text(UserDefaultsHotKey), on_or_off(useron.settings&USER_COLDKEYS)));
	console.add_hotspot('S');
	console.putmsg(format(bbs.text(UserDefaultsCursor), curspin));
	console.add_hotspot('C');
	console.putmsg(format(bbs.text(UserDefaultsCLS), on_or_off(useron.settings&USER_CLRSCRN)));
	console.add_hotspot('N');
	console.putmsg(format(bbs.text(UserDefaultsAskNScan), on_or_off(useron.settings&USER_ASK_NSCAN)));
	console.add_hotspot('Y');
	console.putmsg(format(bbs.text(UserDefaultsAskSScan), on_or_off(useron.settings&USER_ASK_SSCAN)));
	console.add_hotspot('F');
	console.putmsg(format(bbs.text(UserDefaultsANFS), on_or_off(useron.settings&USER_ANFSCAN)));
	console.add_hotspot('R');
	console.putmsg(format(bbs.text(UserDefaultsRemember), on_or_off(useron.settings&USER_CURSUB)));
	console.add_hotspot('B');
	console.putmsg(format(bbs.text(UserDefaultsBatFlag), on_or_off(useron.settings&USER_BATCHFLAG)));
	console.putmsg(format(bbs.text(UserDefaultsNetMail), on_or_off(useron.settings&USER_NETMAIL),useron.netmail));
	console.add_hotspot('M');
	if(bbs.startup_options&BBS_OPT_AUTO_LOGON && useron.security.exemptions&UFLAG_V) {
		console.putmsg(format(bbs.text(UserDefaultsAutoLogon), on_or_off(useron.security.exceptions&UFLAG_V)));
		console.add_hotspot('V');
	}

	if(useron.security.exemptions&UFLAG_Q) {
		console.putmsg(format(bbs.text(UserDefaultsQuiet), on_or_off(useron.settings&USER_QUIET)));
		console.add_hotspot('D');
	}

	console.putmsg(format(bbs.text(UserDefaultsProtocol), protname + ' ',useron.settings&USER_AUTOHANG ? "(Auto-Hangup)":''));
	console.add_hotspot('Z');
	console.putmsg(bbs.text(UserDefaultsPassword));
	console.add_hotspot('W');
	console.putmsg(bbs.text(UserDefaultsWhich));
	console.add_hotspot('Q');
}

var cfglib = load({}, "cfglib.js");
var file_cfg = cfglib.read("file.ini");
var main_cfg = cfglib.read("main.ini");
bbs.node_action = NODE_DFLT;

if (typeof(argv) !== 'undefined' && argv.length>0)
	var useron = new User(argv[0]);
else
	var useron = user;

const userSigFilename = system.data_dir + "user" + format("%04d.sig", useron.number);
const PETSCII_DELETE = '\x14';
const PETSCII_UPPERLOWER = '\x1d';

while(bbs.online && !js.terminated) {
	bbs.nodesync();
	console.aborted = false;
	parsemenu();

	var keys = 'ABCFHKLNPQRSTXYZ?\r';
	if(useron.security.exemptions&UFLAG_Q)
		keys += 'D';
	if(Object.getOwnPropertyNames(xtrn_area.editor).length > 0)
		keys += 'E';
	if(bbs.startup_options&BBS_OPT_AUTO_LOGON && useron.security.exemptions&UFLAG_V)
		keys += 'I';
	if(system.settings&SYS_FWDTONET)
		keys += 'M';
	if(system.settings&SYS_PWEDIT && !(useron.security.restrictions&UFLAG_G))
		keys += 'W';

	switch(console.getkeys(keys, K_UPPER)) {
		case 'A':
			var defaultext = 0;
			var archivetypes = [ "zip", "7z", "tgz" ];
			for (var code in file_cfg.compressor) {
				if(useron.compare_ars(file_cfg.compressor[code].ars) && archivetypes.indexOf(file_cfg.compressor[code].extension) === -1)
					archivetypes.push(file_cfg.compressor[code].extension);
			}
					
			for(var i=0; i<archivetypes.length; i++) {
                                console.uselect(i,bbs.text(ArchiveTypeHeading),archivetypes[i]);
				if(archivetypes[i] === useron.temp_file_ext)
					defaultext = i;
			}
                        if((i=console.uselect(defaultext))>=0)
                                useron.temp_file_ext = archivetypes[i];
			if(console.aborted)
				console.aborted = false;
			break;
		case 'B':
			useron.settings ^= USER_BATCHFLAG;
			break;
		case 'C':
			useron.settings ^= USER_CLRSCRN;
			break;
		case 'D':
			useron.settings ^= USER_QUIET;
			break;
		case 'E':
			if(console.noyes(bbs.text(UseExternalEditorQ))) {
				if(console.aborted)
					break;
				useron.editor = '';
			}
			else {
				var editors=[];
				var defaulteditor=0;
				for(var code in xtrn_area.editor)
					editors.push(code);
				for(var i=0; i<editors.length; i++) {
					console.uselect(i,bbs.text(ExternalEditorHeading),xtrn_area.editor[editors[i]].name,xtrn_area.editor[editors[i]].ars);
					if(editors[i] === useron.editor)
						defaulteditor = i;
				}
				if((i=console.uselect(defaulteditor))>=0)
					useron.editor = editors[i];
			}
			break;
		case 'F':
			useron.settings ^= USER_ANFSCAN;
			break;
		case 'H':
			useron.settings ^= USER_COLDKEYS;
			break;
		case 'I':
			useron.settings ^= USER_AUTOLOGON;
			break;
		case 'K':
			var defaultshell=0;
			for (var i=0; i<main_cfg.shell.length; i++) {
				if(!useron.compare_ars(main_cfg.shell[i].ars))
					continue;
				console.uselect(i,bbs.text(CommandShellHeading),main_cfg.shell[i].name,main_cfg.shell[i].ars);
				if(main_cfg.shell[i].code === user.command_shell.toUpperCase())
					defaultshell=i;
			}
			if((i=console.uselect(defaultshell))>=0) {
				useron.command_shell = main_cfg.shell[i].code;
			}
			break;
		case 'L':
			console.putmsg(bbs.text(HowManyColumns));
			useron.screen_columns = console.getnum(999,0);
			console.putmsg(bbs.text(HowManyRows));
			useron.screen_rows = console.getnum(999,0);
			if (user.number === useron.number)
				console.getdimensions();
			break;
		case 'M':
			console.putmsg(bbs.text(EnterNetMailAddress));
			var email = console.getstr(useron.netmail,LEN_NETMAIL,K_EDIT|K_AUTODEL|K_LINE|K_TRIM)
			if(email === "" || email === null || console.aborted) {
				break;
			}
			useron.netmail = email;
			
			if(useron.netmail.length > 0 && (system.settings & SYS_FWDTONET) && bbs.text(ForwardMailQ).length > 0 && console.yesno(bbs.text(ForwardMailQ)))
				useron.settings |= USER_NETMAIL;
			else
				useron.settings &= ~USER_NETMAIL;
			break;
		case 'N':
			useron.settings ^= USER_ASK_NSCAN;
			break;
		case 'P':
			useron.settings ^= USER_PAUSE;
			break;
		case 'R':
			useron.settings ^= USER_CURSUB;
			break;
		case 'S':
			useron.settings ^= USER_SPIN;
			if(!(useron.settings&USER_SPIN)) {
				if(console.yesno(bbs.text(SpinningCursorOnPauseQ)))
					useron.settings &= ~USER_NOPAUSESPIN;
				else
					useron.settings |= USER_NOPAUSESPIN;
			}
			break;
		case 'T':
			if(console.yesno(bbs.text(AutoTerminalQ))) {
				useron.settings |= USER_AUTOTERM;
				useron.settings &= ~(USER_ANSI|USER_RIP|USER_WIP|USER_HTML|USER_PETSCII|USER_UTF8);
			}
			else
				useron.settings &= ~USER_AUTOTERM;
			if(console.aborted)
				break;
			if(!(useron.settings&USER_AUTOTERM)) {
				if(!console.noyes(bbs.text(Utf8TerminalQ)))
					useron.settings |= USER_UTF8;
				else
					useron.settings &= ~USER_UTF8;
				if(console.yesno(bbs.text(AnsiTerminalQ))) {
					useron.settings |= USER_ANSI;
					useron.settings &= ~USER_PETSCII;
				} else if(!(useron.settings&USER_UTF8)) {
					useron.settings &= ~(USER_ANSI|USER_COLOR|USER_ICE_COLOR);
					if(!console.noyes(bbs.text(PetTerminalQ)))
						useron.settings |= USER_PETSCII|USER_COLOR;
					else
						useron.settings &= ~USER_PETSCII;
				}
			}
			if(console.aborted)
				break;
			var term = (user.number == useron.number) ? console.term_supports() : useron.settings;
			
			if(term&(USER_AUTOTERM|USER_ANSI) && !(term&USER_PETSCII)) {
				useron.settings |= USER_COLOR;
				useron.settings &= ~USER_ICE_COLOR;
				if((useron.settings&USER_AUTOTERM) || console.yesno(bbs.text(ColorTerminalQ))) {
					if(!(console.status&(CON_BLINK_FONT|CON_HBLINK_FONT))
						&& !console.noyes(bbs.text(IceColorTerminalQ)))
						useron.settings |= USER_ICE_COLOR;
				} else
					useron.settings &= ~USER_COLOR;
			}
			if(console.aborted)
				break;
			if(term&USER_ANSI) {
				if(bbs.text(MouseTerminalQ) && console.yesno(bbs.text(MouseTerminalQ)))
					useron.settings |= USER_MOUSE;
				else
					useron.settings &= ~USER_MOUSE;
			}
			if(console.aborted)
				break;
			if(!(term&USER_PETSCII)) {
				if(!(term&USER_UTF8) && !console.yesno(bbs.text(ExAsciiTerminalQ)))
					useron.settings |= USER_NO_EXASCII;
				else
					useron.settings &= ~USER_NO_EXASCII;
				useron.settings &= ~USER_SWAP_DELETE;
				while(bbs.text(HitYourBackspaceKey) && !(useron.settings&(USER_PETSCII|USER_SWAP_DELETE)) && bbs.online) {
					console.putmsg(bbs.text(HitYourBackspaceKey));
					var key = console.getkey(K_CTRLKEYS);
					console.putmsg(format(bbs.text(CharacterReceivedFmt), ascii(key), ascii(key)));
					if(key == '\b')
						break;
					if(key == '\x7f') {
						if(bbs.text(SwapDeleteKeyQ) || console.yesno(bbs.text(SwapDeleteKeyQ)))
							useron.settings |= USER_SWAP_DELETE;
					}
					else if(key == PETSCII_DELETE) {
						console.autoterm |= USER_PETSCII;
						useron.settings |= USER_PETSCII;
						console.putbyte(PETSCII_UPPERLOWER);
						console.putmsg(bbs.text(PetTerminalDetected));
					}
					else
						console.putmsg(format(bbs.text(InvalidBackspaceKeyFmt), ascii(key), ascii(key)));
				}
			}
			if(console.aborted)
				break;
			if(!(useron.settings&USER_AUTOTERM) && (term&(USER_ANSI|USER_NO_EXASCII)) == USER_ANSI) {
				if(!console.noyes(bbs.text(RipTerminalQ)))
					useron.settings |= USER_RIP;
				else
					useron.settings &= ~USER_RIP; 
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
				if(str !== useron.security.password) {
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
				useron.security.password = str;
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
			useron.settings ^= USER_EXPERT;
			break;
		case 'Y':
			useron.settings ^= USER_ASK_SSCAN;
			break;
		case 'Z':
			var c=0;
			var keylist = 'Q';
			for (var code in file_cfg.protocol) {
				if(!useron.compare_ars(file_cfg.protocol[code].ars) || file_cfg.protocol[code].dlcmd.length === 0)
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
			useron.download_protocol = kp;
			if(console.yesno(bbs.text(HangUpAfterXferQ)))
				useron.settings |=USER_AUTOHANG;
			else
				useron.settings &=~USER_AUTOHANG;
			break;
		case 'Q':
		case '\r':
			console.clear_hotspots();
			exit();
	}
}

