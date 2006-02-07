// lbshell.js

// Lightbar Command Shell for Synchronet Version 4.00a+

// $Id$

// @format.tab-size 4, @format.use-tabs true

//##############################################################################
//#
//# Tips:
//#
//#	Tabstops should be set to 4 to view/edit this file
//#	If your editor does not support control characters,
//#		use \1 for Ctrl-A codes
//#
//################################# Begins Here #################################

load("sbbsdefs.js");
load("lightbar.js");
bbs.command_str='';	// Clear STR (Contains the EXEC for default.js)
load("str_cmds.js");
var str;
const LBShell_Attr=0x37;
var bars80="\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4";
var spaces80="                                                                               ";

function Mainbar()
{
	/* ToDo: They all need this... feels like a bug to ME */
	this.items=new Array();
	this.direction=1;
	this.xpos=2;
	this.ypos=1;
	this.hotkeys=KEY_DOWN+";";
	this.add("|File","F",undefined,undefined,undefined,user.compare_ars("REST T"));
	this.add("|Messages","M");
	this.add("|Email","E",undefined,undefined,undefined,user.compare_ars("REST SE"));
	this.add("|Chat","C",undefined,undefined,undefined,user.compare_ars("REST C"));
	this.add("|Settings","S");
	this.add("E|xternals","x",undefined,undefined,undefined,user.compare_ars("REST X"));
	this.add("|View","V");
	this.add("|Goodbye","G");
	this.add("Commands",";");
}
Mainbar.prototype=new Lightbar;
var mainbar=new Mainbar;

function top_bar(width)
{
	return("\xda"+bars80.substr(0,width)+"\xbf");
}

function bottom_bar(width)
{
	return("\xc0"+bars80.substr(0,width)+"\xd9");
}

function format_opt(str, width, expand)
{
	var opt=str;
	if(expand) {
		var cleaned=opt;
		cleaned=cleaned.replace(/\|/g,'');
		opt+=spaces80.substr(0,width-cleaned.length-2);
		opt+=' >';
	}
	return(opt);
}

function Filemenu()
{
	this.items=new Array();
	// Width of longest line with no dynamic variables
	var width=0;
	var scantime=system.datestr(bbs.new_file_time);
	// Expand for scan time line.
	if(width < 27+scantime.length)
		width=27+scantime.length;
	if(width < 19+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length)
		width=19+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length;
	this.xpos=1;
	this.ypos=2;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	this.add(top_bar(width),undefined,undefined,"","");
	this.add(
		 format_opt("|Change Current Dir",width,false)
		,"C",width
	);
	this.add(
		 format_opt("|List Current Dir ("+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name+")",width,false)
		,"L",width
	);
	this.add(
		 format_opt("Scan for |New Files since "+scantime,width,true)
		,"N",width
	);
	this.add(
		 format_opt("Search for |Filenames",width,true)
		,"F",width
	);
	this.add(
		 format_opt("Search for |Text in Descriptions",width,true)
		,"T",width
	);
	this.add(
		 format_opt("|Download file(s)",width,true)
		,"D",width,undefined,undefined
		,user.compare_ars("REST D")
			|| (!file_area.lib_list[bbs.curdir].dir_list[bbs.curdir].can_download)
	);
	this.add(
		 format_opt("|Upload file(s)",width,true)
		,"U",width,undefined,undefined
		,user.compare_ars("REST U")
			|| ((!file_area.lib_list[bbs.curdir].dir_list[bbs.curdir].can_upload)
			&& file_area.upload_dir==undefined)
	);
	this.add(
		 format_opt("|Remove/Edit Files",width,false)
		,"R",width
	);
	this.add(
		 format_opt("View/Edit |Batch Queue",width,false)
		,"B",width,undefined,undefined
		// Disabled if you can't upload or download.
		// Disabled if no upload dir and no batch queue
		,(user.compare_ars("REST U AND REST D"))
			|| (bbs.batch_upload_total <= 0  
				&& bbs.batch_dnload_total <= 0 
				&& file_area.upload_dir==undefined
			)
	);
	this.add(
		 format_opt("|View",width,true)
		,"V",width
	);
	this.add(
		 format_opt("|Settings",width,true)
		,"S",width
	);
	this.add(bottom_bar(width),undefined,undefined,"","");
}
Filemenu.prototype=new Lightbar;

function Filedirmenu(x, y, changenewscan)
{
	this.items=new Array();
	var width=changenewscan?20:0;

	if(width<18+file_area.lib_list[bbs.curlib].name.length)
		width=18+file_area.lib_list[bbs.curlib].name.length;
	if(width<20+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length)
		width=20+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length;
	this.xpos=x;
	this.ypos=y;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	this.add(top_bar(width),undefined,undefined,"","");
	this.add("|All File Areas","A",width);
	this.add("Current |Library ("+file_area.lib_list[bbs.curlib].name+")","L",width);
	this.add("Current |Directory ("+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name+")","D",width);
	if(changenewscan)
		this.add("Change New Scan |Date","N",width);
	this.add(bottom_bar(width),undefined,undefined,"","");
}
Filedirmenu.prototype=new Lightbar;

function Fileinfo()
{
	this.items=new Array();
	this.xpos=22;
	this.ypos=4;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	this.add("File |Transfer Policy","T",32);
	this.add("Information on Current |Directory","D",32);
	this.add("|Users With Access to Current Dir","U",32);
	this.add("|Your File Transfer Statistics","Y",32);
	this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
}
Fileinfo.prototype=new Lightbar;
var fileinfo=new Fileinfo;

function Settingsmenu()
{
	var width=18;

	this.items=new Array();
	this.xpos=30;
	this.ypos=2;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	this.add(top_bar(width),undefined,undefined,"","");
	this.add("|User Configuration","U",width);
	this.add("Minute |Bank","B",width);
	this.add(bottom_bar(width),undefined,undefined,"","");
}
Settingsmenu.prototype=new Lightbar;

function Xfercfgmenu()
{
	this.items=new Array();
	this.xpos=33;
	this.ypos=6;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	this.add("|Set New Scan Time","S",28);
	this.add("Toggle |Batch Flag","B",28);
	this.add("Toggle |Extended Descriptions","E",28);
	this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
}
Xfercfgmenu.prototype=new Lightbar;
var xfercfgmenu=new Xfercfgmenu;

function Emailmenu()
{
	var width=24;

	this.items=new Array();
	this.xpos=17;
	this.ypos=2;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	this.add(top_bar(width),undefined,undefined,"","");
	this.add(format_opt("|Send Mail",width,true),"S",width);
	this.add("|Read Inbox","R",width);
	this.add("Read Sent |Messages","M",width,undefined,undefined,user.compare_ars("REST K"));
	this.add(bottom_bar(width),undefined,undefined,"","");
}
Emailmenu.prototype=new Lightbar;
var emailmenu=new Emailmenu;

