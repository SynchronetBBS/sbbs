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
//#	All lines starting with // are considered comments and are ignored
//#	Left margins (indents) are not relevant and used only for clarity
//#	Almost everything is case sensitive with the exception of @-codes
//#
//################################# Begins Here #################################

load("sbbsdefs.js");
load("lightbar.js");
bbs.command_str='';	// Clear STR (Contains the EXEC for default.js)
load("str_cmds.js");
var str;
const LBShell_Attr=0x37;

function Mainbar()
{
	this.direction=1;
	this.xpos=2;
	this.ypos=1;
	this.hotkeys=KEY_DOWN+";";
	this.add("|File","F",undefined,undefined,undefined,user.compare_ars("REST T"));
	this.add("|Settings","S");
	this.add("|Email","E");
	this.add("|Messages","M");
	this.add("|Chat","C",undefined,undefined,undefined,user.compare_ars("REST C"));
	this.add("E|xternals","x",undefined,undefined,undefined,user.compare_ars("REST X"));
	this.add("|Info","I");
	this.add("|Goodbye","G");
	this.add("Commands",";");
}
Mainbar.prototype=new Lightbar;
var mainbar=new Mainbar;

	var filemenu=new Lightbar;
function Filemenu()
{
	this.xpos=1;
	this.ypos=2;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	this.add("|List files","L",19);
	this.add("|Download","D",19,undefined,undefined,user.compare_ars("REST D"));
	this.add("File |Info       -->","I",19);
	this.add("|Extended File Info","E",19);
	this.add("|Search Descriptions","S",19);
	this.add("Search |Filenames","F",19);
	this.add("|Change Directory","C",19);
	this.add("|New File Scan","N",19);
	this.add("|Batch Transfer Menu","B",19,undefined,undefined,user.compare_ars("REST U AND REST D"));
	this.add("|Remove/Edit File","R",19);
	this.add("|Upload File","U",19,undefined,undefined,user.compare_ars("REST U"));
	this.add("|View File","V",19);
	this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
}
Filemenu.prototype=new Lightbar;
var filemenu=new Filemenu;

function Fileinfo()
{
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
	this.xpos=7;
	this.ypos=2;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	this.add("|User Config","U",24);
	this.add("|Message Scan Config","M",24);
	this.add("To |You Scan Config","Y",24);
	this.add("Message |Pointers","P",24);
	this.add("|File Xfer Config     -->","F",24);
	this.add("|Re-Init Message Pointers","R",24);
	this.add("|Toggle Paging","T",24);
	this.add("|Activity Alerts On/Off","A",24);
	this.add("Minute |Bank","B",24);
	this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
}
Settingsmenu.prototype=new Lightbar;
var settingsmenu=new Settingsmenu;

function Xfercfgmenu()
{
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
	this.xpos=17;
	this.ypos=2;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	this.add("|Send Mail","S",24,undefined,undefined,user.compare_ars("REST E"));
	this.add("Send |NetMail","N",24,undefined,undefined,user.compare_ars("REST M OR REST E"));
	this.add("Send |Feedback to Sysop","F",24,undefined,undefined,user.compare_ars("REST S"));
	this.add("|Read Mail Sent To You","R",24);
	this.add("Read Mail |You Have Sent","Y",24,undefined,undefined,user.compare_ars("REST K"));
	this.add("|Upload File To a Mailbox","U",24);
	this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
}
Emailmenu.prototype=new Lightbar;
var emailmenu=new Emailmenu;

