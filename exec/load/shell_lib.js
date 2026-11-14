// Library for writing command shells
// portions derived from classic_shell.js, default.src, and exec*.cpp

// @format.tab-size 4

"use strict";

require("sbbsdefs.js", "USER_EXPERT");
require("nodedefs.js", "NODE_MAIN");
require("gettext.js", "gettext");
require("select_item.js", "select_item");

var options = load("modopts.js", "shell");
if(!options)
	options = {};

// modopts.ini [shell] key prefixes for the <prefix>_sound, <prefix>_music and
// <prefix>_enter options acted upon when entering a section of the BBS
var section_key = {};
section_key[NODE_MAIN] = "main";
section_key[NODE_XFER] = "file";

var usrsubs = [];
var usrdirs = [];
var usrgrps = msg_area.grp_list.length;
var usrlibs = file_area.lib_list.length;
for(var i = 0; i < usrgrps; ++i)
	usrsubs[i] = msg_area.grp_list[i].sub_list.length;
for(var i = 0; i < usrlibs; ++i)
	usrdirs[i] = file_area.lib_list[i].dir_list.length;

// The 'bbs' object only exposes the current sub/dir of the current
// group/library, so select the group/library briefly to read it
function cursub(grp)
{
	var curgrp = bbs.curgrp;
	bbs.curgrp = grp;
	var sub = bbs.cursub;
	bbs.curgrp = curgrp;
	return sub;
}

function curdir(lib)
{
	var curlib = bbs.curlib;
	bbs.curlib = lib;
	var dir = bbs.curdir;
	bbs.curlib = curlib;
	return dir;
}

function get_num(str, max)
{
	var num = Number(str);
	while(bbs.online && !js.terminated) {
		if(num * 10 > max)
			break;
		var ch = console.getkey(K_UPPER);
		if(ch < '0' || ch > '9') {
			if(ch > ' ')
				console.ungetstr(ch);
			break;
		}
		console.print(ch);
		num *= 10;
		num += Number(ch);
		if(num > max)
			return 0;
	}
	return num;
}

function get_grp_num(str)
{
	var num = get_num(str, usrgrps);
	if(num > 0)
		bbs.curgrp = num -1;
}

function get_sub_num(str)
{
	var num = get_num(str, usrsubs[bbs.curgrp]);
	if(num > 0)
		bbs.cursub = num - 1;
}

function get_lib_num(str)
{
	var num = get_num(str, usrlibs);
	if(num > 0)
		bbs.curlib = num -1;
}

function get_dir_num(str)
{
	var num = get_num(str, usrdirs[bbs.curlib]);
	if(num > 0)
		bbs.curdir = num - 1;
}

// The prefix (e.g. '*') printed before an item of an area list, per the
// modopts.ini [shell] default_item_prefix and nondefault_item_prefix options
function area_item_prefix(is_default)
{
	if(is_default)
		return options.default_item_prefix !== undefined ? options.default_item_prefix : '*';
	return options.nondefault_item_prefix !== undefined ? options.nondefault_item_prefix : ' ';
}

// Indent of the "more" indicator: aligned with the item numbers that follow
// the default item prefix (e.g. '*')
function area_more_indent()
{
	return Math.max(strip_ctrl(area_item_prefix(true)).length
		, strip_ctrl(area_item_prefix(false)).length) + 1;
}

// The width of the text.dat area list formats' description field (60),
// less any columns taken by a prefix wider than the stock 1-column '*', so
// that a line of an area list doesn't reach the last column and wrap
function area_desc_field_width()
{
	var prefix_width = Math.max(strip_ctrl(area_item_prefix(true)).length
		, strip_ctrl(area_item_prefix(false)).length);
	return Math.max(60 - Math.max(prefix_width - 1, 0), 1);
}

// The widest description in a list of areas (as displayed in the description field)
function area_desc_widest(list)
{
	var field_width = area_desc_field_width();
	var widest = 0;
	for(var i = 0; i < list.length; ++i)
		widest = Math.max(widest, Math.min(list[i].description.length, field_width));
	return widest;
}

