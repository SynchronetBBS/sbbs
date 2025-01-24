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

console.clear();
require("sbbsdefs.js", 'BBS_OPT_AUTO_LOGON');
require("userdefs.js", 'USER_SPIN');
require("nodedefs.js", 'NODE_DFLT');
require("gettext.js", 'gettext');
var termdesc = load("termdesc.js");

function get_lang_count()
{
	return directory(system.ctrl_dir + "text.*.ini").length + 1;
}

function get_lang_list()
{
	var result = [""];
	var list = directory(system.ctrl_dir + "text.*.ini");
	const prefix_len = (system.ctrl_dir + "text.").length;
	for (var i in list) {
		result.push(list[i].slice(prefix_len, -4));
	}
	return result;
}

function get_lang_desc_list()
{
	var result = [bbs.text(bbs.text.LANG, /* default: */true)];
	var list = directory(system.ctrl_dir + "text.*.ini");
	for (var i in list) {
		var f = new File(list[i]);
		if (!f.open("r"))
			continue;
		result.push(f.iniGetValue(null, "LANG", ""));
		f.close();
	}
	return result;
}

function on_or_off(on)
{
	return bbs.text(on ? bbs.text.On : bbs.text.Off);
}

function display_menu(thisuser)
{
	var keys = 'Q\r';

	const curspin = (thisuser.settings & USER_SPIN) ? bbs.text(On)
		: (thisuser.settings & USER_NOPAUSESPIN) ? bbs.text(Off) : gettext("Pause Prompt") + " " + bbs.text(Only);
	for (var i = 0; i < main_cfg.shell.length; i++) {
		if (main_cfg.shell[i].code === thisuser.command_shell.toUpperCase()) {
			const cmdshell = main_cfg.shell[i].name;
			break;
		}
	}
	var protname = bbs.text(bbs.text.None);
	var autohang = '';
	for (var i = 0; i < file_cfg.protocol.length; i++) {
		if (String(file_cfg.protocol[i].key) === String(thisuser.download_protocol)) {
			protname = file_cfg.protocol[i].name;
			if(thisuser.settings & USER_AUTOHANG)
				autohang = gettext("(Auto-Hangup)");
			break;
		}
	}
	console.clear();
	console.putmsg(format(bbs.text(UserDefaultsHdr),thisuser.alias,thisuser.number));
	if (bbs.text(bbs.text.UserDefaultsTerminal).length) {
		keys += 'T';
		console.add_hotspot('T');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsTerminal)
			,termdesc.type(true, thisuser.number == user.number ? undefined : thisuser)));
	}
	if (bbs.text(bbs.text.UserDefaultsRows).length) {
		keys += 'L';
		console.add_hotspot('L');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsRows)
			,termdesc.columns(true,user), termdesc.rows(true,user)));
	}
	if (bbs.text(bbs.text.UserDefaultsCommandSet).length
		&& main_cfg.shell.length > 1) {
		keys += 'K';
		console.add_hotspot('K');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsCommandSet), cmdshell));
	}
	if (bbs.text(bbs.text.UserDefaultsLanguage).length && get_lang_count() > 1) {
		keys += 'I';
		console.add_hotspot('I');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsLanguage)
			,bbs.text(bbs.text.Language), bbs.text(bbs.text.LANG)));
	}
	if (bbs.text(bbs.text.UserDefaultsXeditor).length
		&& Object.getOwnPropertyNames(xtrn_area.editor).length > 0) {
		keys += 'E';
		console.add_hotspot('E');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsXeditor)
			,(thisuser.editor && xtrn_area.editor[thisuser.editor])
				? xtrn_area.editor[thisuser.editor].name : bbs.text(bbs.text.None)));
	}
	if (bbs.text(bbs.text.UserDefaultsArcType).length) {
		keys += 'A';
		console.add_hotspot('A');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsArcType)
			,thisuser.temp_file_ext));
	}
	if (bbs.text(bbs.text.UserDefaultsMenuMode).length) {
		keys += 'X';
		console.add_hotspot('X');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsMenuMode)
			,on_or_off(thisuser.settings & USER_EXPERT)));
	}
	if (bbs.text(bbs.text.UserDefaultsPause).length) {
		keys += 'P';
		console.add_hotspot('P');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsPause)
			,on_or_off(thisuser.settings & USER_PAUSE)));
	}
	if (bbs.text(bbs.text.UserDefaultsHotKey).length) {
		keys += 'H';
		console.add_hotspot('H');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsHotKey)
			,on_or_off(!(thisuser.settings & USER_COLDKEYS))));
	}
	if (bbs.text(bbs.text.UserDefaultsCursor).length) {
		keys += 'S';
		console.add_hotspot('S');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsCursor)
			,curspin));
	}
	if (bbs.text(bbs.text.UserDefaultsCLS).length) {
		keys += 'C';
		console.add_hotspot('C');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsCLS)
			,on_or_off(thisuser.settings & USER_CLRSCRN)));
	}
	if (bbs.text(bbs.text.UserDefaultsAskNScan).length) {
		keys += 'N';
		console.add_hotspot('N');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsAskNScan)
			,on_or_off(thisuser.settings & USER_ASK_NSCAN)));
	}
	if (bbs.text(bbs.text.UserDefaultsAskSScan).length) {
		keys += 'Y';
		console.add_hotspot('Y');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsAskSScan)
			,on_or_off(thisuser.settings & USER_ASK_SSCAN)));
	}
	if (bbs.text(bbs.text.UserDefaultsANFS).length) {
		keys += 'F';
		console.add_hotspot('F');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsANFS)
			,on_or_off(thisuser.settings & USER_ANFSCAN)));
	}
	if (bbs.text(bbs.text.UserDefaultsRemember).length) {
		keys += 'R';
		console.add_hotspot('R');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsRemember)
			,on_or_off(thisuser.settings & USER_CURSUB)));
	}
	if (bbs.text(bbs.text.UserDefaultsBatFlag).length) {
		keys += 'B';
		console.add_hotspot('B');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsBatFlag)
			,on_or_off(thisuser.settings & USER_BATCHFLAG)));
	}
	if (bbs.text(bbs.text.UserDefaultsNetMail).length
		&& (system.settings & SYS_FWDTONET)) {
		keys += 'M';
		console.add_hotspot('M');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsNetMail)
			,on_or_off(thisuser.settings & USER_NETMAIL), thisuser.netmail));
	}
	if (bbs.text(bbs.text.UserDefaultsQuiet).length
		&& (thisuser.security.exemptions & UFLAG_Q)) {
		keys += 'D';
		console.add_hotspot('D');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsQuiet)
			,on_or_off(thisuser.settings & USER_QUIET)));
	}
	if (bbs.text(bbs.text.UserDefaultsProtocol).length) {
		keys += 'Z';
		console.add_hotspot('Z');
		console.putmsg(format(bbs.text(bbs.text.UserDefaultsProtocol)
			,protname + ' '
			,autohang));
	}
	if (bbs.text(bbs.text.UserDefaultsPassword).length
		&& (system.settings & SYS_PWEDIT)
		&& !(thisuser.security.restrictions & UFLAG_G)) {
		keys += 'W';
		console.add_hotspot('W');
		console.putmsg(bbs.text(bbs.text.UserDefaultsPassword));
	}
	console.putmsg(bbs.text(bbs.text.UserDefaultsWhich));
	console.add_hotspot('Q');

	return keys;
}