function Messagemenu()
{
	var width=31;

	if(width<8+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length)
		width=8+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length
	this.items=new Array();
	this.xpos=7;
		this.ypos=2;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	this.add(top_bar(width),undefined,undefined,"","");
	this.add("|Change Current Sub","C",width);
	this.add("|Read "+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name,"R",width);
	this.add(
		 format_opt("Scan For |New Messages",width,true)
		,"N",width
	);
	this.add(
		 format_opt("Scan For Messages To |You",width,true)
		,"Y",width
	);
	this.add(
		 format_opt("Search For |Text in Messages",width,true)
		,"T",width
	);
	this.add("|Post In "+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name,"P",width,undefined,undefined,user.compare_ars("REST P"));
	if(user.compare_ars("REST N") && (msg_area.grp_list[bbs.curgrp].sub_list[bbs.crusub] & (SUB_QNET|SUB_PNET|SUB_FIDO)))
		this.items[6].disabed=true;
	this.add("Read/Post |Auto-Message","A",width);
	this.add("|QWK Packet Transfer Menu","Q",width);
	this.add("|View Information on Current Sub","V",width);
	this.add(bottom_bar(width),undefined,undefined,"","");
}
Messagemenu.prototype=new Lightbar;

function Chatmenu()
{
	var width=27;

	this.items=new Array();
	this.xpos=24;
	this.ypos=2;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	this.add(top_bar(width),undefined,undefined,"","");
	this.add("|Multinode Chat","M",width);
	this.add("|Private Node to Node Chat","P",width);
	this.add("|Chat With The SysOp","C",width);
	this.add("|Talk With The System Guru","T",width);
	this.add("|Finger A Remote User/System","F",width);
	this.add("I|RC Chat","R",width);
	this.add("InterBBS |Instant Messages","I",width);
	this.add(format_opt("|Settings",width,true),"S",width);
	this.add(bottom_bar(width),undefined,undefined,"","");
}
Chatmenu.prototype=new Lightbar;

// Generate menus of available xtrn sections.
function Xtrnsecs()
{
	this.items=new Array();
	this.xpos=40;
	this.ypos=2;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	var xtrnsecwidth=0;
	var j;
	for(j=0; j<xtrn_area.sec_list.length && j<console.screen_rows-2; j++) {
		if(xtrn_area.sec_list[j].name.length > xtrnsecwidth)
			xtrnsecwidth=xtrn_area.sec_list[j].name.length;
	}
	xtrnsecwidth += 4;
	if(xtrnsecwidth>37)
		xtrnsecwidth=37;
	this.add("\xda"+bars80.substr(0,xtrnsecwidth)+"\xbf",undefined,undefined,"","");
	for(j=0; j<xtrn_area.sec_list.length; j++)
		this.add("<-- "+xtrn_area.sec_list[j].name,j.toString(),xtrnsecwidth);
	this.add("\xc0"+bars80.substr(0,xtrnsecwidth)+"\xd9",undefined,undefined,"","");
}
Xtrnsecs.prototype=new Lightbar;
var xtrnsec=new Xtrnsecs;

function Xtrnsec(sec)
{
	this.items=new Array();
	var j=0;

	xtrnsecprogwidth=0;
	this.hotkeys=KEY_RIGHT+KEY_LEFT+"\b\x7f\x1b";
	for(j=0; j<xtrn_area.sec_list[sec].prog_list.length; j++) {
		if(xtrn_area.sec_list[sec].prog_list[j].name.length > xtrnsecprogwidth)
			xtrnsecprogwidth=xtrn_area.sec_list[sec].prog_list[j].name.length;
	}
	if(xtrnsecprogwidth>37)
		xtrnsecprogwidth=37;
	if(xtrn_area.sec_list[sec].prog_list.length+3+sec <= console.screen_rows)
		this.ypos=sec+2;
	else
		this.ypos=console.screen_rows-j-1;
	this.xpos=40-xtrnsecprogwidth-2;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.add("\xda"+bars80.substr(0,xtrnsecprogwidth)+"\xbf",undefined,undefined,"","");
	for(j=0; j<xtrn_area.sec_list[sec].prog_list.length && j<console.screen_rows-3; j++)
		this.add(xtrn_area.sec_list[sec].prog_list[j].name,j.toString(),xtrnsecprogwidth);
	this.add("\xc0"+bars80.substr(0,xtrnsecprogwidth)+"\xd9",undefined,undefined,"","");
}
Xtrnsec.prototype=new Lightbar;
var xtrnsecs=new Array();
var j;
for(j=0; j<xtrn_area.sec_list.length; j++)
	xtrnsecs[j]=new Xtrnsec(j);

function Infomenu()
{
	this.items=new Array();
	this.xpos=51;
	this.ypos=2;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	this.add("System |Information","I",25);
	this.add("Synchronet |Version Info","V",25);
	this.add("Info on Current |Sub-Board","S",25);
	this.add("|Your Statistics","Y",25);
	this.add("<-- |User Lists","U",25);
	this.add("|Text Files","T",25);
	this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
}
Infomenu.prototype=new Lightbar;
var infomenu=new Infomenu;

function Userlists()
{
	this.items=new Array();
	this.xpos=37;
	this.ypos=6;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_RIGHT+KEY_LEFT+"\b\x7f\x1b";
	this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	this.add("|Logons Today","L",12);
	this.add("|Sub-Board","S",12);
	this.add("|All","A",12);
	this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
}
Userlists.prototype=new Lightbar;
var userlists=new Userlists;

draw_main(true);
var next_key='';
while(1) {
	var done=0;
	var key=next_key;
	var extra_select=false;
	next_key='';
	draw_main(false);
	if(key=='')
		key=mainbar.getval()
	extra_select=false;
	if(key==KEY_DOWN) {
		key=mainbar.items[mainbar.current].retval;
		extra_select=true;
	}
	switch(key) {
		case ';':
			mainbar.current=8;
			mainbar.draw();
			console.gotoxy(1,2);
			console.attributes=9;
			console.write("Command (? For Help): ");
			console.attributes=7;
			if(!console.aborted) {
				var str=console.getstr("",40,K_EDIT);
				clear_screen();
				if(str=='?') {
					if(!user.compare_ars("SYSOP"))
						str='HELP';
				}
				if(str=='?') {
					bbs.menu("sysmain");
					console.pause();
					bbs.menu("sysxfer");
				}
				else
					str_cmds(str);
				console.pause();
				draw_main(true);
			}
			else
				draw_main(false);
			break;
		case 'F':
			show_filemenu();
			break;
		case 'S':
			show_settingsmenu();
			break;
		case 'E':
			show_emailmenu();
			break;
		case 'M':
			show_messagemenu();
			break;
		case 'C':
			show_chatmenu();
			break;
		case 'x':
			var curr_xtrnsec=0;
			var x_sec;
			var x_prog;
			done=false;
			while(!done) {
				x_sec=xtrnsec.getval();
				if(x_sec==KEY_LEFT)
					x_sec=xtrnsec.current-1;
				if(x_sec==KEY_RIGHT) {
					main_right();
					break;
				}
				if(x_sec=='\b' || x_sec=='\x7f' || x_sec=='\x1b')
					break;
				curr_xtrnsec=parseInt(x_sec);
				while(1) {
					x_prog=xtrnsecs[curr_xtrnsec].getval();
					if(x_prog==KEY_LEFT) {
						main_left();
						done=1;
						break;
					}
					if(x_prog==KEY_RIGHT)
						break;
					if(x_prog=='\b' || x_prog=='\x7f' || x_prog=='\x1b')
						break;
					clear_screen();
					bbs.exec_xtrn(xtrn_area.sec_list[curr_xtrnsec].prog_list[parseInt(x_prog)].number);
					draw_main(true);
					xtrnsec.draw();
				}
				draw_main(false);
			}
			draw_main(false);
			break;
		case 'V':
			infoloop: while(1) {
				switch(infomenu.getval()) {
					case 'I':
						clear_screen();
						bbs.sys_info();
						console.pause();
						draw_main(true);
						break;
					case 'V':
						clear_screen();
						bbs.ver();
						console.pause();
						draw_main(true);
						break;
					case 'S':
						clear_screen();
						bbs.sub_info();
						console.pause();
						draw_main(true);
						break;
					case 'Y':
						clear_screen();
						bbs.user_info();
						console.pause();
						draw_main(true);
						break;
					case KEY_LEFT:
						if(infomenu.items[infomenu.current].retval!='U') {
							main_left();
							done=1;
							break infoloop;
						}
						// Fall-through
					case 'U':
						userlistloop: while(1) {
							switch(userlists.getval()) {
								case KEY_LEFT:
									main_left();
									break infoloop;
								case KEY_RIGHT:
								case '\b':
								case '\x7f':
								case '\x1b':
									break userlistloop;
								case 'L':
									clear_screen();
									bbs.list_logons();
									console.pause();
									draw_main(true);
									infomenu.draw();
									break;
								case 'S':
									clear_screen();
									bbs.list_users(UL_SUB);
									console.pause();
									draw_main(true);
									infomenu.draw();
									break;
								case 'A':
									clear_screen();
									bbs.list_users(UL_ALL);
									console.pause();
									draw_main(true);
									infomenu.draw();
									break;
							}
						}
						draw_main(false);
						break;
					case 'T':
						clear_screen();
						bbs.text_sec();
						draw_main(true);
						break infoloop;
					case KEY_RIGHT:
						main_right();
						done=1;
						break infoloop;
					case '\b':
					case '\x7f':
					case '\x1b':
						break infoloop;
				}
			}
			draw_main(false);
			break;
		case 'G':
			if(!extra_select)
				exit(1);
	}
}