// Format an area list item using the text.dat format 'fmt', narrowing its
// description field to fit a wide prefix, and highlighting the description
// of the default item with the modopts.ini [shell] default_item_attr option.
// Both apply only if the format's description field is the expected width
// (%-60.60s), so that a customized format is never misaligned.
function area_item_str(fmt, num, desc, count, widest, is_default)
{
	var field = "%-60.60s";
	if(fmt.indexOf(field) >= 0) {
		var width = area_desc_field_width();
		if(is_default && options.default_item_attr) {
			desc = options.default_item_attr + format("%-*s", widest, desc.substr(0, widest))
				+ "\x01n" + format("%-*s", width - widest, "");
			fmt = fmt.replace(field, "%s");
		} else
			fmt = fmt.replace(field, "%-" + width + "." + width + "s");
	}
	// The caller (or select_item) ends the line
	return format(fmt, num, desc, "", count).replace(/[\r\n]+$/, "");
}

function print_grp(i, is_default, widest, redraw, eol)
{
	if(widest === undefined)
		widest = area_desc_widest(msg_area.grp_list);
	console.print(area_item_prefix(is_default));
	if(i<9) console.print(' ');
	if(!redraw)
		console.add_hotspot(i+1);
	console.print(area_item_str(bbs.text(bbs.text.GrpLstFmt), i+1
		,msg_area.grp_list[i].description, msg_area.grp_list[i].sub_list.length
		,widest, is_default));
	if(eol !== false)
		console.newline();
}

function print_sub(grp, i, is_default, widest, redraw, eol)
{
	if(widest === undefined)
		widest = area_desc_widest(msg_area.grp_list[grp].sub_list);
	console.print(area_item_prefix(is_default));
	var str = area_item_str(bbs.text(bbs.text.SubLstFmt), i+1
		,msg_area.grp_list[grp].sub_list[i].description
		,msg_area.grp_list[grp].sub_list[i].posts
		,widest, is_default);
	if(i<9) console.print(' ');
	if(i<99) console.print(' ');
	if(!redraw)
		console.add_hotspot(i+1);
	console.print(str);
	if(eol !== false)
		console.newline();
}

function sub_list_hdr(grp)
{
	return "\r\n" + format(bbs.text(bbs.text.SubLstHdr), msg_area.grp_list[grp].description);
}

// List Message Groups
function show_grps()
{
	if(msg_area.grp_list.length < 1)
		return;
	if(bbs.menu("grps", P_NOERROR))
		return;
	console.print(bbs.text(bbs.text.GrpLstHdr));
	var widest = area_desc_widest(msg_area.grp_list);
	for(var i=0; i < msg_area.grp_list.length && !console.aborted; i++)
		print_grp(i, i == bbs.curgrp, widest);
}

function show_subs(grp)
{
	if(msg_area.grp_list.length < 1)
		return;
	if(bbs.menu("subs" + (msg_area.grp_list[grp].number + 1), P_NOERROR))
		return;
	console.print(sub_list_hdr(grp));
	var sub = cursub(grp);
	var widest = area_desc_widest(msg_area.grp_list[grp].sub_list);
	for(var i=0; i < usrsubs[grp] && !console.aborted; ++i)
		print_sub(grp, i, i == sub, widest);
}

