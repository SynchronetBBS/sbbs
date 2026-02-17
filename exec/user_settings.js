// User (default) settings/configuration menu
// for Synchronet v3.21+

"use strict";

console.clear();
require("sbbsdefs.js", 'SS_USERON');
require("userdefs.js", 'USER_SPIN');
require("nodedefs.js", 'NODE_DFLT');
require("gettext.js", 'gettext');
var prompts = bbs.mods.prompts || load(bbs.mods.prompts = {}, "user_info_prompts.js");
var termdesc = bbs.mods.termdesc || load(bbs.mods.termdesc = {}, "termdesc.js");
var lang = bbs.mods.lang || load(bbs.mods.lang = {}, "lang.js");
var options = load("modopts.js", "user_terminal");
if (!options)
	options = {};

prompts.operation = "";

function on_or_off(on)
{
	return bbs.text(on ? bbs.text.On : bbs.text.Off);
}

function display_menu(thisuser)
{
	var keys = 'Q\r';

	const curspin = (thisuser.settings & USER_SPIN) ? bbs.text(bbs.text.On)
		: (thisuser.settings & USER_NOPAUSESPIN) ? bbs.text(bbs.text.Off) : gettext("Pause Prompt") + " " + bbs.text(bbs.text.Only);
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
	console.aborted = false;
	console.print(format(bbs.text(bbs.text.UserDefaultsHdr),thisuser.alias,thisuser.number));
	if (bbs.text(bbs.text.UserDefaultsTerminal).length) {
		keys += 'T';
		console.add_hotspot('T');
		console.print(format(bbs.text(bbs.text.UserDefaultsTerminal)
			,termdesc.type(/* verbosity: */2, (bbs.sys_status & SS_USERON)
				&& (thisuser.number == user.number && !user_is_guest) ? undefined : thisuser)));
	}
	if (bbs.text(bbs.text.UserDefaultsCommandSet).length
		&& main_cfg.shell.length > 1) {
		keys += 'K';
		console.add_hotspot('K');
		console.print(format(bbs.text(bbs.text.UserDefaultsCommandSet), cmdshell));
	}
	if (bbs.text(bbs.text.UserDefaultsLanguage).length && lang.count() > 1) {
		keys += 'L';
		console.add_hotspot('L');
		console.print(format(bbs.text(bbs.text.UserDefaultsLanguage)
			,bbs.text(bbs.text.Language), bbs.text(bbs.text.LANG)));
	}
	if (bbs.text(bbs.text.UserDefaultsXeditor).length
		&& Object.getOwnPropertyNames(xtrn_area.editor).length > 0) {
		keys += 'E';
		console.add_hotspot('E');
		console.print(format(bbs.text(bbs.text.UserDefaultsXeditor)
			,(thisuser.editor && xtrn_area.editor[thisuser.editor])
				? xtrn_area.editor[thisuser.editor].name : bbs.text(bbs.text.None)));
	}
	if (bbs.text(bbs.text.UserDefaultsArcType).length) {
		keys += 'A';
		console.add_hotspot('A');
		console.print(format(bbs.text(bbs.text.UserDefaultsArcType)
			,thisuser.temp_file_ext));
	}
	if (bbs.text(bbs.text.UserDefaultsMenuMode).length) {
		keys += 'X';
		console.add_hotspot('X');
		console.print(format(bbs.text(bbs.text.UserDefaultsMenuMode)
			,on_or_off(thisuser.settings & USER_EXPERT)));
	}
	if (bbs.text(bbs.text.UserDefaultsPause).length) {
		keys += 'P';
		console.add_hotspot('P');
		console.print(format(bbs.text(bbs.text.UserDefaultsPause)
			,on_or_off(thisuser.settings & USER_PAUSE)));
	}
	if (bbs.text(bbs.text.UserDefaultsHotKey).length) {
		keys += 'H';
		console.add_hotspot('H');
		console.print(format(bbs.text(bbs.text.UserDefaultsHotKey)
			,on_or_off(!(thisuser.settings & USER_COLDKEYS))));
	}
	if (bbs.text(bbs.text.UserDefaultsCursor).length) {
		keys += 'S';
		console.add_hotspot('S');
		console.print(format(bbs.text(bbs.text.UserDefaultsCursor)
			,curspin));
	}
	if (bbs.text(bbs.text.UserDefaultsCLS).length) {
		keys += 'C';
		console.add_hotspot('C');
		console.print(format(bbs.text(bbs.text.UserDefaultsCLS)
			,on_or_off(thisuser.settings & USER_CLRSCRN)));
	}
	if (bbs.text(bbs.text.UserDefaultsAskNScan).length) {
		keys += 'N';
		console.add_hotspot('N');
		console.print(format(bbs.text(bbs.text.UserDefaultsAskNScan)
			,on_or_off(thisuser.settings & USER_ASK_NSCAN)));
	}
	if (bbs.text(bbs.text.UserDefaultsAskSScan).length) {
		keys += 'Y';
		console.add_hotspot('Y');
		console.print(format(bbs.text(bbs.text.UserDefaultsAskSScan)
			,on_or_off(thisuser.settings & USER_ASK_SSCAN)));
	}
	if (bbs.text(bbs.text.UserDefaultsANFS).length) {
		keys += 'F';
		console.add_hotspot('F');
		console.print(format(bbs.text(bbs.text.UserDefaultsANFS)
			,on_or_off(thisuser.settings & USER_ANFSCAN)));
	}
	if (bbs.text(bbs.text.UserDefaultsRemember).length) {
		keys += 'R';
		console.add_hotspot('R');
		console.print(format(bbs.text(bbs.text.UserDefaultsRemember)
			,on_or_off(thisuser.settings & USER_CURSUB)));
	}
	if (bbs.text(bbs.text.UserDefaultsBatFlag).length) {
		keys += 'B';
		console.add_hotspot('B');
		console.print(format(bbs.text(bbs.text.UserDefaultsBatFlag)
			,on_or_off(thisuser.settings & USER_BATCHFLAG)));
	}
	if (bbs.text(bbs.text.UserDefaultsNetMail).length
		&& (system.settings & SYS_FWDTONET)) {
		keys += 'M';
		console.add_hotspot('M');
		console.print(format(bbs.text(bbs.text.UserDefaultsNetMail)
			,on_or_off(thisuser.settings & USER_NETMAIL), thisuser.netmail));
	}
	if (bbs.text(bbs.text.UserDefaultsQuiet).length
		&& (thisuser.security.exemptions & UFLAG_Q)) {
		keys += 'D';
		console.add_hotspot('D');
		console.print(format(bbs.text(bbs.text.UserDefaultsQuiet)
			,on_or_off(thisuser.settings & USER_QUIET)));
	}
	if (bbs.text(bbs.text.UserDefaultsProtocol).length) {
		keys += 'Z';
		console.add_hotspot('Z');
		console.print(format(bbs.text(bbs.text.UserDefaultsProtocol)
			,protname + ' '
			,autohang));
	}
	if (bbs.text(bbs.text.UserDefaultsPassword).length && !(user_is_guest)) {
		keys += 'W';
		console.add_hotspot('W');
		console.print(bbs.text(bbs.text.UserDefaultsPassword));
	}
	console.print(options.prompt || bbs.text(bbs.text.UserDefaultsWhich), P_ATCODES);
	console.add_hotspot('Q');

	return keys;
}