function todo_getfiles(lib, dir)
{
	var path=format("%s%s.ixb", file_area.lib_list[lib].dir_list[dir].data_dir, file_area.lib_list[lib].dir_list[dir].code);
	return(file_size(path)/22);	/* F_IXBSIZE */
}

function clear_screen()
{
	/*
	 * Called whenever a command needs to exit the menu for user interaction.
	 *
	 * If you'd like a header before non-menu stuff, this is the place to put
	 * it.
	 */
	console.attributes=7;
	console.clear();
}

function draw_main(topline)
{
	/*
	 * Called to re-display the main menu.
	 * topline is false when the top line doesn't need redrawing.
	 */
	if(topline) {
		console.gotoxy(1,1);
		console.attributes=0x17;
		console.cleartoeol();
	}
	mainbar.draw();
	var i;
	console.gotoxy(1,1);
	console.attributes=LBShell_Attr;
	for(i=1;i<console.screen_rows-9;i++) {
		console.line_counter=0;
		console.write("\n");
		console.cleartoeol();
	}
	/*
	 * If you want a background ANSI or something for the menus,
	 * this is the place to draw it from.
	 */
	console.gotoxy(1,console.screen_rows-8);
	console.line_counter=0;
	console.putmsg("\x01n \x01n\x01h\xdc\xdc\xdc\xdc \xdb \xdc\xdc  \xdc\xdc\xde\xdb \xdc \xdc\xdc\xdc\xdc \xdc\xdc \xdc\xdc  \xdc\xdc\xdc\xdc\xdc\xdc\xdc \x01n\x01b\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc ");
	console.gotoxy(1,console.screen_rows-7);
	console.putmsg("\x01h\x01c\x016\xdf\x01n\x01c\xdc\xdc \x01h\x016\xdf\x01n \x01h\x016\xdf\x01n \x01n\x01c\xdc \x01h\x016\xdf\x01n \x01h\x016\xdf\x01n \x01c\xdc\x01h\x016\xdf\x01n\x01c\xdc\x01h\x016\xdf\x01n\x01c\xdc\xdc\xdc\x01h\xdf \x016\xdf\x01n \x01h\x016\xdf\x01n \x01c\xdc \x01h\x016\xdf\x01n \x01h\x016\xdf\x01n\x01c\xdc\xdc \xdc\x01bgj \xdb\x01w\x014@TIME-L@ @DATE@  \x01y\x01h@BOARDNAME-L19@ \x01n ");
	console.gotoxy(1,console.screen_rows-6);
	console.putmsg("\x01n  \x01b\x01h\x014\xdf\x01n\x01b\xdd\x01h\xdf\x014\xdf\x010\xdf \x014\xdf\x010 \x014\xdf\x010 \x014\xdf\x010  \x014\xdf\x010 \x014\xdf\x01n\x01b\xdd\x01h\x014\xdf\x010 \x014\xdf\x01n\x01b\xdd\x01h\x014\xdf\x010 \x014\xdf\x010 \x014\xdf\x010 \x014\xdf\x010 \x014\xdf\x010   \x014\xdf\x010   \x014\x01n\x01b\xdb\x01h\x01w\x014Last On\x01k: \x01n\x014@LASTDATEON@  \x01h\x01cNode \x01k\x01n\x01c\x014@NODE-L3@ \x01wUp \x01c@UPTIME-L8@\x01n ");
	console.gotoxy(1,console.screen_rows-5);
	console.putmsg("\x01n\x01b \xdf\xdf  \xdf  \xdf \xdb\xdd\xdf\xdf\xdf\xdf \xdb\xdd  \xdb\xdb\xdf\xdf  \xdf \xdb\xdd\xdf\xdf\xdf \xdf   \xdb\x014\x01h\x01wFirstOn\x01k:\x01n\x014 @SINCE@  \x01h\x01cCalls\x01n\x01c\x014@SERVED-R4@ \x01wof\x01c @TCALLS-L7@\x01n ");
	console.gotoxy(1,console.screen_rows-4);
	console.putmsg("\x01n                                       \x01b\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf ");
	console.line_counter=0;
	console.gotoxy(1,console.screen_rows-3);
	console.attributes=7;
	console.cleartoeol();
	console.putmsg(" \x01n\x01c[\x01h@GN@\x01n\x01c] @GRP@ [\x01h@SN@\x01n\x01c] @SUB@\x01n\r\n");
	console.gotoxy(1,console.screen_rows-2);
	console.cleartoeol();
	console.putmsg(" \x01n\x01c(\x01h@LN@\x01n\x01c) @LIB@ (\x01h@DN@\x01n\x01c) @DIR@\x01n\r\n");
	console.gotoxy(1,console.screen_rows-1);
	console.attributes=LBShell_Attr;
	console.cleartoeol();
	console.gotoxy(1,console.screen_rows);
	console.cleartoeol();
	console.gotoxy(1,1);
}

function main_right()
{
	do {
		mainbar.current++;
		if(mainbar.current==mainbar.items.length)
			mainbar.current=0;
	} while(mainbar.items[mainbar.current].disabled || mainbar.items[mainbar.current].retval==undefined)
	next_key=mainbar.items[mainbar.current].retval;
	if(next_key=='G' || next_key==';')
		next_key='';
}

