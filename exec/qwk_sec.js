// QWK Message Packet menu
// The default system "QWK Section" loadable module for Synchronet v3.22
// i.e. in SCFG->System->Loadable Modules->QWK Section

require("sbbsdefs.js", "SCAN_CFG_NEW");

"use strict";

// The first letter of the (translatable) Quit text, unless that letter is
// already one of the menu's command keys (#1242)
function quit_key(cmd_keys)
{
	var quit = console.quit_key.toUpperCase();

	if(quit != 'Q' && (!quit || cmd_keys.indexOf(quit) >= 0) && cmd_keys.indexOf('Q') < 0)
		return 'Q';
	return quit;
}

function remove_temp_files()
{
	var files = directory(system.temp_dir + "*");

	for(var i = 0; i < files.length; i++)
		file_remove(files[i]);
}

function is_qwk_node()
{
	return (user.security.restrictions & UREST_QWK_NODE) != 0;
}

function menu_every_prompt()
{
	return (console.term_supports(USER_RIP) || !(user.settings & USER_EXPERT))
		&& (user.stats.total_logons < 2 || !is_qwk_node());
}

function yes_no(value)
{
	return bbs.text(value ? bbs.text.Yes : bbs.text.No);
}

function print_setting(key, text_id, value)
{
	console.add_hotspot(key);
	console.print(format(bbs.text(text_id), value));
}

function select_archive_type()
{
	var formats = bbs.get_archive_formats();

	for(var i = 0; i < formats.length; i++)
		console.uselect(i, bbs.text(bbs.text.ArchiveTypeHeading), formats[i]);
	var choice = console.uselect();
	if(choice >= 0)
		user.temp_file_ext = formats[choice];
}