function Messagemenu()
{
	this.xpos=24;
	this.ypos=2;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	this.add("|New Message Scan","N",24);
	this.add("|Read Message Prompt","R",24);
	this.add("|Continuous New Scan","C",24);
	this.add("|Browse New Scan","B",24);
	this.add("|QWK Packet Transfer","Q",24);
	this.add("|Post a Message","P",24,undefined,undefined,user.compare_ars("REST P"));
	if(user.compare_ars("REST N") && (msg_area.grp_list[bbs.curgrp].sub_list[bbs.crusub] & (SUB_QNET|SUB_PNET|SUB_FIDO)))
		this.items[6].disabed=true;
	this.add("Read/Post |Auto-Message","A",24);
	this.add("|Find Text in Messages","F",24);
	this.add("|Scan For Messages To You","S",24);
	this.add("|Jump To New Sub-Board","J",24);
	this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
}
Messagemenu.prototype=new Lightbar;
var messagemenu=new Messagemenu;

function Chatmenu()
{
	this.xpos=34;
	this.ypos=2;
	this.lpadding="\xb3";
	this.rpadding="\xb3";
	this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	this.add("|Join/Initiate Multinode Chat","J",39);
	this.add("Join/Initiate |Private Node to Node Chat","P",39);
	this.add("|Chat With The SysOp","C",39);
	this.add("|Talk With The System Guru","T",39);
	this.add("|Finger (Query) A Remote User or System","F",39);
	this.add("I|RC Chat","R",39);
	this.add("|InterBBS Instant Messages","I",39);
	this.add("|Toggle Split Screen Private Chat","S",39);
	this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
}
Chatmenu.prototype=new Lightbar;
var chatmenu=new Chatmenu;

var bars40="\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4";
// Generate menus of available xtrn sections.
function Xtrnsecs()
{
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
	this.add("\xda"+bars40.substr(0,xtrnsecwidth)+"\xbf",undefined,undefined,"","");
	for(j=0; j<xtrn_area.sec_list.length; j++)
		this.add("<-- "+xtrn_area.sec_list[j].name,j.toString(),xtrnsecwidth);
	this.add("\xc0"+bars40.substr(0,xtrnsecwidth)+"\xd9",undefined,undefined,"","");
}
Xtrnsecs.prototype=new Lightbar;
var xtrnsec=new Xtrnsecs;

function Xtrnsec(sec)
{
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
	this.add("\xda"+bars40.substr(0,xtrnsecprogwidth)+"\xbf",undefined,undefined,"","");
	for(j=0; j<xtrn_area.sec_list[sec].prog_list.length && j<console.screen_rows-3; j++)
		this.add(xtrn_area.sec_list[sec].prog_list[j].name,j.toString(),xtrnsecprogwidth);
	this.add("\xc0"+bars40.substr(0,xtrnsecprogwidth)+"\xd9",undefined,undefined,"","");
}
Xtrnsec.prototype=new Lightbar;
var xtrnsecs=new Array();
var j;
for(j=0; j<xtrn_area.sec_list.length; j++)
	xtrnsecs[j]=new Xtrnsec(j);