function main_left()
{
	do {
		if(mainbar.current==0)
			mainbar.current=mainbar.items.length;
		mainbar.current--;
	} while(mainbar.items[mainbar.current].disabled || mainbar.items[mainbar.current].retval==undefined)
	next_key=mainbar.items[mainbar.current].retval;
	if(next_key=='G' || next_key==';')
		next_key='';
}

/*
 * Displays and handles the file menu
 */
function show_filemenu()
{
	var cur=1;
	var nd=false;
	while(1) {
		var filemenu=new Filemenu();
		var ret;
		var i;
		var j;
		filemenu.nodraw=nd;
		filemenu.current=cur;

		ret=filemenu.getval();
		if(ret==KEY_RIGHT) {
			if(filemenu.items[filemenu.current].text.substr(-2,2)==' >')
				ret=filemenu.items[filemenu.current].retval;
		}
		file: switch(ret) {
			case KEY_LEFT:
				main_left();
				return;
			case '\b':
			case '\x7f':
			case '\x1b':
				return;
			case KEY_RIGHT:
				main_right();
				return;
			case 'C':
				clear_screen();
				changedir: do {
					if(!file_area.lib_list.length)
						break changedir;
					while(1) {
						var orig_lib=bbs.curlib;
						i=0;
						j=0;
						if(file_area.lib_list.length>1) {
							if(file_exists(system.text_dir+"menu/libs.*"))
								bbs.menu("libs");
							else {
								console.putmsg(bbs.text(CfgLibLstHdr),P_SAVEATR);
								for(i=0; i<file_area.lib_list.length; i++) {
									if(i==bbs.curlib)
										console.putmsg('*',P_SAVEATR);
									else
										console.putmsg(' ',P_SAVEATR);
									if(i<9)
										console.putmsg(' ',P_SAVEATR);
									if(i<99)
										console.putmsg(' ',P_SAVEATR);
									// We use console.putmsg to expand ^A, @, etc
									console.putmsg(format(bbs.text(CfgLibLstFmt),i+1,file_area.lib_list[i].description),P_SAVEATR);
								}
							}
							console.mnemonics(format(bbs.text(JoinWhichLib),bbs.curlib+1));
							j=console.getnum(file_area.lib_list.length,false);
							if(j<0)
								break changedir;
							if(!j)
								j=bbs.curlib;
							else
								j--;
						}
						bbs.curlib=j;
						if(file_exists(system.text_dir+"menu/dirs"+(bbs.curlib+1)))
							bbs.menu("dirs"+(bbs.curlib+1));
						else {
							 console.clear();
							 console.putmsg(format(bbs.text(DirLstHdr), file_area.lib_list[j].description),P_SAVEATR);
							 for(i=0; i<file_area.lib_list[j].dir_list.length; i++) {
								if(i==bbs.curdir)
									console.putmsg('*',P_SAVEATR);
								else
									console.putmsg(' ',P_SAVEATR);
								if(i<9)
									console.putmsg(' ',P_SAVEATR);
								if(i<99)
									console.putmsg(' ',P_SAVEATR);
								console.putmsg(format(bbs.text(DirLstFmt),i+1, file_area.lib_list[j].dir_list[i].description,"",todo_getfiles(j,i)),P_SAVEATR);
							}
						}
						console.mnemonics(format(bbs.text(JoinWhichDir),bbs.curdir+1));
						i=console.getnum(file_area.lib_list[j].dir_list.length);
						if(i==-1) {
							if(file_area.lib_list.length==1) {
								bbs.curlib=orig_lib;
								break changedir;
							}
							continue;
						}
						if(!i)
							i=bbs.curdir;
						else
							i--;
						bbs.curdir=i;
						break changedir;
					}
				} while(0);
				draw_main(true);
				break;
			case 'L':
				clear_screen();
				bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number);
				console.pause();
				draw_main(true);
				break;
			case 'N':
				var typemenu=new Filedirmenu(filemenu.xpos+filemenu.items[0].text.length, filemenu.current+1, true);
				while(1) {
					switch(typemenu.getval()) {
						case 'A':
							clear_screen();
							console.putmsg("\r\nchNew File Scan (All)\r\n");
							bbs.scan_dirs(FL_ULTIME,true);
							console.pause();
							draw_main(true);
							filemenu.draw();
							break;
						case 'L':
							/* Scan this lib only */
							clear_screen();
							console.putmsg("\r\nchNew File Scan (Lib)\r\n");
							for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++)
								bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[i].number,FL_ULTIME);
							console.pause();
							draw_main(true);
							filemenu.draw();
							break;
						case 'D':
							/* Scan this dir only */
							clear_screen();
							console.putmsg("\r\nchNew File Scan (Dir)\r\n");
							bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number,FL_ULTIME);
							console.pause();
							draw_main(true);
							filemenu.draw();
							break;
						case 'N':
							// ToDo: Don't clear screen here, just do one line
							clear_screen();
							bbs.new_file_time=bbs.get_newscantime(bbs.new_file_time);
							draw_main(true);
							filemenu.draw();
							break;
						case KEY_RIGHT:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							main_right();
							return;
						default:	// Anything else will escape.
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							filemenu.nodraw=true;
							break file;
					}
				}
				break;
			case 'F':
				var typemenu=new Filedirmenu(filemenu.xpos+filemenu.items[0].text.length, filemenu.current+1, false);
				while(1) {
					switch(typemenu.getval()) {
						case 'A':
							clear_screen();
							console.putmsg("\r\nchSearch for Filename(s) (All)\r\n");
							var spec=bbs.get_filespec();
							for(i=0; i<file_area.lib_list.length; i++) {
								for(j=0;j<file_area.lib_list[i].dir_list.length;j++)
									bbs.list_files(file_area.lib_list[i].dir_list[j].number,spec,0);
							}
							console.pause();
							draw_main(true);
							filemenu.draw();
							break;
						case 'L':
							/* Scan this lib only */
							clear_screen();
								console.putmsg("\r\nchSearch for Filename(s) (Lib)\r\n");
							var spec=bbs.get_filespec();
							for(j=0;j<file_area.lib_list[bbs.curlib].dir_list.length;j++)
								bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[j].number,spec,0);
							console.pause();
							draw_main(true);
							filemenu.draw();
							break;
						case 'D':
							/* Scan this dir only */
							clear_screen();
							console.putmsg("\r\nchSearch for Filename(s) (Dir)\r\n");
							var spec=bbs.get_filespec();
							bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number,spec,0);
							console.pause();
							draw_main(true);
							filemenu.draw();
							break;
						case KEY_RIGHT:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							main_right();
							return;
						default:	// Anything else will escape.
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							filemenu.nodraw=true;
							break file;
					}
				}
				break;
			case 'T':
				var typemenu=new Filedirmenu(filemenu.xpos+filemenu.items[0].text.length, filemenu.current+1, false);
				while(1) {
					switch(typemenu.getval()) {
						case 'A':
							clear_screen();
							console.putmsg("\r\nchSearch for Text in Description(s) (All)\r\n");
							console.putmsg(bbs.text(SearchStringPrompt));
							var spec=console.getstr(40,K_LINE|K_UPPER);
							for(i=0; i<file_area.lib_list.length; i++) {
								for(j=0;j<file_area.lib_list[i].dir_list.length;j++)
									bbs.list_files(file_area.lib_list[i].dir_list[j].number,spec,FL_FINDDESC);
							}
							console.pause();
							draw_main(true);
							filemenu.draw();
							break;
						case 'L':
							/* Scan this lib only */
							clear_screen();
							console.putmsg("\r\nchSearch for Text in Description(s) (Lib)\r\n");
							console.putmsg(bbs.text(SearchStringPrompt));
							var spec=console.getstr(40,K_LINE|K_UPPER);
							for(j=0;j<file_area.lib_list[bbs.curlib].dir_list.length;j++)
								bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[j].number,spec,FL_FINDDESC);
							console.pause();
							draw_main(true);
							filemenu.draw();
							break;
						case 'D':
							/* Scan this dir only */
							clear_screen();
							console.putmsg("\r\nchSearch for Text in Description(s) (Dir)\r\n");
							console.putmsg(bbs.text(SearchStringPrompt));
							var spec=console.getstr(40,K_LINE|K_UPPER);
							bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number,spec,FL_FINDDESC);
							console.pause();
							draw_main(true);
							filemenu.draw();
							break;
						case KEY_RIGHT:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							main_right();
							return;
						default:	// Anything else will escape.
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							filemenu.nodraw=true;
							break file;
					}
				}
				break;
			case 'D':
				var typemenu=new Lightbar;
				typemenu.xpos=filemenu.xpos+filemenu.items[0].text.length;
				typemenu.ypos=filemenu.current+1;
				typemenu.lpadding="\xb3";
				typemenu.rpadding="\xb3";
				typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
				typemenu.add(top_bar(17),undefined,undefined,"","");
				typemenu.add('|Batch','B',17,undefined,undefined,bbs.batch_dnload_total<=0);
				typemenu.add('By |Name/File spec','N',17);
				typemenu.add('From |User','U',17);
				typemenu.add(bottom_bar(17),undefined,undefined,"","");
				while(1) {
					switch(typemenu.getval()) {
						case 'B':
							bbs.batch_download();
							/* Redraw just in case */
							draw_main(true);
							filemenu.draw();
							break;
						case 'N':
							var spec=bbs.get_filespec();
							bbs.list_file_info(bbs.curdir,spec,FI_DOWNLOAD);
							break;
						case 'U':
							bbs.list_file_info(bbs.curdir,spec,FI_USERXFER);
							break;
						case KEY_RIGHT:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							main_right();
							return;
						default:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							filemenu.nodraw=true;
							break file;
					}
				}
				break;
			case 'U':
				var typemenu=new Lightbar;
				var width=19;
				typemenu.xpos=filemenu.xpos+filemenu.items[0].text.length;
				typemenu.ypos=filemenu.current+1;
				typemenu.lpadding="\xb3";
				typemenu.rpadding="\xb3";
				typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
				if(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload || file_area.upload_dir==undefined) {
					if(width<17+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length)
						width=17+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length;
				}
				typemenu.add(top_bar(width),undefined,undefined,"","");
				if(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload || file_area.upload_dir==undefined) {
					typemenu.add('To Current |Dir ('+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name+')','C',width,undefined,undefined,!file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload);
				}
				else {
					typemenu.add('To Upload |Dir','P',width);
				}
				typemenu.add('To |Sysop Only','S',width,undefined,undefined,file_area.sysop_dir==undefined);
				typemenu.add('To Specific |User(s)','U',width,undefined,undefined,file_area.user_dir==undefined);
				typemenu.add(bottom_bar(width),undefined,undefined,"","");
				while(1) {
					switch(typemenu.getval()) {
						case 'C':	// Current dir
							bbs.upload_file(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number);
							draw_main(true);
							filemenu.draw();
							break;
						case 'P':	// Upload dir
							bbs.upload_file(file_area.upload_dir);
							draw_main(true);
							filemenu.draw();
							break;
						case 'S':	// Sysop dir
							bbs.upload_file(file_area.sysop_dir);
							draw_main(true);
							filemenu.draw();
							break;
						case 'U':	// To user
							bbs.upload_file(file_area.user_dir);
							draw_main(true);
							filemenu.draw();
						case KEY_RIGHT:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							main_right();
							return;
						default:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							filemenu.nodraw=true;
							break file;
					}
				}
				break;
			case 'R':
				clear_screen();
				fileremove: do {
					console.putmsg("\r\nchRemove/Edit File(s)\r\n");
					str=bbs.get_filespec();
					if(str==null)
						break fileremove;
					if(!bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FI_REMOVE)) {
						var s=0;
						console.putmsg(bbs.text(SearchingAllDirs));
						for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
							if(i!=bbs.curdir &&
									(s=bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FI_REMOVE))!=0) {
								if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
									break fileremove;
								}
							}
						}
						console.putmsg(bbs.text(SearchingAllLibs));
						for(i=0; i<file_area.lib_list.length; i++) {
							if(i==bbs.curlib)
								continue;
							for(j=0; j<file_area.lib_list[i].dir_list.length; j++) {
								if((s=bbs.list_file_info(file_area.lib_list[i].dir_list[j].number, str, FI_REMOVE))!=0) {
									if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
										break fileremove;
									}
								}
							}
						}
					}
				} while(0);
				draw_main(true);
				break;
			case 'B':
				console.attributes=LBShell_Attr;
				clear_screen();
				bbs.batch_menu();
				draw_main(true);
				break;
			case 'V':
				var typemenu=new Lightbar;
				var width=32;
				typemenu.xpos=filemenu.xpos+filemenu.items[0].text.length;
				typemenu.ypos=filemenu.current+1;
				typemenu.lpadding="\xb3";
				typemenu.rpadding="\xb3";
				typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
				typemenu.add(top_bar(width),undefined,undefined,"","");
				typemenu.add('File |Contents','C',width);
				typemenu.add('File |Information','I',width);
				typemenu.add('File Transfer |Policy','P',width);
				typemenu.add('Current |Directory Info','D',width);
				typemenu.add('|Users with Access to Current Dir','U',width);
				typemenu.add('Your File Transfer |Statistics','S',width);
				typemenu.add(bottom_bar(width),undefined,undefined,"","");
				while(1) {
					switch(typemenu.getval()) {
						case 'C':
							clear_screen();
							console.putmsg("\r\nchView File(s)\r\n");
							str=bbs.get_filespec();
							if(str!=null) {
								if(!bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FL_VIEW)) {
									console.putmsg(bbs.text(SearchingAllDirs));
									for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
										if(i==bbs.curdir)
											continue;
										if(bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FL_VIEW))
											break;
									}
									if(i<file_area.lib_list[bbs.curlib].dir_list.length)
										break file;
									console.putmsg(bbs.text(SearchingAllLibs));
									libloop: for(i=0; i<file_area.lib_list.length; i++) {
										if(i==bbs.curlib)
											continue;
										for(j=0; j<file_area.lib_list[i].dir_list.length; j++) {
											if(bbs.list_files(file_area.lib_list[i].dir_list[j].number, str, FL_VIEW))
											break libloop;
										}
									}
								}
							}
							console.pause();
							draw_main(true);
							filemenu.draw();
							break;
						case 'I':
							clear_screen();
							console.putmsg("\r\nchView File Information\r\n");
							str=bbs.get_filespec();
							if(str!=null) {
								if(!bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FI_INFO)) {
									console.putmsg(bbs.text(SearchingAllDirs));
									for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
										if(i==bbs.curdir)
											continue;
										if(bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FI_INFO))
											break;
									}
									if(i<file_area.lib_list[bbs.curlib].dir_list.length)
										break file;
									console.putmsg(bbs.text(SearchingAllLibs));
									libloop: for(i=0; i<file_area.lib_list.length; i++) {
										if(i==bbs.curlib)
											continue;
										for(j=0; j<file_area.lib_list[i].dir_list.length; j++) {
											if(bbs.list_files(file_area.lib_list[i].dir_list[j].number, str, FI_INFO))
											break libloop;
										}
									}
								}
							}
							console.pause();
							draw_main(true);
							filemenu.draw();
							break;
						case 'P':
							clear_screen();
							bbs.xfer_policy();
							console.pause();
							draw_main(true);
							filemenu.draw();
							break;
						case 'D':
							clear_screen();
							bbs.dir_info();
							console.pause();
							draw_main(true);
							filemenu.draw();
							break;
						case 'U':
							clear_screen();
							bbs.list_users(UL_DIR);
							console.pause();
							draw_main(true);
							filemenu.draw();
							break;
						case 'S':
							break;
						case KEY_RIGHT:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							main_right();
							return;
						default:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							filemenu.nodraw=true;
							break file;
					}
				}
				break;
			case 'S':
				var cur=1;
				while(1) {
					var typemenu=new Lightbar;
					var width=28;
					if(user.settings&USER_EXTDESC)
						width++;
					typemenu.xpos=filemenu.xpos+filemenu.items[0].text.length;
					typemenu.ypos=filemenu.current+1;
					typemenu.lpadding="\xb3";
					typemenu.rpadding="\xb3";
					typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
					typemenu.add(top_bar(width),undefined,undefined,"","");
					typemenu.add('Set Batch Flagging '+(user.settings&USER_BATCHFLAG?'Off':'On'),'B',width);
					typemenu.add('Set Extended Descriptions '+(user.settings&USER_EXTDESC?'Off':'On'),'S',width);
					typemenu.add(bottom_bar(width),undefined,undefined,"","");
					typemenu.current=cur;
					switch(typemenu.getval()) {
						case 'B':
							user.settings ^= USER_BATCHFLAG;
							break;
						case 'S':
							/* Need to clear for shorter menu */
							if(user.settings & USER_EXTDESC)
								clearlines(typemenu.xpos+typemenu.items[0].text.length-1,typemenu.ypos,typemenu.items.length);
							user.settings ^= USER_EXTDESC;
							break;
						case KEY_RIGHT:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							main_right();
							return;
						default:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							filemenu.nodraw=true;
							break file;
					}
					cur=typemenu.current;
				}
				break;
		}
		cur=filemenu.current;
		nd=filemenu.nodraw;
	}
}