function select_msg_area()
{
	if(usrgrps < 1)
		return false;
	var grp = bbs.curgrp;
	var grp_widest = area_desc_widest(msg_area.grp_list);
	while(bbs.online) {
		if(usrgrps > 1) {
			var i = select_item({
				count: usrgrps,
				current: grp,
				header: bbs.text(bbs.text.GrpLstHdr),
				prompt: function(num) { return format(bbs.text(bbs.text.JoinWhichGrp), num); },
				print_item: function(i, is_default, redraw) { print_grp(i, is_default, grp_widest, redraw, false); },
				menu: function() { return bbs.menu("grps", P_NOERROR); },
				more_fmt: options.more_fmt,
				more_indent: area_more_indent()
			});
			if(i < 0)
				return false;
			grp = i;
		}
		var sub_widest = area_desc_widest(msg_area.grp_list[grp].sub_list);
		var sub = select_item({
			count: usrsubs[grp],
			current: cursub(grp),
			header: sub_list_hdr(grp),
			prompt: function(num) { return format(bbs.text(bbs.text.JoinWhichSub), num); },
			print_item: function(i, is_default, redraw) { print_sub(grp, i, is_default, sub_widest, redraw, false); },
			menu: function() { return bbs.menu("subs" + (msg_area.grp_list[grp].number + 1), P_NOERROR); },
			more_fmt: options.more_fmt,
			more_indent: area_more_indent()
		});
		if(sub < 0) {
			if(usrgrps==1)
				return false;
			continue;
		}
		bbs.curgrp=grp;
		bbs.cursub=sub;
		return true;
	}
	return false;
}

function print_lib(i, is_default, widest, redraw, eol)
{
	if(widest === undefined)
		widest = area_desc_widest(file_area.lib_list);
	console.print(area_item_prefix(is_default));
	if(i<9) console.print(' ');
	if(!redraw)
		console.add_hotspot(i+1);
	console.print(area_item_str(bbs.text(bbs.text.LibLstFmt), i+1
		,file_area.lib_list[i].description, file_area.lib_list[i].dir_list.length
		,widest, is_default));
	if(eol !== false)
		console.newline();
}

function print_dir(lib, i, is_default, widest, redraw, eol)
{
	if(widest === undefined)
		widest = area_desc_widest(file_area.lib_list[lib].dir_list);
	console.print(area_item_prefix(is_default));
	var str = area_item_str(bbs.text(bbs.text.DirLstFmt), i+1
		,file_area.lib_list[lib].dir_list[i].description
		,file_area.lib_list[lib].dir_list[i].files
		,widest, is_default);
	if(i<9) console.print(' ');
	if(i<99) console.print(' ');
	if(!redraw)
		console.add_hotspot(i+1);
	console.print(str);
	if(eol !== false)
		console.newline();
}

function dir_list_hdr(lib)
{
	return "\r\n" + format(bbs.text(bbs.text.DirLstHdr), file_area.lib_list[lib].description);
}

// List File Libraries
function show_libs()
{
	if(file_area.lib_list.length < 1)
		return;
	if(bbs.menu("libs", P_NOERROR))
		return;
	console.print(bbs.text(bbs.text.LibLstHdr));
	var widest = area_desc_widest(file_area.lib_list);
	for(var i=0; i < file_area.lib_list.length && !console.aborted; i++)
		print_lib(i, i == bbs.curlib, widest);
}

function show_dirs(lib)
{
	if(file_area.lib_list.length < 1)
		return;
	if(bbs.menu("dirs" + (file_area.lib_list[lib].number + 1), P_NOERROR))
		return;
	console.print(dir_list_hdr(lib));
	var dir = curdir(lib);
	var widest = area_desc_widest(file_area.lib_list[lib].dir_list);
	for(var i=0; i < usrdirs[lib] && !console.aborted; ++i)
		print_dir(lib, i, i == dir, widest);
}

