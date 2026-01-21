// logon.js

// Synchronet v3.21 Default Logon Module

// @format.tab-size 4, @format.use-tabs true

"use strict";

require("sbbsdefs.js", 'SS_RLOGIN');
require("nodedefs.js", 'NODE_QUIET');
if(!bbs.mods.avatar_lib)
	bbs.mods.avatar_lib = load({}, 'avatar_lib.js');
if(!bbs.mods.logonlist_lib)
	bbs.mods.logonlist_lib = load({}, 'logonlist_lib.js');
load("fonts.js", "preload", "default");
if(!bbs.mods.userprops)
	bbs.mods.userprops = load({}, "userprops.js");
var options = load("modopts.js", "logon");
if(!options)
	options = {};
if(options.show_avatar === undefined)
	options.show_avatar = true;
if(options.draw_avatar_right === undefined)
	options.draw_avatar_right = true;
if(options.show_logon_list === undefined)
	options.show_logon_list = true;
if(options.guest_name === undefined || options.guest_name === true)
	options.guest_name = "\x01y\x01hFor our records, please enter your full name: \x01w";
if(options.guest_email === undefined || options.guest_email === true)
	options.guest_email = "\x01y\x01hPlease enter your e-mail address: \x01w";
if(options.guest_location === undefined || options.guest_location == true)
	options.guest_location = "\x01y\x01hPlease enter your location (City, State): \x01w";
if(options.guest_referral === undefined || options.guest_referral === true)
	options.guest_referral = "\x01y\x01hWhere did you hear about this BBS?\r\n: \x01w";
if(options.sysop_available) {
	var list = options.sysop_available.split(',');
	bbs.replace_text(bbs.text.LiSysopAvailable, list[random(list.length)].trim());
}
if(options.sysop_unavailable) {
	var list = options.sysop_unavailable.split(',');
	bbs.replace_text(bbs.text.LiSysopNotAvailable, list[random(list.length)].trim());
}

if(options.eval_first)
	eval(options.eval_first);

if(user.settings & USER_ICE_COLOR) {
	var cterm = load({}, "cterm_lib.js");
	cterm.bright_background(true);
}

if(options.email_validation !== undefined && options.email_validation === true) {
	js.exec("emailval.js", {});
	if(!bbs.online)
		exit();
}

// Check if we're being asked to auto-run an external (web interface external programs section uses this)
if ((options.rlogin_auto_xtrn) && (bbs.sys_status & SS_RLOGIN) && (console.terminal.indexOf("xtrn=") === 0)) {
    var external_code = console.terminal.substring(5);
	bbs.node_action = NODE_XTRN;
    if (!bbs.exec_xtrn(external_code)) {
        alert(log(LOG_ERR,"!ERROR Unable to launch external: '" + external_code + "'"));
    }
    bbs.hangup();
	exit();
}
//Disable spinning cursor at pause prompts
//bbs.node_settings|=NM_NOPAUSESPIN	

var prompts = bbs.mods.prompts || load(bbs.mods.prompts = {}, "user_info_prompts.js");
prompts.operation = "logon";

if(user.security.restrictions&UFLAG_G) {
	console.cond_newline();
	if (system.version_hex >= 0x32100) { // Replaces the 3.20 Guest Logon user prompts
		var guest_options = load("modopts.js", "logon:guest_prompts");
		if(!guest_options)
			guest_options = {};
		prompts.get_terminal(user, guest_options);
	}
	while(options.guest_name && bbs.online) {
		write(options.guest_name);
		const name = console.getstr(LEN_NAME,K_UPRLWR);
		if(!name || !name.length)
			continue;
		bbs.log_str("Guest: " + name);
		user.name = name;
		break;
	}

	while(options.guest_email&& bbs.online) {
		write(options.guest_email);
		const email = console.getstr(LEN_NETMAIL);
		if(!email || !email.length)
			continue;
		if(bbs.trashcan("email", email)) {
			bbs.hangup();
			exit();
		}
		bbs.log_str("  " + email);
		user.netmail=email;
		user.settings|=USER_NETMAIL;
		break;
	}

	while(options.guest_location && bbs.online) {
		write(options.guest_location);
		const location=console.getstr(LEN_LOCATION,K_UPRLWR);
		if(!location || !location.length)
			continue;
		if(bbs.trashcan("location", location)) {
			bbs.hangup();
			exit();
		}
		bbs.log_str("  " + location);
		user.location=location;
		break;
	}

	if(options.guest_referral) {
		if(bbs.online)
			bbs.log_str("\r\n");
		while(bbs.online) {
			write(options.guest_referral);
			const ref=console.getstr(70);
			if(!ref || !ref.length)
				continue;
			bbs.log_str(ref + "\r\n");
			break;
		}
	}
}
else { // !Guest

	// Replaces the 3.20 Logon user prompts
	if(!user.name)
		prompts.get_name();
	if((system.newuser_questions & UQ_HANDLE) && !user.handle)
		prompts.get_handle();
	if((system.newuser_questions & UQ_ADDRESS) && !user.address)
		prompts.get_address();
	if((system.newuser_questions & (UQ_ADDRESS | UQ_LOCATION)) && !user.location)
		prompts.get_location();
	if((system.newuser_questions & UQ_ADDRESS) && !user.zipcode)
		prompts.get_zipcode();
	if((system.newuser_questions & UQ_PHONE) && !user.phone)
		prompts.get_phone();
	if((system.newuser_questions & UQ_SEX) && !user.gender)
		prompts.get_gender();
	if((system.newuser_questions & UQ_BIRTH)
		&& typeof system.check_birthdate == "function"
		&& !system.check_birthdate(user.birthdate))
		prompts.get_birthdate();
	if(!(system.newuser_questions & UQ_NONETMAIL) && !user.netmail)
		prompts.get_netmail();
}