function show_messagemenu()
{
	var cur=1;
	var nd=false;

	while(!done) {
		var i;
		var j;
		var ret;
		var messagemenu=new Messagemenu();
		messagemenu.current=cur;
		messagemenu.nodraw=nd;

		ret=messagemenu.getval();
		if(ret==KEY_RIGHT) {
			if(messagemenu.items[messagemenu.current].text.substr(-2,2)==' >')
				ret=messagemenu.items[messagemenu.current].retval;
		}
		message: switch(ret) {
			case KEY_LEFT:
				main_left();
				return;
			case '\b':
			case '\x7f':
			case '\x1b':
				return;
			case KEY_RIGHT:
				main_right();
				return;
			case 'C':
				clear_screen();
				if(!msg_area.grp_list.length)
					break;
				msgjump: while(1) {
					var orig_grp=bbs.curgrp;
					var i=0;
					var j=0;
					if(msg_area.grp_list.length>1) {
						if(file_exists(system.text_dir+"menu/grps.*"))
							bbs.menu("grps");
						else {
							console.putmsg(bbs.text(CfgGrpLstHdr),P_SAVEATR);
							for(i=0; i<msg_area.grp_list.length; i++) {
								if(i==bbs.curgrp)
									console.putmsg('*',P_SAVEATR);
								else
									console.putmsg(' ',P_SAVEATR);
								if(i<9)
									console.putmsg(' ',P_SAVEATR);
								if(i<99)
									console.putmsg(' ',P_SAVEATR);
								// We use console.putmsg to expand ^A, @, etc
								console.putmsg(format(bbs.text(CfgGrpLstFmt),i+1,msg_area.grp_list[i].description),P_SAVEATR);
							}
						}
						console.mnemonics(format(bbs.text(JoinWhichGrp),bbs.curgrp+1));
						j=console.getnum(msg_area.grp_list.length);
						if(j<0)
							break msgjump;
						if(!j)
							j=bbs.curgrp;
						else
							j--;
					}
					bbs.curgrp=j;
					if(file_exists(system.text_dir+"menu/subs"+(bbs.curgrp+1)))
						bbs.menu("subs"+(bbs.curgrp+1));
					else {
						console.clear();
						console.putmsg(format(bbs.text(SubLstHdr), msg_area.grp_list[j].description),P_SAVEATR);
						for(i=0; i<msg_area.grp_list[j].sub_list.length; i++) {
							var msgbase=new MsgBase(msg_area.grp_list[j].sub_list[i].code);
							if(msgbase==undefined)
								continue;
							if(!msgbase.open())
								continue;
							if(i==bbs.cursub)
								console.putmsg('*',P_SAVEATR);
							else
								console.putmsg(' ',P_SAVEATR);
							if(i<9)
								console.putmsg(' ',P_SAVEATR);
							if(i<99)
								console.putmsg(' ',P_SAVEATR);
							console.putmsg(format(bbs.text(SubLstFmt),i+1, msg_area.grp_list[j].sub_list[i].description,"",msgbase.total_msgs),P_SAVEATR);
							msgbase.close();
						}
					}
					console.mnemonics(format(bbs.text(JoinWhichSub),bbs.cursub+1));
					i=console.getnum(msg_area.grp_list[j].sub_list.length);
					if(i==-1) {
						if(msg_area.grp_list.length==1) {
							bbs.curgrp=orig_grp;
							break msgjump;
						}
						continue;
					}
					if(!i)
						i=bbs.cursub;
					else
						i--;
					bbs.cursub=i;
					break;
				}
				draw_main(true);
				break;
			case 'R':
				clear_screen();
				bbs.scan_posts();
				draw_main(true);
				break;
			case 'N':
				var typemenu=new Lightbar;
				var width=38;
				if(width<16+msg_area.grp_list[bbs.curgrp].name.length)
					width=16+msg_area.grp_list[bbs.curgrp].name.length;
				if(width<14+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length)
					width=14+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length;
				typemenu.xpos=messagemenu.xpos+messagemenu.items[0].text.length;
				typemenu.ypos=messagemenu.current+1;
				typemenu.lpadding="\xb3";
				typemenu.rpadding="\xb3";
				typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
				typemenu.add(top_bar(width),undefined,undefined,"","");
				typemenu.add('|All Message Areas','A',width);
				typemenu.add("Current |Group ("+msg_area.grp_list[bbs.curgrp].name+")",'G',width);
				typemenu.add('Current |Sub ('+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name+')','S',width);
				typemenu.add('Change New Scan |Configuration','C',width);
				typemenu.add('Change New Scan |Pointers','P',width);
				typemenu.add('|Reset New Scan Pointers to Logon State','R',width);
				typemenu.add(bottom_bar(width),undefined,undefined,"","");
				while(1) {
					switch(typemenu.getval()) {
						case 'A':
							clear_screen();
							console.putmsg("\r\n\x01c\x01hNew Message Scan\r\n");
							bbs.scan_subs(SCAN_NEW);
							draw_main(true);
							messagemenu.draw();
							break;
						case 'G':
							clear_screen();
							console.putmsg("\r\n\x01c\x01hNew Message Scan\r\n");
							for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++)
								bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_NEW);
							draw_main(true);
							messagemenu.draw();
							break;
						case 'S':
							clear_screen();
							console.putmsg("\r\n\x01c\x01hNew Message Scan\r\n");
							bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_NEW);
							draw_main(true);
							messagemenu.draw();
							break;
						case 'C':
							clear_screen();
							bbs.cfg_msg_scan(SCAN_CFG_NEW);
							draw_main(true);
							messagemenu.draw();
							break;
						case 'P':
							clear_screen();
							bbs.cfg_msg_ptrs(SCAN_CFG_NEW);
							draw_main(true);
							messagemenu.draw();
							break;
						case 'R':
							bbs.reinit_msg_ptrs()
							break;
						case KEY_RIGHT:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							main_right();
							return;
						default:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							messagemenu.nodraw=true;
							break message;
					}
				}
				break;
			case 'Y':
				var typemenu=new Lightbar;
				var width=30;
				if(width<16+msg_area.grp_list[bbs.curgrp].name.length)
					width=16+msg_area.grp_list[bbs.curgrp].name.length;
				if(width<14+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length)
					width=14+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length;
				typemenu.xpos=messagemenu.xpos+messagemenu.items[0].text.length;
				typemenu.ypos=messagemenu.current+1;
				typemenu.lpadding="\xb3";
				typemenu.rpadding="\xb3";
				typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
				typemenu.add(top_bar(width),undefined,undefined,"","");
				typemenu.add('|All Message Areas','A',width);
				typemenu.add("Current |Group ("+msg_area.grp_list[bbs.curgrp].name+")",'G',width);
				typemenu.add('Current |Sub ('+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name+')','S',width);
				typemenu.add('Change Your Scan |Configuration','C',width);
				typemenu.add(bottom_bar(width),undefined,undefined,"","");
				while(1) {
					switch(typemenu.getval()) {
						case 'A':
							clear_screen();
							console.putmsg("\r\n\x01c\x01hYour Message Scan\r\n");
							bbs.scan_subs(SCAN_TOYOU);
							draw_main(true);
							messagemenu.draw();
							break;
						case 'G':
							clear_screen();
							console.putmsg("\r\n\x01c\x01hYour Message Scan\r\n");
							for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++)
								bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_TOYOU);
							draw_main(true);
							messagemenu.draw();
							break;
						case 'S':
							clear_screen();
							console.putmsg("\r\n\x01c\x01hYour Message Scan\r\n");
							bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_TOYOU);
							draw_main(true);
							messagemenu.draw();
							break;
						case 'C':
							clear_screen();
							bbs.cfg_msg_scan(SCAN_CFG_TOYOU);
							draw_main(true);
							messagemenu.draw();
							break;
						case KEY_RIGHT:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							main_right();
							return;
						default:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							messagemenu.nodraw=true;
							break message;
					}
				}
				break;
			case 'T':
				var typemenu=new Lightbar;
				var width=17;
				if(width<16+msg_area.grp_list[bbs.curgrp].name.length)
					width=16+msg_area.grp_list[bbs.curgrp].name.length;
				if(width<14+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length)
					width=14+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length;
				typemenu.xpos=messagemenu.xpos+messagemenu.items[0].text.length;
				typemenu.ypos=messagemenu.current+1;
				typemenu.lpadding="\xb3";
				typemenu.rpadding="\xb3";
				typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
				typemenu.add(top_bar(width),undefined,undefined,"","");
				typemenu.add('|All Message Areas','A',width);
				typemenu.add("Current |Group ("+msg_area.grp_list[bbs.curgrp].name+")",'G',width);
				typemenu.add('Current |Sub ('+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name+')','S',width);
				typemenu.add(bottom_bar(width),undefined,undefined,"","");
				while(1) {
					switch(typemenu.getval()) {
						case 'A':
							clear_screen();
							console.putmsg("\r\n\x01c\x01hMessage Search\r\n");
							console.putmsg(bbs.text(SearchStringPrompt));
							str=console.getstr("",40,K_LINE|K_UPPER);
							for(i=0; i<msg_area.grp_list.length; i++) {
								for(j=0; j<msg_area.grp_list[i].sub_list.length; j++) {
									bbs.scan_posts(msg_area.grp_list[i].sub_list[j].number, SCAN_FIND, str);
								}
							}
							draw_main(true);
							messagemenu.draw();
							break;
						case 'G':
							clear_screen();
							console.putmsg("\r\n\x01c\x01hMessage Search\r\n");
							str=console.getstr("",40,K_LINE|K_UPPER);
							for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++)
								bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_FIND, str);
							draw_main(true);
							messagemenu.draw();
							break;
						case 'S':
							clear_screen();
							console.putmsg("\r\n\x01c\x01hMessage Search\r\n");
							str=console.getstr("",40,K_LINE|K_UPPER);
							bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_FIND, str);
							draw_main(true);
							messagemenu.draw();
							break;
						case KEY_RIGHT:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							main_right();
							return;
						default:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							messagemenu.nodraw=true;
							break message;
					}
				}
				break;
			case 'P':
				clear_screen();
				bbs.post_msg();
				draw_main(true);
				break;
			case 'A':
				clear_screen();
				bbs.auto_msg();
				draw_main(true);
				break;
			case 'Q':
				clear_screen();
				bbs.qwk_sec();
				draw_main(true);
				break;
		}
		cur=messagemenu.current;
		nd=messagemenu.nodraw;
	}
}

