// Library for writing command shells
// portions derived from classic_shell.js, default.src, and exec*.cpp

// @format.tab-size 4

"use strict";

require("sbbsdefs.js", "USER_EXPERT");
require("gettext.js", "gettext");

// Build list of current subs/dirs in each group/library
// This hack is required because the 'bbs' object doesn't expose the current
// sub/dir for any group/library except the current
var curgrp = bbs.curgrp;
var curlib = bbs.curlib;
var cursub = [];
var curdir = [];
var usrsubs = [];
var usrdirs = [];
var usrgrps = msg_area.grp_list.length;
var usrlibs = file_area.lib_list.length;
for(var i = 0; i < usrgrps; ++i) {
	bbs.curgrp = i;
	cursub[i] = bbs.cursub;
	usrsubs[i] = msg_area.grp_list[i].sub_list.length;
}
for(var i = 0; i < usrlibs; ++i) {
	bbs.curlib = i;
	curdir[i] = bbs.curdir;
	usrdirs[i] = file_area.lib_list[i].dir_list.length;
}
bbs.curgrp = curgrp;
bbs.curlib = curlib;

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

// List Message Groups
function show_grps()
{
	if(msg_area.grp_list.length < 1)
		return;
	if(bbs.menu("grps", P_NOERROR))
		return;
	console.print(bbs.text(bbs.text.GrpLstHdr));
	for(var i=0; i < msg_area.grp_list.length && !console.aborted; i++) {
		if(i == bbs.curgrp)
			console.print('*');
		else console.print(' ');
		if(i<9) console.print(' ');
		console.add_hotspot(i+1);
		console.print(format(bbs.text(bbs.text.GrpLstFmt), i+1
			,msg_area.grp_list[i].description, "", msg_area.grp_list[i].sub_list.length));
	}
}

function show_subs(grp)
{
	if(msg_area.grp_list.length < 1)
		return;
	if(bbs.menu("subs" + msg_area.grp_list[grp].number + 1, P_NOERROR))
		return;
	console.newline();
	console.print(format(bbs.text(bbs.text.SubLstHdr), msg_area.grp_list[grp].description));
	for(var i=0; i < usrsubs[grp] && !console.aborted; ++i) {
		if(i==cursub[grp]) console.print('*');
		else console.print(' ');
		var str = format(bbs.text(bbs.text.SubLstFmt),i+1
			,msg_area.grp_list[grp].sub_list[i].description, ""
			,msg_area.grp_list[grp].sub_list[i].posts);
		if(i<9) console.print(' ');
		if(i<99) console.print(' ');
		console.add_hotspot(i+1);
		console.print(str);
	}
}

function select_msg_area()
{
	if(usrgrps < 1)
		return;
	while(bbs.online) {
		var j=0;
		if(usrgrps > 1) {
			show_grps();
			console.mnemonics(format(bbs.text(bbs.text.JoinWhichGrp), bbs.curgrp + 1));
			j=console.getnum(usrgrps);
			console.clear_hotspots();
			if(j==-1)
				return;
			if(!j)
				j=bbs.curgrp;
			else
				j--;
		}
		show_subs(j);
		console.mnemonics(format(bbs.text(bbs.text.JoinWhichSub), cursub[j]+1));
		i=console.getnum(usrsubs[j]);
		console.clear_hotspots();
		if(i==-1) {
			if(usrgrps==1)
				return;
			continue;
		}
		if(!i)
			i=cursub[j];
		else
			i--;
		bbs.curgrp=j;
		bbs.cursub=i;
		return;
	}
	return;
}

// List File Libraries
function show_libs()
{
	if(file_area.lib_list.length < 1)
		return;
	if(bbs.menu("libs", P_NOERROR))
		return;
	console.print(bbs.text(bbs.text.LibLstHdr));
	for(var i=0; i < file_area.lib_list.length && !console.aborted; i++) {
		if(i == bbs.curlib)
			console.print('*');
		else console.print(' ');
		if(i<9) console.print(' ');
		console.add_hotspot(i+1);
		console.print(format(bbs.text(bbs.text.LibLstFmt), i+1
			,file_area.lib_list[i].description, "", file_area.lib_list[i].dir_list.length));
	}
}