function select_file_area()
{
	var usrlibs = file_area.lib_list.length;
	if(usrlibs < 1)
		return false;
	var lib = bbs.curlib;
	var lib_widest = area_desc_widest(file_area.lib_list);
	while(bbs.online) {
		if(usrlibs > 1) {
			var i = select_item({
				count: usrlibs,
				current: lib,
				header: bbs.text(bbs.text.LibLstHdr),
				prompt: function(num) { return format(bbs.text(bbs.text.JoinWhichLib), num); },
				print_item: function(i, is_default, redraw) { print_lib(i, is_default, lib_widest, redraw, false); },
				menu: function() { return bbs.menu("libs", P_NOERROR); },
				more_fmt: options.more_fmt,
				more_indent: area_more_indent()
			});
			if(i < 0)
				return false;
			lib = i;
		}
		var dir_widest = area_desc_widest(file_area.lib_list[lib].dir_list);
		var dir = select_item({
			count: usrdirs[lib],
			current: curdir(lib),
			header: dir_list_hdr(lib),
			prompt: function(num) { return format(bbs.text(bbs.text.JoinWhichDir), num); },
			print_item: function(i, is_default, redraw) { print_dir(lib, i, is_default, dir_widest, redraw, false); },
			menu: function() { return bbs.menu("dirs" + (file_area.lib_list[lib].number + 1), P_NOERROR); },
			more_fmt: options.more_fmt,
			more_indent: area_more_indent()
		});
		if(dir < 0) {
			if(usrlibs==1)
				return false;
			continue;
		}
		bbs.curlib=lib;
		bbs.curdir=dir;
		return true;
	}
	return false;
}

function main_info()
{
	while(bbs.online && !js.terminated) {
		if(!(user.settings & USER_EXPERT))
			bbs.menu("maininfo");
		bbs.nodesync();
		console.print("\r\n\x01y\x01h"+ gettext("Main") + " " + gettext("Info") + ": \x01n");
		var key = console.getkeys("?QISVY\r", 0);
		bbs.log_key(key);
		switch(key) {
		case '?':
			if(user.settings & USER_EXPERT)
				bbs.menu("maininfo");
			break;
		case 'I':
			bbs.sys_info();
			break;
		case 'S':
			bbs.sub_info();
			break;
		case 'Y':
			bbs.user_info();
			break;
		case 'V':
			bbs.ver();
			break;
		default:
			return;
		}
	}
}

// Prompts for new-file-scan the first time the user enters the file section
function enter_file_section()
{
	bbs.menu("tmessage", P_NOERROR);
	if(bbs.file_cmds > 0)
		return;
	if(!(user.settings & USER_ASK_NSCAN))
		return;
	if(user.new_file_time == 0)
		return;
	console.cond_blankline();
	if(console.yesno(gettext("Search all libraries for new files")))
		bbs.scan_dirs(FL_ULTIME, /* all */true);
}

function file_info()
{
	var key;

	while(1) {
		if(!(user.settings & USER_EXPERT))
			bbs.menu("xferinfo");
		bbs.nodesync();
		console.print("\r\n\x01y\x01h" + gettext("File") + " " + gettext("Info") + ": \x01n");
		key=console.getkeys("?TYDVUQ\r", 0);
		bbs.log_key(key);

		switch(key) {
			case '?':
				if(user.settings & USER_EXPERT)
					bbs.menu("xferinfo");
				break;

			case 'T':
				bbs.xfer_policy();
				break;

			case 'Y':
				bbs.user_info();
				break;

			case 'D':
				bbs.dir_info();
				break;

			case 'V':
				bbs.ver();
				break;

			case 'U':
				bbs.list_users(UL_DIR);
				break;

			case 'Q':
			default:
				return;
		}
	}
}

function list_users()
{
	console.print("\r\n\x01c\x01h" + "List Users" + "\r\n");
	console.mnemonics("\r\n~" + gettext("Logons Today") + ", ~" + gettext("Yesterday") + ", ~" + gettext("Sub-board") + ", " + gettext("or") + " ~@All@: ");
	switch(console.getkeys("LSY\r" + console.all_key, 0)) {
	case 'L':
		bbs.list_logons();
		break;
	case 'Y':
		bbs.exec("?logonlist -y");
		break;
	case 'S':
		bbs.list_users(UL_SUB);
		break;
	case console.all_key:
		bbs.list_users(UL_ALL);
		break;
	}
}

function list_files()
{
	var result = bbs.list_files();
	if(result < 0)
		return;
	if(result == 0)
		console.print(bbs.text(bbs.text.EmptyDir));
	else
		console.print(format(bbs.text(bbs.text.NFilesListed), result));
}