function show_emailmenu()
{
	var cur=1;
	/* There's nothing dynamic, so we can fiddle this here */
	var emailmenu=new Emailmenu();
	/* For consistency */
	emailmenu.current=cur;

	while(!done) {
		var i;
		var j;
		var ret;

		/* Nothing dynamic, so we don't need to save/restore nodraw */

		ret=emailmenu.getval();
		if(ret==KEY_RIGHT) {
			if(emailmenu.items[emailmenu.current].text.substr(-2,2)==' >')
				ret=emailmenu.items[emailmenu.current].retval;
		}
		email: switch(ret) {
			case KEY_LEFT:
				main_left();
				return;
			case '\b':
			case '\x7f':
			case '\x1b':
				return;
			case KEY_RIGHT:
				main_right();
				return;
			case 'S':
				var typemenu=new Lightbar;
				var width=30;
				typemenu.xpos=emailmenu.xpos+emailmenu.items[0].text.length;
				typemenu.ypos=emailmenu.current+1;
				typemenu.lpadding="\xb3";
				typemenu.rpadding="\xb3";
				typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
				typemenu.add(top_bar(width),undefined,undefined,"","");
				typemenu.add('To |Sysop','S',width,undefined,undefined,user.compare_ars("REST S"));
				typemenu.add('To |Local User','L',width,undefined,undefined,user.compare_ars("REST E"));
				typemenu.add('To Local User with |Attachment','A',width,undefined,undefined,user.compare_ars("REST E"));
				typemenu.add('To |Remote User','R',width,undefined,undefined,user.compare_ars("REST E OR REST M"));
				typemenu.add('To Remote User with A|ttachment','T',width,undefined,undefined,user.compare_ars("REST E OR REST M"));
				typemenu.add(bottom_bar(width),undefined,undefined,"","");
				while(1) {
					switch(typemenu.getval()) {
						case 'S':
							clear_screen();
							bbs.email(1,WM_EMAIL,bbs.text(ReFeedback));
							draw_main(true);
							break;
						case 'L':
							clear_screen();
							console.putmsg("\x01_\r\n\x01b\x01hE-mail (User name or number): \x01w");
							str=console.getstr("",40,K_UPRLWR);
							if(str!=null && str!="") {
								if(str=="Sysop")
									str="1";
								if(str.search(/\@/)!=-1)
									bbs.netmail(str);
								else {
									i=bbs.finduser(str);
									if(i>0)
										bbs.email(i,WM_EMAIL);
								}
							}
							draw_main(true);
							break;
						case 'A':
							clear_screen();
							console.putmsg("\x01_\r\n\x01b\x01hE-mail (User name or number): \x01w");
							str=console.getstr("",40,K_UPRLWR);
							if(str!=null && str!="") {
								i=bbs.finduser(str);
								if(i>0)
									bbs.email(i,WM_EMAIL|WM_FILE);
							}
							draw_main(true);
							break;
						case 'R':
							clear_screen();
							if(console.noyes("\r\nAttach a file"))
								i=0;
							else
								i=WM_FILE;
							console.putmsg(bbs.text(EnterNetMailAddress),P_SAVEATR);
							str=console.getstr("",60,K_LINE);
							if(str!=null && str !="")
								bbs.netmail(str,i);
							draw_main(true);
							break;
						case 'T':
							clear_screen();
							console.putmsg("\x01_\r\n\x01b\x01hE-mail (User name or number): \x01w");
							str=console.getstr("",40,K_UPRLWR);
							if(str!=null && str!="")
								bbs.netmail(str,WM_FILE);
							draw_main(true);
							break;
						case KEY_RIGHT:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							main_right();
							return;
						default:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							emailmenu.nodraw=true;
							break email;
					}
				}
				break;
			case 'R':
				clear_screen();
				bbs.read_mail(MAIL_YOUR);
				console.pause();
				draw_main(true);
				break;
			case 'M':
				clear_screen();
				bbs.read_mail(MAIL_SENT);
				console.pause();
				draw_main(true);
				break;
		}
		cur=emailmenu.current;
	}
}