var cfglib = load({}, "cfglib.js");
var file_cfg = cfglib.read("file.ini");
var main_cfg = cfglib.read("main.ini");

var thisuser = new User(argv[0] || user.number);

const userSigFilename = system.data_dir + "user/" + format("%04d.sig", thisuser.number);
const PETSCII_DELETE = '\x14';
const PETSCII_UPPERLOWER = 0x1d;

while(bbs.online && !js.terminated) {
	bbs.node_action = NODE_DFLT;
	bbs.nodesync();
	console.aborted = false;
	if (user.number === thisuser.number) {
		bbs.load_user_text();
		console.term_updated();
	}
	var keys = display_menu(thisuser);

	switch(console.getkeys(keys, K_UPPER)) {
		case 'A':
			var defaultext = 0;
			var archivetypes = Archive.supported_formats || [ "zip", "7z", "tgz" ];
			for (var code in file_cfg.compressor) {
				if (thisuser.compare_ars(file_cfg.compressor[code].ars)
					&& archivetypes.indexOf(file_cfg.compressor[code].extension) === -1)
					archivetypes.push(file_cfg.compressor[code].extension);
			}

			for (var i = 0; i < archivetypes.length; i++) {
		                console.uselect(i
					,bbs.text(bbs.text.ArchiveTypeHeading)
					,archivetypes[i]);
				if (archivetypes[i] === thisuser.temp_file_ext)
					defaultext = i;
			}
			if ((i=console.uselect(defaultext))>=0)
				thisuser.temp_file_ext = archivetypes[i];
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
			if ((!thisuser.editor && console.noyes(bbs.text(bbs.text.UseExternalEditorQ)))
				|| (thisuser.editor && !console.yesno(bbs.text(bbs.text.UseExternalEditorQ)))) {
				if (console.aborted)
					break;
				thisuser.editor = '';
			}
			else {
				var editors=[];
				var defaulteditor = 0;
				for (var code in xtrn_area.editor)
					editors.push(code);
				for (var i = 0; i < editors.length; i++) {
					console.uselect(i, bbs.text(bbs.text.ExternalEditorHeading)
						,xtrn_area.editor[editors[i]].name
						,xtrn_area.editor[editors[i]].ars);
					if (editors[i] === thisuser.editor)
						defaulteditor = i;
				}
				if ((i=console.uselect(defaulteditor))>=0)
					thisuser.editor = editors[i];
			}
			break;
		case 'F':
			thisuser.settings ^= USER_ANFSCAN;
			break;
		case 'H':
			thisuser.settings ^= USER_COLDKEYS;
			break;
		case 'K':
			var defaultshell = 0;
			for (var i = 0; i < main_cfg.shell.length; i++) {
				if (!thisuser.compare_ars(main_cfg.shell[i].ars))
					continue;
				console.uselect(i,bbs.text(bbs.text.CommandShellHeading),main_cfg.shell[i].name,main_cfg.shell[i].ars);
				if (main_cfg.shell[i].code === thisuser.command_shell.toUpperCase())
					defaultshell=i;
			}
			if ((i=console.uselect(defaultshell))>=0) {
				thisuser.command_shell = main_cfg.shell[i].code;
			}
			break;
		case 'I': /* Language */
		{
			var lang = get_lang_list();
			var i;
			for (i = 0; i < lang.length; ++i) {
				if (thisuser.lang == lang[i])
					break;
			}
			if (i >= lang.length)
				i = 0;
			var desc = get_lang_desc_list();
			for (var j = 0; j < desc.length; ++j) {
				console.uselect(j, bbs.text(bbs.text.Language), desc[j]);
			}
			if ((j = console.uselect(i)) >= 0)
				thisuser.lang = lang[j];
			break;
		}
		case 'L':
		{
			console.putmsg(bbs.text(bbs.text.HowManyColumns));
			var val = console.getnum(999,0);
			if (val < 0)
				break;
			thisuser.screen_columns = val;
			if (user.number === thisuser.number) {
				user.screen_columns = thisuser.screen_columns;
				console.getdimensions();
			}
			console.putmsg(bbs.text(bbs.text.HowManyRows));
			val = console.getnum(999,0);
			if (val < 0)
				break;
			thisuser.screen_rows = val;
			if (user.number === thisuser.number) {
				user.screen_rows = thisuser.screen_rows;
				console.getdimensions();
			}
			break;
		}
		case 'M':
			console.putmsg(bbs.text(bbs.text.EnterNetMailAddress));
			var email = console.getstr(thisuser.netmail, LEN_NETMAIL
				,K_EDIT | K_AUTODEL | K_LINE | K_TRIM)
			if (email === "" || email === null || console.aborted) {
				break;
			}
			thisuser.netmail = email;

			if (thisuser.netmail.length > 0
				&& (system.settings & SYS_FWDTONET)
				&& bbs.text(bbs.text.ForwardMailQ).length > 0
				&& console.yesno(bbs.text(bbs.text.ForwardMailQ)))
				thisuser.settings |= USER_NETMAIL;
			else if (!console.aborted)
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
			if (!(thisuser.settings & USER_SPIN)) {
				if (console.yesno(bbs.text(bbs.text.SpinningCursorOnPauseQ)))
					thisuser.settings &= ~USER_NOPAUSESPIN;
				else if (!console.aborted)
					thisuser.settings |= USER_NOPAUSESPIN;
			}
			break;
		case 'T':
			if (console.yesno(bbs.text(bbs.text.AutoTerminalQ))) {
				thisuser.settings |= USER_AUTOTERM;
				thisuser.settings &=
					~(USER_ANSI | USER_RIP | USER_WIP | USER_HTML | USER_PETSCII | USER_UTF8);
				if (user.number === thisuser.number)
					thisuser.settings |= console.autoterm;
			}
			else if (!console.aborted)
				thisuser.settings &= ~USER_AUTOTERM;
			if (console.aborted)
				break;
			if (!(thisuser.settings & USER_AUTOTERM)) {
				if (!console.noyes(bbs.text(bbs.text.Utf8TerminalQ)))
					thisuser.settings |= USER_UTF8;
				else if (!console.aborted)
					thisuser.settings &= ~USER_UTF8;
				if (console.yesno(bbs.text(bbs.text.AnsiTerminalQ))) {
					thisuser.settings |= USER_ANSI;
					thisuser.settings &= ~USER_PETSCII;
				} else if (!console.aborted && !(thisuser.settings & USER_UTF8)) {
					thisuser.settings &= ~(USER_ANSI | USER_COLOR | USER_ICE_COLOR);
					if (!console.noyes(bbs.text(bbs.text.PetTerminalQ)))
						thisuser.settings |= USER_PETSCII|USER_COLOR;
					else if (!console.aborted)
						thisuser.settings &= ~USER_PETSCII;
				}
			}
			if (console.aborted)
				break;
			var term = (user.number == thisuser.number) ?
				console.term_supports() : thisuser.settings;

			if (term&(USER_AUTOTERM|USER_ANSI) && !(term & USER_PETSCII)) {
				thisuser.settings |= USER_COLOR;
				thisuser.settings &= ~USER_ICE_COLOR;
				if ((thisuser.settings & USER_AUTOTERM)
					|| console.yesno(bbs.text(bbs.text.ColorTerminalQ))) {
					if (!(console.status & (CON_BLINK_FONT|CON_HBLINK_FONT))
						&& !console.noyes(bbs.text(bbs.text.IceColorTerminalQ)))
						thisuser.settings |= USER_ICE_COLOR;
				} else if (!console.aborted)
					thisuser.settings &= ~USER_COLOR;
			}
			if (console.aborted)
				break;
			if (term & USER_ANSI) {
				if (bbs.text(bbs.text.MouseTerminalQ).length) {
					if(term & USER_MOUSE) {
						if (!console.yesno(bbs.text(bbs.text.MouseTerminalQ)) && !console.aborted)
							thisuser.settings &= ~USER_MOUSE;
					} else {
						if (!console.noyes(bbs.text(bbs.text.MouseTerminalQ)) && !console.aborted)
							thisuser.settings |= USER_MOUSE;
					}
				} else if (!console.aborted)
					thisuser.settings &= ~USER_MOUSE;
			}
			if (console.aborted)
				break;
			if (!(term & USER_PETSCII)) {
				if (!(term & USER_UTF8) && !console.yesno(bbs.text(bbs.text.ExAsciiTerminalQ)))
					thisuser.settings |= USER_NO_EXASCII;
				else if (!console.aborted)
					thisuser.settings &= ~USER_NO_EXASCII;
				if (console.aborted)
					break;
				thisuser.settings &= ~USER_SWAP_DELETE;
				while(bbs.text(bbs.text.HitYourBackspaceKey).length
					&& !(thisuser.settings & (USER_PETSCII | USER_SWAP_DELETE))
					&& bbs.online) {
					console.putmsg(bbs.text(bbs.text.HitYourBackspaceKey));
					console.status |= CON_RAW_IN;
					var key = console.getkey(K_CTRLKEYS);
					console.status &= ~CON_RAW_IN;
					console.putmsg(format(bbs.text(bbs.text.CharacterReceivedFmt), ascii(key), ascii(key)));
					if (key == '\b')
						break;
					if (key == '\x7f') {
						if (bbs.text(bbs.text.SwapDeleteKeyQ).length || console.yesno(bbs.text(bbs.text.SwapDeleteKeyQ)))
							thisuser.settings |= USER_SWAP_DELETE;
					}
					else if (key == PETSCII_DELETE) {
						console.autoterm |= USER_PETSCII;
						thisuser.settings |= USER_PETSCII;
						console.putbyte(PETSCII_UPPERLOWER);
						console.putmsg(bbs.text(bbs.text.PetTerminalDetected));
					}
					else
						console.putmsg(format(bbs.text(bbs.text.InvalidBackspaceKeyFmt)
							,ascii(key), ascii(key)));
				}
			}
			if (console.aborted)
				break;
			if (!(thisuser.settings & USER_AUTOTERM)
				&& (term&(USER_ANSI|USER_NO_EXASCII)) == USER_ANSI) {
				if (!console.noyes(bbs.text(bbs.text.RipTerminalQ)))
					thisuser.settings |= USER_RIP;
				else
					thisuser.settings &= ~USER_RIP;
			}
			if (console.aborted)
				break;
			break;
		case 'W':
			if (console.yesno(bbs.text(bbs.text.NewPasswordQ))){
				console.putmsg(bbs.text(bbs.text.CurrentPassword));
				console.status |= CON_R_ECHOX;
				var str = console.getstr(LEN_PASS * 2, K_UPPER);
				console.status &= ~(CON_R_ECHOX|CON_L_ECHOX);
				bbs.user_sync();
				if (str !== thisuser.security.password) {
					console.putmsg(bbs.text(bbs.text.WrongPassword));
					break;
				}
				console.putmsg(format(bbs.text(bbs.text.NewPasswordPromptFmt)
					,system.min_password_length, system.max_password_length));
				str = console.getstr(LEN_PASS, K_UPPER | K_LINE | K_TRIM);
				if (!bbs.good_password(str)) {
					console.newline();
					console.pause();
					break;
				}
				console.putmsg(bbs.text(bbs.text.VerifyPassword));
				console.status |= CON_R_ECHOX;
				var pw = console.getstr(LEN_PASS, K_UPPER | K_LINE | K_TRIM);
				console.status &= ~(CON_R_ECHOX|CON_L_ECHOX);
				if (str !== pw) {
					console.putmsg(bbs.text(bbs.text.WrongPassword));
					break;
				}
				thisuser.security.password = str;
				console.putmsg(bbs.text(bbs.text.PasswordChanged));
				log(LOG_NOTICE,'changed password');
			}
			if (file_exists(userSigFilename)) {
				if (console.yesno(bbs.text(bbs.text.ViewSignatureQ)))
					console.printfile(userSigFilename);
			}
			if (console.yesno(bbs.text(bbs.text.CreateEditSignatureQ)))
				console.editfile(userSigFilename);
			else if (!console.aborted) {
				if (file_exists(userSigFilename)) {
					if (console.yesno(bbs.text(bbs.text.DeleteSignatureQ)))
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
			var keylist = console.quit_key;
			console.newline();
			console.print(gettext("Choose a default file transfer protocol (or [ENTER] for None):"));
			console.newline(2);
			keylist += bbs.xfer_prot_menu();
			console.mnemonics(bbs.text(bbs.text.ProtocolOrQuit));
			var kp = console.getkeys(keylist);
			if (kp === console.quit_key || console.aborted)
				break;
			thisuser.download_protocol = kp;
			if (kp && console.yesno(bbs.text(bbs.text.HangUpAfterXferQ)))
				thisuser.settings |= USER_AUTOHANG;
			else if (!console.aborted)
				thisuser.settings &= ~USER_AUTOHANG;
			break;
		case 'Q':
		case '\r':
			console.clear_hotspots();
			exit();
	}
}