function view_file_info(mode, filespec)
{
	var str=filespec || bbs.get_filespec();
	if(!str)
		return;
	if(!bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, mode) && str != '*') {
		var s=0;
		console.putmsg(bbs.text(bbs.text.SearchingAllDirs));
		for(var i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
			if(i!=bbs.curdir &&
					(s=bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[i].number, str, mode))!=0) {
				if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
					return;
				}
			}
			if(console.aborted)
				return;
		}
		console.putmsg(bbs.text(bbs.text.SearchingAllLibs));
		for(var i=0; i<file_area.lib_list.length; i++) {
			if(i==bbs.curlib)
				continue;
			for(var j=0; j<file_area.lib_list[i].dir_list.length; j++) {
				if((s=bbs.list_file_info(file_area.lib_list[i].dir_list[j].number, str, mode))!=0) {
					if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
						return;
					}
				}
				if(console.aborted)
					return;
			}
		}
	}
}

function view_files(filespec)
{
	var str=filespec || bbs.get_filespec();
	if(!str)
		return;
	if(!bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FL_VIEW) && str != '*') {
		console.putmsg(bbs.text(bbs.text.SearchingAllDirs));
		for(var i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
			if(i==bbs.curdir)
				continue;
			if(bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FL_VIEW))
				break;
			if(console.aborted)
				return;
		}
		if(i<file_area.lib_list[bbs.curlib].dir_list.length)
			return;
		console.putmsg(bbs.text(bbs.text.SearchingAllLibs));
		for(var i=0; i<file_area.lib_list.length; i++) {
			if(i==bbs.curlib)
				continue;
			for(var j=0; j<file_area.lib_list[i].dir_list.length; j++) {
				if(bbs.list_files(file_area.lib_list[i].dir_list[j].number, str, FL_VIEW))
					return;
				if(console.aborted)
					return;
			}
		}
	}
}

function sub_up()
{
	if(bbs.cursub == msg_area.grp_list[bbs.curgrp].sub_list.length - 1)
		bbs.cursub = 0;
	else
		bbs.cursub++;
}

function sub_down()
{
	if(bbs.cursub == 0)
		bbs.cursub = msg_area.grp_list[bbs.curgrp].sub_list.length - 1;
	else
		bbs.cursub--;
}

function grp_up()
{
	if(bbs.curgrp == msg_area.grp_list.length - 1)
		bbs.curgrp = 0;
	else
		bbs.curgrp++;
}

function grp_down()
{
	if(bbs.curgrp == 0)
		bbs.curgrp = msg_area.grp_list.length - 1;
	else
		bbs.curgrp--;
}

function dir_up()
{
	if(bbs.curdir == file_area.lib_list[bbs.curlib].dir_list.length - 1)
		bbs.curdir = 0;
	else
		bbs.curdir++;
}

function dir_down()
{
	if(bbs.curdir == 0)
		bbs.curdir = file_area.lib_list[bbs.curlib].dir_list.length - 1;
	else
		bbs.curdir--;
}

function lib_up()
{
	if(bbs.curlib == file_area.lib_list.length - 1)
		bbs.curlib = 0;
	else
		bbs.curlib++;
}

function lib_down()
{
	if(bbs.curlib == 0)
		bbs.curlib = file_area.lib_list.length - 1;
	else
		bbs.curlib--;
}

function logoff(fast)
{
	if(bbs.batch_dnload_total) {
		var prompt = bbs.text(bbs.text.DownloadBatchQ);
		if(prompt.length) {
			if(console.yesno(prompt)) {
				bbs.batch_download();
				return false; // hang-up is handled in bbs.batch_download()
			}
			if(!console.noyes(bbs.text(bbs.text.ClearDownloadQueueQ)))
				if(bbs.batch_clear(/* upload_queue */false))
					console.putmsg(bbs.text(bbs.text.DownloadQueueCleared));
				else
					alert(gettext("Failed to clear batch download queue!"));
		}
	}
	if(fast) {
		bbs.hangup();
		return true;
	}
	return bbs.logoff(/* prompt: */true);
}