function show_chatmenu()
{
	var cur=1;
	/* There's nothing dynamic, so we can fiddle this here */
	var chatmenu=new Chatmenu();
	/* For consistency */
	chatmenu.current=cur;

	while(!done) {
		var i;
		var j;
		var ret;

		/* Nothing dynamic, so we don't need to save/restore nodraw */

		ret=chatmenu.getval();
		if(ret==KEY_RIGHT) {
			if(chatmenu.items[chatmenu.current].text.substr(-2,2)==' >')
				ret=chatmenu.items[chatmenu.current].retval;
		}
		chat: switch(ret) {
			case KEY_LEFT:
				main_left();
				return;
			case '\b':
			case '\x7f':
			case '\x1b':
				return;
			case KEY_RIGHT:
				main_right();
				return;
			case 'J':
				clear_screen();
				bbs.multinode_chat();
				draw_main(true);
				break;
			case 'P':
				clear_screen();
				bbs.private_chat();
				draw_main(true);
				break;
			case 'C':
				clear_screen();
				if(!bbs.page_sysop())
					bbs.page_guru();
				draw_main(true);
				break;
			case 'T':
				clear_screen();
				bbs.page_guru();
				draw_main(true);
				break;
			case 'F':
				clear_screen();
				bbs.exec("?finger");
				console.pause();
				draw_main(true);
				break;
			case 'R':
				clear_screen();
				write("\001n\001y\001hServer and channel: ");
				str="irc.synchro.net 6667 #Synchronet";
				str=console.getstr(str, 50, K_EDIT|K_LINE|K_AUTODEL);
				if(!console.aborted)
					bbs.exec("?irc -a "+str);
				draw_main(true);
				break;
			case 'M':
				clear_screen();
				bbs.exec("?sbbsimsg");
				draw_main(true);
				break;
			case 'S':
				while(1) {
					var typemenu=new Lightbar;
					var width=24;
					if(user.chat_settings&CHAT_SPLITP)
						width++;
					typemenu.xpos=chatmenu.xpos+chatmenu.items[0].text.length;
					typemenu.ypos=chatmenu.current+1;
					typemenu.lpadding="\xb3";
					typemenu.rpadding="\xb3";
					typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
					typemenu.add(top_bar(width),undefined,undefined,"","");
					typemenu.add("Set |Split Screen Chat "+(user.chat_settings&CHAT_SPLITP?"Off":"On"),'S',width);
					typemenu.add("Set A|vailability "+(user.chat_settings&CHAT_NOPAGE?"On":"Off"),'V',width);
					typemenu.add("Set Activity |Alerts "+(user.chat_settings&CHAT_NOACT?"On":"Off"),'A',width);
					typemenu.add(bottom_bar(width),undefined,undefined,"","");
					switch(typemenu.getval()) {
						case 'S':
							if(user.chat_settings&CHAT_SPLITP)
								clearlines(typemenu.xpos+typemenu.items[0].text.length-1,typemenu.ypos,typemenu.items.length);
							user.chat_settings ^= CHAT_SPLITP;
							break;
						case 'V':
							user.chat_settings ^= CHAT_NOPAGE;
							break;
						case 'A':
							user.chat_settings ^= CHAT_NOACT;
							break;
						case KEY_RIGHT:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							main_right();
							return;
						default:
							clearlines(typemenu.xpos,typemenu.ypos,typemenu.items.length);
							chatmenu.nodraw=true;
							break chat;
					}
				}
				break;
		}
		cur=emailmenu.current;
	}
}

function show_settingsmenu()
{
	var settingsmenu=new Settingsmenu();
	while(1) {
		switch(settingsmenu.getval()) {
			case 'U':
				clear_screen();
				var oldshell=user.command_shell;
				bbs.user_config();
				/* Still using this shell? */
				if(user.command_shell != oldshell)
					exit(0);
				draw_main(true);
				break;
			case 'B':
				clear_screen();
				bbs.time_bank();
				draw_main(true);
				break;
			case KEY_RIGHT:
				main_right();
				return;
			case KEY_LEFT:
				main_left();
				return;
			case '\b':
			case '\x7f':
			case '\x1b':
				return;
		}
	}
}

function clearlines(x,y,count)
{
	var i;
	console.attributes=LBShell_Attr;
	for(i=0; i<count;i++) {
		console.gotoxy(x,y+i);
		console.cleartoeol();
	}
}