if(!bbs.online)
	exit(1);

// Force split-screen chat on ANSI users
if(!(user.chat_settings&CHAT_SPLITP) && console.term_supports(USER_ANSI))
	user.chat_settings|=CHAT_SPLITP;

// Inactivity exemption
if(user.security.exemptions&UFLAG_H)
	console.status|=CON_NO_INACT;

/******************************
* Replaces the 2.1 Logon stuff
******************************/
if (!(bbs.sys_status&SS_RLOGIN) || options.rlogin_xtrn_logon !== false) {
	if(options.fast_logon !== true || !(bbs.sys_status&SS_FASTLOGON)
		|| !user.compare_ars(options.fast_logon_requirements)) {

		// Logon screens

		// Print successively numbered logon screens (logon, logon1, logon2, etc.)
		var highest_printed_logon_screen=-1;
		for(var i=0;;i++) {
			var fname="logon";
			if(i)
				fname+=i;
			if(!bbs.menu_exists(fname)) {
				if(i>1)
					break;
				continue;
			}
			bbs.menu(fname);
			highest_printed_logon_screen = i;
		}

		// Print logon screens based on security level
		if(user.security.level > highest_printed_logon_screen
			&& bbs.menu_exists("logon" + user.security.level))
			bbs.menu("logon" + user.security.level);

		// Print one of text/menu/random*.*, picked at random
		// e.g. random1.asc, random2.asc, random3.asc, etc.
		bbs.menu("random*");

		console.clear(LIGHTGRAY);
		bbs.user_event(EVENT_LOGON);
	}

	if(user.security.level==99				/* Sysop logging on */
		&& !system.matchuser("guest")		/* Guest account does not yet exist */
		&& bbs.mods.userprops.get("logon", "makeguest", true) /* Sysop has not asked to stop this question */
		) {
		if(console.yesno("\x01?Create Guest/Anonymous user account (highly recommended)"))
			load("makeguest.js");
		else if(!console.yesno("Ask again later")) {
			bbs.mods.userprops.set("logon", "makeguest", false);
			console.crlf();
		}
	}

	if(!bbs.online)
		exit(1);

	// Last few callers
	console.aborted=false;
	console.clear(LIGHTGRAY);
	if(options.show_logon_list === true)
		bbs.exec("?logonlist -l");
	if(bbs.node_status != NODE_QUIET && ((system.settings&SYS_SYSSTAT) || !user.is_sysop))
		bbs.mods.logonlist_lib.add();

	// Auto-message
	const auto_msg = system.data_dir + "msgs/auto.msg"
	if(file_size(auto_msg)>0) {
		var pmode = P_NOATCODES|P_WORDWRAP;
		if(options.center_automsg)
			pmode |= P_CENTER;
		console.printfile(auto_msg, pmode);
	}
	console.crlf();

	if(options.show_avatar && console.term_supports(USER_ANSI)) {
		if(options.draw_avatar_above || options.draw_avatar_right)
			bbs.mods.avatar_lib.draw(user.number
				, /* name: */null, /* netaddr: */null
				, options.draw_avatar_above, options.draw_avatar_right
				, /* top */false
				, /* columns */ 80);
		else
			bbs.mods.avatar_lib.show(user.number);
		console.attributes = 7;	// Clear the background attribute
	}
}

// Set rlogin_xtrn_menu=true in [logon] section of ctrl/modopts.ini
// if you want your RLogin server to act as a door game server only
if(options.rlogin_xtrn_menu
	&& bbs.sys_status&SS_RLOGIN) {
	var xtrn_sec;
	if (console.terminal.indexOf("xtrn_sec=") === 0)
		xtrn_sec = console.terminal.substring(9);
	while(bbs.online && !js.terminated) {
		bbs.xtrn_sec(xtrn_sec);
		if (!options.rlogin_xtrn_logoff)
			break;
		if (options.rlogin_xtrn_logoff == "full")
			bbs.logoff(/* prompt: */true);
		else {
			if (console.yesno(bbs.text(bbs.text.LogOffQ)))
				break;
		}
	}
	bbs.hangup();
} else if(!(user.security.restrictions&UFLAG_G)
	&& console.term_supports(USER_ANSI) 
	&& options.set_avatar == true) {
	var avatar = bbs.mods.avatar_lib.read(user.number);
	if(!avatar || (!avatar.data && !avatar.disabled)) {
		alert("You have not selected an avatar.");
		if(console.yesno("Select avatar now"))
			load("avatar_chooser.js");
	}
}

if(options.eval_last)
	eval(options.eval_last);