var cfglib = load({}, "cfglib.js");
var file_cfg = cfglib.read("file.ini");
var main_cfg = cfglib.read("main.ini");

var thisuser = new User(argv[0] || user.number);
var user_is_guest = (thisuser.security.restrictions & UFLAG_G);

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
				if (!bbs.select_editor(thisuser)) {
					console.print(gettext("Sorry, no external editors are available to you", "no_external_editors"))
					console.newline();
				}
			}
			if (thisuser.number === user.number && user_is_guest)
				user.editor = thisuser.editor;
			break;
		case 'F':
			thisuser.settings ^= USER_ANFSCAN;
			break;
		case 'H':
			thisuser.settings ^= USER_COLDKEYS;
			break;
		case 'K':
			bbs.select_shell(thisuser);
			if (thisuser.number === user.number && user_is_guest)
				user.command_shell = thisuser.command_shell;
			break;
		case 'L': /* Language */
			lang.select(thisuser.number === user.number ? user : thisuser);
			break;
		case 'M':
			prompts.get_netmail(thisuser);
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
			js.exec("user_terminal.js", { user: thisuser });
			break;
		case 'W':
			js.exec("user_personal.js", { user: thisuser });
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
			console.print(gettext("Choose a default file transfer protocol (or [ENTER] for None):", "choose_protocol_or_none"));
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

