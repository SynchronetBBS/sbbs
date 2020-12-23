// $Id: showmsgavatar.js,v 1.9 2019/06/15 01:23:30 rswindell Exp $

// This can be loaded from text/menu/msghdr.asc via @EXEC:SHOWMSGAVATAR@
// Don't forget to include or exclude the blank line after if you do
// (or don't) want a blank line separating message headers and body text

// This script may be used instead of (not in addition to) showmsghdr.js
// Use this script if you have/want a custom message header defined in
// msghdr.asc, i.e. with @-codes (not using text.dat strings).
// If you do not want the avatar right-justified, copy this file to your
// mods directory and change that parameter below.

if(!bbs.mods.smbdefs)
	load(bbs.mods.smbdefs = {}, "smbdefs.js");

function get_options()
{
	var options = bbs.mods.avatars_options;
	if(!options) {
		options = load({}, "modopts.js", "avatars");
		if(!options)
			options = { cached: true };
		if(options.msghdr_draw_top === undefined)
			options.msghdr_draw_top = true;
		if(options.msghdr_draw_above === undefined)
			options.msghdr_draw_above = true;
		if(options.msghdr_draw_right === undefined)
			options.msghdr_draw_right = true;
		bbs.mods.avatars_options = options;	// cache the options
	}
	return options;
}

function draw_default_avatar(sub)
{
	var options = get_options();
	var avatar = options[sub + "_default"];
	if(!avatar)
		avatar = options[msg_area.sub[sub].grp_name.toLowerCase() + "_default"];
	if(!avatar)
		avatar = options.msg_default;
	if(avatar)
		bbs.mods.avatar_lib.draw_bin(avatar
			,options.msghdr_draw_above, options.msghdr_draw_right
			,options.msghdr_draw_top && bbs.msghdr_top_of_screen);
}

// Avatar support here:
if(!(bbs.msg_attr&bbs.mods.smbdefs.MSG_ANONYMOUS)) {
	if(!bbs.mods.avatar_lib)
		bbs.mods.avatar_lib = load({}, 'avatar_lib.js');
	var options = get_options();
	var success = bbs.mods.avatar_lib.draw(bbs.msg_from_ext, bbs.msg_from, bbs.msg_from_net
		,options.msghdr_draw_above, options.msghdr_draw_right
		,options.msghdr_draw_top && bbs.msghdr_top_of_screen);
	if(!success && bbs.smb_sub_code) {
		draw_default_avatar(bbs.smb_sub_code);
	}
	console.attributes = 7;	// Clear the background attribute as the next line might scroll, filling with BG attribute
}