function Infomenu()
{
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
			done=0;
			while(!done) {
				file: switch(filemenu.getval()) {
					case KEY_LEFT:
						main_left();
						done=1;
						break;
					case '\b':
					case '\x7f':
					case '\x1b':
						done=1;
						break;
					case 'B':
						console.attributes=LBShell_Attr;
						clear_screen();
						bbs.batch_menu();
						draw_main(true);
						break;
					case 'D':
						download: do {
							clear_screen();
							console.putmsg("\r\nchDownload File(s)\r\n");
							if(bbs.batch_dnload_total>0) {
								if(console.yesno(bbs.text(DownloadBatchQ))) {
									bbs.batch_download();
									break;
								}
							}
							str=bbs.get_filespec();
							if(str==null)
								break;
							if(file_area.lib_list.length==0)
								break;
							if(user.security.restrictions&UFLAG_D) {
								console.putmsg(bbs.text(R_Download));
								break;
							}
							if(!bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FI_DOWNLOAD)) {
								var s=0;
								console.putmsg(bbs.text(SearchingAllDirs));
								for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
									if(i!=bbs.curdir &&
											(s=bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FI_DOWNLOAD))!=0) {
										if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
												break download;
										}
									}
								}
								console.putmsg(bbs.text(SearchingAllLibs));
								for(i=0; i<file_area.lib_list.length; i++) {
									if(i==bbs.curlib)
										continue;
									for(j=0; j<file_area.lib_list[i].dir_list.length; j++) {
										if((s=bbs.list_file_info(file_area.lib_list[i].dir_list[j].number, str, FI_DOWNLOAD))!=0) {
											if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
												break download;
											}
										}
									}
								}
							}
						} while(0);
						draw_main(true);
						break;
					case KEY_RIGHT:
						if(filemenu.items[filemenu.current].retval!='I') {
							main_right();
							done=1;
							break;
						}
						// Fall-through
					case 'I':
						var info_done=0;
						while(!info_done) {
							switch(fileinfo.getval()) {
								case 'T':
									clear_screen();
									bbs.xfer_policy();
									console.pause();
									break;
								case 'Y':
									clear_screen();
									bbs.user_info();
									console.pause();
									break;
								case 'D':
									clear_screen();
									bbs.dir_info();
									console.pause();
									break;
								case 'U':
									clear_screen();
									bbs.list_users(UL_DIR);
									console.pause();
									break;
								case KEY_RIGHT:
									main_right();
									info_done=1;
									done=1;
									break;
								case KEY_LEFT:
								case '\b':
								case '\x7f':
								case '\x1b':
									info_done=1;
									break;
							}
							if(info_done) {
								console.attributes=LBShell_Attr;
								console.gotoxy(22,4);
								console.cleartoeol();
								console.gotoxy(22,5);
								console.cleartoeol();
								console.gotoxy(22,6);
								console.cleartoeol();
								console.gotoxy(22,7);
								console.cleartoeol();
								console.gotoxy(22,8);
								console.cleartoeol();
								console.gotoxy(22,9);
								console.cleartoeol();
							}
							else {
								draw_main(true);
								filemenu.draw();
							}
						}
						break;
					case 'E':
						clear_screen();
						console.putmsg("\r\nchList Extended File Information\r\n");
						str=bbs.get_filespec();
						if(str==null)
							break file;
						if(!bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FI_INFO)) {
							var s=0;
							console.putmsg(bbs.text(SearchingAllDirs));
							for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
								if(i!=bbs.curdir &&
										(s=bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FI_INFO))!=0) {
									if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
										break file;
									}
								}
							}
							console.putmsg(bbs.text(SearchingAllLibs));
							for(i=0; i<file_area.lib_list.length; i++) {
								if(i==bbs.curlib)
									continue;
								for(j=0; j<file_area.lib_list[i].dir_list.length; j++) {
									if((s=bbs.list_file_info(file_area.lib_list[i].dir_list[j].number, str, FI_INFO))!=0) {
										if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
											break file;
										}
									}
								}
							}
						}
						console.pause();
						draw_main(true);
						break;
					case 'S':
						clear_screen();
						console.putmsg("\r\nchFind Text in File Descriptions (no wildcards)\r\n");
						bbs.scan_dirs(FL_FINDDESC);
						console.pause();
						draw_main(true);
						break;
					case 'F':
						clear_screen();
						console.putmsg("\r\nchSearch for Filename(s)\r\n");
						bbs.scan_dirs(FL_NO_HDR);
						console.pause();
						draw_main(true);
						break;
					case 'C':
						clear_screen();
						changedir: do {
							if(!file_area.lib_list.length)
								break changedir;
							while(1) {
								var orig_lib=bbs.curlib;
								var i=0;
								var j=0;
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
						i=bbs.list_files();
						if(i!=-1) {
							if(i==0)
								console.putmsg(bbs.text(EmptyDir),P_SAVEATR);
							else
								console.putmsg(bbs.text(NFilesListed,i),P_SAVEATR);
						}
						console.pause();
						draw_main(true);
						break;
					case 'N':
						clear_screen();
						console.putmsg("\r\nchNew File Scan\r\n");
						bbs.scan_dirs(FL_ULTIME);
						console.pause();
						draw_main(true);
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
					case 'U':
						clear_screen();
						console.putmsg("\r\nchUpload File\r\n");
						i=0xffff;	/* INVALID_DIR */
						if(file_exists(system.text_dir+"menu/upload.*"))
							bbs.menu("upload");
						if(file_area.lib_list.length) {
							i=file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number;
							if(file_area.upload_dir != undefined && !file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload)
								i=file_area.upload_dir.number;
						}
						else {
							if(file_area.upload_dir != undefined)
								i=file_area.upload_dir.number;
						}
						bbs.upload_file(i);
						draw_main(true);
						break;
					case 'V':
						clear_screen();
						fileview: do {
							console.putmsg("\r\nchView File(s)\r\n");
							str=bbs.get_filespec();
							if(str==null)
								break fileview;
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
								for(i=0; i<file_area.lib_list.length; i++) {
									if(i==bbs.curlib)
										continue;
									for(j=0; j<file_area.lib_list[i].dir_list.length; j++) {
										if(bbs.list_files(file_area.lib_list[i].dir_list[j].number, str, FL_VIEW))
											break fileview;
									}
								}
							}
						} while(0);
						draw_main(true);
						break;
				}
			}
			break;
		case 'S':
			done=0;
			while(!done) {
				switch(settingsmenu.getval()) {
					case 'U':
						clear_screen();
						bbs.user_config();
						draw_main(true);
						break;
					case 'M':
						clear_screen();
						bbs.cfg_msg_scan(SCAN_CFG_NEW);
						draw_main(true);
						break;
					case 'Y':
						clear_screen();
						bbs.cfg_msg_scan(SCAN_CFG_TOYOU);
						draw_main(true);
						break;
					case 'P':
						clear_screen();
						bbs.cfg_msg_ptrs();
						draw_main(true);
						break;
					case KEY_RIGHT:
						if(settingsmenu.items[settingsmenu.current].retval!='F') {
							main_right();
							done=1;
							break;
						}
						// Fall-through
					case 'F':
						var xfercfgdone=0;
						while(!xfercfgdone) {
							switch(xfercfgmenu.getval()) {
								case 'S':
									clear_screen();
									bbs.new_file_time=bbs.get_newscantime(bbs.new_file_time);
									draw_main(true);
									settingsmenu.draw();
									break;
								case 'B':
									user.settings ^= USER_BATCHFLAG;
									break;
								case 'E':
									user.settings ^= USER_EXTDESC;
									break;
								case KEY_RIGHT:
									main_right();
									xfercfgdone=1;
									done=1;
									break;
								case KEY_LEFT:
								case '\b':
								case '\x7f':
								case '\x1b':
									console.attributes=LBShell_Attr;
									console.gotoxy(33,6);
									console.cleartoeol();
									console.gotoxy(33,7);
									console.cleartoeol();
									console.gotoxy(33,8);
									console.cleartoeol();
									console.gotoxy(33,9);
									console.cleartoeol();
									console.gotoxy(33,10);
									console.cleartoeol();
									xfercfgdone=1;
							}
						}
						break;
					case 'R':
						bbs.reinit_msg_ptrs();
						break;
					case 'T':
						user.chat_settings ^= CHAT_NOPAGE;
						system.node_list[bbs.node_num-1].misc ^= NODE_POFF;
						break;
					case 'A':
						user.chat_settings ^= CHAT_NOACT;
						system.node_list[bbs.node_num-1].misc ^= NODE_AOFF;
						break;
					case 'B':
						clear_screen();
						bbs.time_bank();
						draw_main(true);
						break;
					case KEY_LEFT:
						main_left();
						done=1;
						break;
					case '\b':
					case '\x7f':
					case '\x1b':
						done=1;
						break;
				}
			}
			break;
		case 'E':
			done=0;
			while(!done) {
				switch(emailmenu.getval()) {
					case 'S':
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
					case 'N':
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
					case 'F':
						clear_screen();
						bbs.email(1,WM_EMAIL,bbs.text(ReFeedback));
						draw_main(true);
						break;
					case 'R':
						clear_screen();
						bbs.read_mail(MAIL_YOUR);;
						console.pause();
						draw_main(true);
						break;
					case 'Y':
						clear_screen();
						bbs.read_mail(MAIL_SENT);;
						console.pause();
						draw_main(true);
						break;
					case 'U':
						clear_screen();
						console.putmsg("\x01_\r\n\x01b\x01hE-mail (User name or number): \x01w");
						str=console.getstr("",40,K_UPRLWR);
						if(str!=null && str!="") {
							if(str=="Sysop")
								str="1";
							if(str.search(/\@/)!=-1)
								bbs.netmail(str,WM_FILE);
							else {
								i=bbs.finduser(str);
								if(i>0)
									bbs.email(i,WM_EMAIL|WM_FILE);
							}
						}
						draw_main(true);
						break;
					case KEY_RIGHT:
						main_right();
						done=1;
						break;
					case KEY_LEFT:
						main_left();
						done=1;
						break;
					case '\b':
					case '\x7f':
					case '\x1b':
						done=1;
						break;
				}
			}
			break;
		case 'M':
			done=0;
			while(!done) {
				if(user.compare_ars("REST N") && (msg_area.grp_list[bbs.curgrp].sub_list[bbs.crusub] & (SUB_QNET|SUB_PNET|SUB_FIDO)))
					messagemenu.items[6].disabed=true;
				switch(messagemenu.getval()) {
					case 'N':
						clear_screen();
						console.putmsg("\r\n\x01c\x01hNew Message Scan\r\n");
						bbs.scan_subs(SCAN_NEW);
						draw_main(true);
						break;
					case 'R':
						clear_screen();
						bbs.scan_posts();
						draw_main(true);
						break;
					case 'C':
						clear_screen();
						console.putmsg("\r\n\x01c\x01hContinuous New Message Scan\r\n");
						bbs.scan_subs(SCAN_NEW|SCAN_CONST);
						console.pause();
						draw_main(true);
						break;
					case 'B':
						clear_screen();
						console.putmsg("\r\n\x01c\x01hBrowse/New Message Scan\r\n");
						bbs.scan_subs(SCAN_NEW|SCAN_BACK);
						draw_main(true);
						break;
					case 'Q':
						clear_screen();
						bbs.qwk_sec();
						draw_main(true);
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
					case 'F':
						clear_screen();
						console.putmsg("\r\n\x01c\x01hFind Text in Messages\r\n");
						bbs.scan_subs(SCAN_FIND);
						console.pause();
						draw_main(true);
						break;
					case 'S':
						clear_screen();
						console.putmsg("\r\n\x01c\x01hScan for Messages Posted to You\r\n");
						bbs.scan_subs(SCAN_TOYOU);
						console.pause();
						draw_main(true);
						break;
					case 'J':
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
					case KEY_RIGHT:
						main_right();
						done=1;
						break;
					case KEY_LEFT:
						main_left();
						done=1;
						break;
					case '\b':
					case '\x7f':
					case '\x1b':
						done=1;
						break;
				}
			}
			break;
		case 'C':
			done=0;
			while(!done) {
				switch(chatmenu.getval()) {
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
					case 'I':
						clear_screen();
						bbs.exec("?sbbsimsg");
						draw_main(true);
						break;
					case 'S':
						user.chat_settings ^= CHAT_SPLITP;
						break;
					case KEY_RIGHT:
						main_right();
						done=1;
						break;
					case KEY_LEFT:
						main_left();
						done=1;
						break;
					case '\b':
					case '\x7f':
					case '\x1b':
						done=1;
						break;
				}
			}
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
					next_key='I';
					mainbar.current++;
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
		case 'I':
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