function qwk_settings()
{
	while(bbs.online) {
		var qwk = user.qwk_settings;
		console.clear();
		console.print(format(bbs.text(bbs.text.QWKSettingsHdr), user.alias, user.number));
		print_setting('A', bbs.text.QWKSettingsCtrlA
			, (qwk & QWK_EXPCTLA) ? "Expand to ANSI" : (qwk & QWK_RETCTLA) ? "Leave in" : "Strip");
		print_setting('T', bbs.text.QWKSettingsArchive, user.temp_file_ext);
		print_setting('E', bbs.text.QWKSettingsEmail
			, (qwk & QWK_EMAIL) ? "Un-read Only" : yes_no(qwk & QWK_ALLMAIL));
		if(qwk & (QWK_ALLMAIL | QWK_EMAIL)) {
			print_setting('I', bbs.text.QWKSettingsAttach, yes_no(qwk & QWK_ATTACH));
			print_setting('D', bbs.text.QWKSettingsDeleteEmail, yes_no(qwk & QWK_DELMAIL));
		}
		print_setting('F', bbs.text.QWKSettingsNewFilesList, yes_no(qwk & QWK_FILES));
		print_setting('N', bbs.text.QWKSettingsIndex, yes_no(!(qwk & QWK_NOINDEX)));
		print_setting('C', bbs.text.QWKSettingsControl, yes_no(!(qwk & QWK_NOCTRL)));
		print_setting('V', bbs.text.QWKSettingsVoting, yes_no(qwk & QWK_VOTING));
		print_setting('H', bbs.text.QWKSettingsHeaders, yes_no(qwk & QWK_HEADERS));
		print_setting('Y', bbs.text.QWKSettingsBySelf, yes_no(qwk & QWK_BYSELF));
		print_setting('Z', bbs.text.QWKSettingsTimeZone, yes_no(qwk & QWK_TZ));
		print_setting('P', bbs.text.QWKSettingsVIA, yes_no(qwk & QWK_VIA));
		print_setting('M', bbs.text.QWKSettingsMsgID, yes_no(qwk & QWK_MSGID));
		print_setting('U', bbs.text.QWKSettingsUtf8, yes_no(qwk & QWK_UTF8));
		print_setting('G', bbs.text.QWKSettingsMIME, yes_no(qwk & QWK_MIME));
		print_setting('W', bbs.text.QWKSettingsWrapText, yes_no(qwk & QWK_WORDWRAP));
		print_setting('X', bbs.text.QWKSettingsExtended, yes_no(qwk & QWK_EXT));
		console.print(bbs.text(bbs.text.QWKSettingsWhich), P_ATCODES);
		console.add_hotspot('Q');
		var ch = console.getkeys("AEDFGHIOPQTUYMNCXZVW", 0);
		if(console.aborted || typeof ch != "string" || ch == 'Q' || !bbs.online)
			break;
		switch(ch) {
			case 'A':
				if(!(qwk & (QWK_EXPCTLA | QWK_RETCTLA)))
					qwk |= QWK_EXPCTLA;
				else if(qwk & QWK_EXPCTLA) {
					qwk &= ~QWK_EXPCTLA;
					qwk |= QWK_RETCTLA;
				}
				else
					qwk &= ~(QWK_EXPCTLA | QWK_RETCTLA);
				break;
			case 'T':
				select_archive_type();
				break;
			case 'E':
				if(!(qwk & (QWK_EMAIL | QWK_ALLMAIL)))
					qwk |= QWK_EMAIL;
				else if(qwk & QWK_EMAIL) {
					qwk &= ~QWK_EMAIL;
					qwk |= QWK_ALLMAIL;
				}
				else
					qwk &= ~(QWK_EMAIL | QWK_ALLMAIL);
				break;
			case 'H': qwk ^= QWK_HEADERS; break;
			case 'I': qwk ^= QWK_ATTACH; break;
			case 'D': qwk ^= QWK_DELMAIL; break;
			case 'F': qwk ^= QWK_FILES; break;
			case 'N': qwk ^= QWK_NOINDEX; break;
			case 'C': qwk ^= QWK_NOCTRL; break;
			case 'Z': qwk ^= QWK_TZ; break;
			case 'P': qwk ^= QWK_VIA; break;
			case 'M': qwk ^= QWK_MSGID; break;
			case 'Y': qwk ^= QWK_BYSELF; break;
			case 'V': qwk ^= QWK_VOTING; break;
			case 'U': qwk ^= QWK_UTF8; break;
			case 'W': qwk ^= QWK_WORDWRAP; break;
			case 'G': qwk ^= QWK_MIME; break;
			case 'X': qwk ^= QWK_EXT; break;
		}
		user.qwk_settings = qwk;
	}
}

function qwk_menu()
{
	var cmd_keys = "?UDCSP";

	while(bbs.online) {
		if(menu_every_prompt())
			bbs.menu("qwk");
		bbs.node_action = NODE_TQWK;
		bbs.nodesync();
		console.print(bbs.text(bbs.text.QWKPrompt), P_ATCODES);
		var quit = quit_key(cmd_keys);
		var ch = console.getkeys(cmd_keys + "\r" + quit, 0);
		if(typeof ch == "string" && ch > ' ')
			bbs.log_key(ch);
		if(console.aborted || typeof ch != "string" || ch == quit || ch == '\r' || !bbs.online)
			break;
		switch(ch) {
			case '?':
				if((console.term_supports(USER_RIP) || !(user.settings & USER_EXPERT))
					&& !is_qwk_node())
					break;
				bbs.menu("qwk");
				break;
			case 'S':
				bbs.cfg_msg_scan(SCAN_CFG_NEW);
				remove_temp_files();
				break;
			case 'P':
				bbs.cfg_msg_ptrs();
				remove_temp_files();
				break;
			case 'C':
				qwk_settings();
				remove_temp_files();
				console.clear_hotspots();
				break;
			case 'D':
				bbs.qwk_download();
				break;
			case 'U':
				bbs.qwk_upload();
				break;
		}
	}
}

qwk_menu();
