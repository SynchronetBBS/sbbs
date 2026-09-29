"use strict";

// QWK Message Packet menu
// The default system "QWK Section" loadable module for Synchronet v3.22
// i.e. in SCFG->System->Loadable Modules->QWK Section

require("sbbsdefs.js", "SCAN_CFG_NEW");

// The menu's command keys, from the (translatable) command-word text strings.
// Keys that collide are logged for the sysop; the first command in this order
// keeps the key, and Quit falls back to 'Q' (#1242).
function command_keys()
{
	var cmds = [
		{ name: "Download",  key: console.download_key },
		{ name: "Upload",    key: console.upload_key },
		{ name: "Configure", key: console.configure_key },
		{ name: "Select",    key: console.select_key },
		{ name: "Pointers",  key: console.pointers_key },
		{ name: "Quit",      key: console.quit_key }
	];
	var keys = {};
	var used = { '?': "Help" };

	for(var i = 0; i < cmds.length; i++) {
		var key = cmds[i].key;
		var problem = null;
		if(!key || key <= ' ')
			problem = format("%s command key is empty", cmds[i].name);
		else if(used[key] !== undefined)
			problem = format("%s and %s command keys collide ('%s')", used[key], cmds[i].name, key);
		if(problem) {
			log(LOG_ERR, format("QWK menu: %s for language '%s'", problem, user.lang));
			if(cmds[i].name != "Quit" || used['Q'] !== undefined)
				continue;
			key = 'Q';
		}
		used[key] = cmds[i].name;
		keys[cmds[i].name] = key;
	}
	return keys;
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

// Like the C++ bprintf() of a text string: a string with no % specifiers is
// printed as-is, with @-codes expanded
function print_text(text_id)
{
	var text = bbs.text(text_id);

	if(text.indexOf('%') < 0)
		console.print(text, P_ATCODES);
	else
		console.print(format.apply(js.global, [text].concat(Array.prototype.slice.call(arguments, 1))));
}

function print_setting(key, text_id, value)
{
	console.add_hotspot(key);
	print_text(text_id, value);
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
		print_text(bbs.text.QWKSettingsHdr, user.alias, user.number);
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
	var keys = command_keys();
	var cmd_keys = "?";

	for(var name in keys) {
		if(name != "Quit")
			cmd_keys += keys[name];
	}

	while(bbs.online) {
		if(menu_every_prompt())
			bbs.menu("qwk");
		bbs.node_action = NODE_TQWK;
		bbs.nodesync();
		console.print(bbs.text(bbs.text.QWKPrompt), P_ATCODES);
		var quit = keys.Quit;
		var ch = console.getkeys(cmd_keys + "\r" + (quit || ""), 0);
		if(typeof ch == "string" && ch > ' ')
			bbs.log_key(ch);
		if(console.aborted || typeof ch != "string" || ch == quit || ch == '\r' || !bbs.online)
			break;
		if(ch == '?') {
			if((console.term_supports(USER_RIP) || !(user.settings & USER_EXPERT))
				&& !is_qwk_node())
				continue;
			bbs.menu("qwk");
		}
		else if(ch == keys.Select) {
			bbs.cfg_msg_scan(SCAN_CFG_NEW);
			remove_temp_files();
		}
		else if(ch == keys.Pointers) {
			bbs.cfg_msg_ptrs();
			remove_temp_files();
		}
		else if(ch == keys.Configure) {
			qwk_settings();
			remove_temp_files();
			console.clear_hotspots();
		}
		else if(ch == keys.Download)
			bbs.qwk_download();
		else if(ch == keys.Upload)
			bbs.qwk_upload();
	}
}

qwk_menu();