function upload_file()
{
	bbs.menu("upload", P_NOERROR);
	var i=0xffff;	/* INVALID_DIR */
	if(usrlibs) {
		i = file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number;
		if(file_area.upload_dir != undefined
			&& !file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload)
			i = file_area.upload_dir.number;
	}
	else {
		if(file_area.upload_dir != undefined)
			i = file_area.upload_dir.number;
	}
	console.newline();
	console.print(bbs.text(bbs.text.Filename));
	var fname = console.getstr(file_area.max_filename_length, K_TRIM);
	if(fname)
		bbs.upload_file(i, fname);
	else if(!console.aborted
		&& (file_area.upload_dir !== undefined || bbs.batch_upload_total)
		&& confirm("\r\n" + gettext("Start batch upload")))
		bbs.batch_upload();
}

function upload_user_file()
{
	if(file_area.user_dir == undefined)
		console.print(bbs.text(bbs.text.NoUserDir));
	else
		bbs.upload_file(file_area.user_dir.number);
}

function upload_sysop_file()
{
	if(file_area.sysop_dir == undefined)
		console.print(bbs.text(bbs.text.NoSysopDir));
	else
		bbs.upload_file(file_area.sysop_dir.number);
}

function download_files()
{
	if(bbs.batch_dnload_total && console.yesno(bbs.text(bbs.text.DownloadBatchQ))) {
		bbs.batch_download();
		return;
	}

	view_file_info(FI_DOWNLOAD);
}

function download_user_files()
{
	if(file_area.user_dir == undefined)
		console.print(bbs.text(bbs.text.NoUserDir));
	else {
		if(!bbs.list_file_info(file_area.user_dir.number, FI_USERXFER))
			console.print(bbs.text(bbs.text.NoFilesForYou));
	}
}

// From email_sec.js
function send_email(name, wm_mode)
{
	if(!name) {
		console.putmsg(bbs.text(bbs.text.Email));
		name = console.getstr(40, K_TRIM);
		if(!name)
			return false;
	}
	if(name.indexOf('@') > 0)
		return bbs.netmail(name, wm_mode);
	var number = bbs.finduser(name);
	if(console.aborted)
		return false;
	if(!number)
		number = system.matchuser(name);
	if(!number && (msg_area.settings&MM_REALNAME))
		number = system.matchuserdata(U_NAME, name);
	if(number)
		return bbs.email(number, wm_mode || WM_NONE);
	console.putmsg(bbs.text(bbs.text.UnknownUser));
	return false;
}

// From email_sec.js
var NetmailAddressHistoryLength = 10;
function send_netmail(addr, wm_mode)
{
	var userprops = bbs.mods.userprops || load(bbs.mods.userprops = {}, "userprops.js");
	var netmail = msg_area.fido_netmail_settings | msg_area.inet_netmail_settings;
	const ini_section = "netmail sent";
	console.crlf();
	wm_mode = wm_mode || WM_NONE;
	if(!(wm_mode & WM_FILE) && (netmail&NMAIL_FILE) && !console.noyes(gettext("Attach a file")))
		wm_mode |= WM_FILE;
	if(console.aborted)
		return false;
	if(!addr) {
		console.putmsg(bbs.text(bbs.text.EnterNetMailAddress));
		var addr_list = userprops.get(ini_section, "address", []) || [];
		addr = console.getstr(256, K_LINE | K_TRIM, addr_list);
		if(!addr || console.aborted)
			return false;
	}
	if(bbs.netmail(addr.split(','), wm_mode)) {
		var addr_idx = addr_list.indexOf(addr);
		if(addr_idx >= 0)
			addr_list.splice(addr_idx, 1);
		addr_list.unshift(addr);
		if(addr_list.length > NetmailAddressHistoryLength)
			addr_list.length = NetmailAddressHistoryLength;
		userprops.set(ini_section, "address", addr_list);
		userprops.set(ini_section, "localtime", new Date().toString());
		return true;
	}
	return false;
}