function show_dirs(lib)
{
	if(file_area.lib_list.length < 1)
		return;
	if(bbs.menu("dirs" + file_area.lib_list[lib].number + 1, P_NOERROR))
		return;
	console.newline();
	console.print(format(bbs.text(bbs.text.DirLstHdr), file_area.lib_list[lib].description));
	for(var i=0; i < usrdirs[lib] && !console.aborted; ++i) {
		if(i==curdir[lib]) console.print('*');
		else console.print(' ');
		var str = format(bbs.text(bbs.text.DirLstFmt),i+1
			,file_area.lib_list[lib].dir_list[i].description, ""
			,file_area.lib_list[lib].dir_list[i].files);
		if(i<9) console.print(' ');
		if(i<99) console.print(' ');
		console.add_hotspot(i+1);
		console.print(str);
	}
}

function select_file_area()
{
	var usrlibs = file_area.lib_list.length;
	if(usrlibs < 1)
		return;
	while(bbs.online) {
		var j=0;
		if(usrlibs > 1) {
			show_libs();
			console.mnemonics(format(bbs.text(bbs.text.JoinWhichGrp), bbs.curlib + 1));
			j=console.getnum(usrlibs);
			console.clear_hotspots();
			if(j==-1)
				return;
			if(!j)
				j=bbs.curlib;
			else
				j--;
		}
		show_dirs(j);
		console.mnemonics(format(bbs.text(bbs.text.JoinWhichDir), curdir[j]+1));
		i=console.getnum(usrdirs[j]);
		console.clear_hotspots();
		if(i==-1) {
			if(usrlibs==1)
				return;
			continue;
		}
		if(!i)
			i=curdir[j];
		else
			i--;
		bbs.curlib=j;
		bbs.curdir=i;
		return;
	}
	return;
}

function main_info()
{
	while(bbs.online && !js.terminate) {
		if(!(user.settings & USER_EXPERT))
			bbs.menu("maininfo");
		bbs.nodesync();
		console.print("\r\n\x01y\x01h"+ gettext("Info") + ": \x01n");
		var key = console.getkeys("?QISVY\r");
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
	if(bbs.file_cmds > 0)
		return;
	if(!(user.settings & USER_ASK_NSCAN))
		return;
	console.newline(2);
	if(console.yesno("Search all libraries for new files"))
		bbs.scan_dirs(FL_ULTIME, /* all */true);
}

function file_info()
{
	var key;

	while(1) {
		if(!(user.settings & USER_EXPERT))
			bbs.menu("xferinfo");
		bbs.nodesync();
		console.print("\r\n\x01y\x01h" + gettext("Info") + ": \x01n");
		key=console.getkeys("?TYDUQ\r");
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
	switch(console.getkeys("LSY\r" + console.all_key)) {
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

function view_file_info(mode)
{
	var str=bbs.get_filespec();
	if(!str)
		return;
	if(!bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, mode)) {
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

function view_files()
{
	var str=bbs.get_filespec();
	if(!str)
		return;
	if(!bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FL_VIEW)) {
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
	cursub[bbs.curgrp] = bbs.cursub;
}

function sub_down()
{
	if(bbs.cursub == 0)
		bbs.cursub = msg_area.grp_list[bbs.curgrp].sub_list.length - 1;
	else
		bbs.cursub--;
	cursub[bbs.curgrp] = bbs.cursub;
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
	curdir[bbs.curdir] = bbs.curdir;
}

function dir_down()
{
	if(bbs.curdir == 0)
		bbs.curdir = file_area.lib_list[bbs.curlib].dir_list.length - 1;
	else
		bbs.curdir--;
	curdir[bbs.curdir] = bbs.curdir;
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
				return; // hang-up is handled in bbs.batch_download()
			}
			if(!console.noyes(bbs.text(bbs.text.ClearDownloadQueueQ)))
				if(bbs.batch_clear(/* upload_queue */false))
					console.putmsg(bbs.text(bbs.text.DownloadQueueCleared));
				else
					alert("Failed to clear batch download queue!");
		}
	}
	if(fast)
		bbs.hangup();
	else
		bbs.logoff(/* prompt: */true);
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
		&& confirm("\r\nStart batch upload"))
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

this;
