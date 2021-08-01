// logon.js

// Synchronet v3.1 Default Logon Module

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
if(options.sysop_available) {
	require("text.js", 'LiSysopAvailable');
	var list = options.sysop_available.split(',');
	bbs.replace_text(LiSysopAvailable, list[random(list.length)].trim());
}
if(options.sysop_unavailable) {
	require("text.js", 'LiSysopNotAvailable');
	var list = options.sysop_unavailable.split(',');
	bbs.replace_text(LiSysopNotAvailable, list[random(list.length)].trim());
}

if(options.eval_first)
	eval(options.eval_first);

if(user.settings & USER_ICE_COLOR) {
	var cterm = load({}, "cterm_lib.js");
	cterm.bright_background(true);
}

if(options.email_validation == true) {
	load({}, "emailval.js");
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

if(user.security.restrictions&UFLAG_G) {
	while(options.guest_name !== false && bbs.online) {
		printf("\x01y\x01hFor our records, please enter your full name: \x01w");
		const name = console.getstr(LEN_NAME,K_UPRLWR);
		if(!name || !name.length)
			continue;
		bbs.log_str("Guest: " + name);
		user.name = name;
		break;
	}

	while(options.guest_email !== false && bbs.online) {
		printf("\x01y\x01hPlease enter your e-mail address: \x01w");
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

	while(options.guest_location !== false && bbs.online) {
		printf("\x01y\x01hPlease enter your location (City, State): \x01w");
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

	if(options.guest_referral !== false) {
		if(bbs.online)
			bbs.log_str("\r\n");
		while(bbs.online) {
			printf("\x01y\x01hWhere did you hear about this BBS?\r\n: \x01w");
			const ref=console.getstr(70);
			if(!ref || !ref.length)
				continue;
			bbs.log_str(ref + "\r\n");
			break;
		}
	}
}

// Force split-screen chat on ANSI users
if(console.term_supports(USER_ANSI))
	user.chat_settings|=CHAT_SPLITP;

// Inactivity exemption
if(user.security.exemptions&UFLAG_H)
	console.status|=CON_NO_INACT;

/******************************
* Replaces the 2.1 Logon stuff
******************************/

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
	console.printfile(auto_msg,P_NOATCODES|P_WORDWRAP);
}
console.crlf();

if(options.show_avatar && console.term_supports(USER_ANSI)) {
	if(options.draw_avatar_above || options.draw_avatar_right)
		bbs.mods.avatar_lib.draw(user.number, /* name: */null, /* netaddr: */null, options.draw_avatar_above, options.draw_avatar_right);
	else
		bbs.mods.avatar_lib.show(user.number);
	console.attributes = 7;	// Clear the background attribute
}

// Set rlogin_xtrn_menu=true in [logon] section of ctrl/modopts.ini
// if you want your RLogin server to act as a door game server only
if(options.rlogin_xtrn_menu
	&& bbs.sys_status&SS_RLOGIN) {
	bbs.xtrn_sec();
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