function send_feedback(wm_mode)
{
	return bbs.email(/* user # */1, wm_mode || WM_NONE, bbs.text(bbs.text.ReFeedback));
}

function page_sysop()
{
	if(!bbs.page_sysop()
		&& !deny(format(bbs.text(bbs.text.ChatWithGuruInsteadQ), system.guru || "The Guru")))
		bbs.page_guru();
}

// From default.js
function menu_loop()
{
	var last_str_cmd = "";
	var last_action = menu.node_action;	// No "enter" event for the initial menu

	// The menu-display/command-prompt loop
	while(bbs.online && !js.terminated) {
		if(menu.node_action !== last_action) {
			last_action = menu.node_action;
			var key = section_key[menu.node_action];
			if(key) {
				if(options[key + "_sound"])
					console.play_sound(options[key + "_sound"]);
				if(options[key + "_music"])
					console.play_music(options[key + "_music"]);
				if(options[key + "_enter"])
					eval(options[key + "_enter"]);
			}
		}
		if(!(user.settings & USER_EXPERT)) {
			if (menu.cls)
				console.clear();
			bbs.menu(menu.file);
		}
		if (menu.node_action !== undefined)
			bbs.node_action = menu.node_action;
		bbs.nodesync();
		eval(menu.eval);
		console.newline();
		console.aborted = false;
		console.putmsg(menu.prompt, P_SAVEATR);
		var cmd = console.getkey(K_UPPER);
		if(cmd > ' ')
			console.print(cmd);
		if(cmd == ';') {
			cmd = console.getstr(100, K_LINEWRAP);
			if(cmd == '!')
				cmd = last_str_cmd;
			load({}, "str_cmds.js", cmd);
			last_str_cmd = cmd;
			continue;
		}
		if(cmd == '/') {
			cmd = console.getkey(K_UPPER);
			console.print(cmd);
			if(cmd >= '1' && cmd <= '9' && typeof menu.slash_num_input == "function") {
				menu.slash_num_input(cmd);
				continue;
			}
			cmd = '/' + cmd;
		}
		if(cmd >= '1' && cmd <= '9' && typeof menu.num_input == "function") {
			menu.num_input(cmd);
			continue;
		}
		if(cmd > ' ') {
			bbs.log_key(cmd, /* comma: */true);
		}
		if(menu.nav[cmd]) {
			if(menu.nav[cmd].eval)
				eval(menu.nav[cmd].eval);
			continue;
		}
		console.newline();
		console.line_counter = 0;
		if(cmd == help_key) {
			if(user.settings & USER_EXPERT) {
				if (menu.cls)
					console.clear();
				bbs.menu(menu.file);
			}
			continue;
		}
		var menu_cmd = menu.command[cmd];
		if(!menu_cmd) {
			console.print("\r\n\x01c\x01h" + gettext("Unrecognized command."));
			if(user.settings & USER_EXPERT)
				console.print("  " + gettext("Hit") + " '\x01i" + help_key + "\x01n\x01c\x01h' " + gettext("for a menu."));
			console.print("  " + gettext("Type \x01y;help\x01c for more commands."));
			console.newline();
			continue;
		}
		if(menu_cmd.ars === undefined || bbs.compare_ars(menu_cmd.ars)) {
			if(menu_cmd.msg)
				console.print(menu_cmd.msg);
			if(menu_cmd.eval)
				eval(menu_cmd.eval);
			if(menu_cmd.exec) {
				var script = system.mods_dir + menu_cmd.exec;
				if(!file_exists(script))
					script = system.exec_dir + menu_cmd.exec;
				if(menu_cmd.args)
					js.exec.apply(null, [script, {}].concat(menu_cmd.args));
				else
					js.exec(script, {});
			}
		}
		else if(menu_cmd.err)
			console.print(menu_cmd.err);
	}
}

this;
