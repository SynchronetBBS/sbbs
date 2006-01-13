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

var mainbar=new Lightbar;
mainbar.direction=1;
mainbar.xpos=2;
mainbar.ypos=1;
mainbar.hotkeys=KEY_DOWN|";";
mainbar.add("|File","F");
	var filemenu=new Lightbar;
	filemenu.xpos=1;
	filemenu.ypos=2;
	filemenu.lpadding="\xb3";
	filemenu.rpadding="\xb3";
	filemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	filemenu.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	filemenu.add("|Batch Download","B",19);
	filemenu.add("|Download","D",19);
	filemenu.add("File |Info       -->","I",19);
		var fileinfo=new Lightbar;
		fileinfo.xpos=22;
		fileinfo.ypos=4;
		fileinfo.lpadding="\xb3";
		fileinfo.rpadding="\xb3";
		fileinfo.hotkeys=KEY_LEFT+"\b\x7f\x1b";
		fileinfo.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
		fileinfo.add("File |Transfer Policy","T",32);
		fileinfo.add("Information on Current |Directory","D",32);
		fileinfo.add("|Users With Access to Current Dir","U",32);
		fileinfo.add("|Your File Transfer Statistics","Y",32);
		fileinfo.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
	filemenu.add("|Extended File Info","E",19);
	filemenu.add("|Search Descriptions","S",19);
	filemenu.add("Search |Filenames","F",19);
	filemenu.add("|Change Directory","C",19);
	filemenu.add("|List files","L",19);
	filemenu.add("|New File Scan","N",19);
	filemenu.add("|Remove/Edit File","R",19);
	filemenu.add("|Upload File","U",19);
	filemenu.add("|View File","V",19);
	filemenu.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
mainbar.add("|Settings","S");
	var settingsmenu=new Lightbar;
	settingsmenu.xpos=7;
	settingsmenu.ypos=2;
	settingsmenu.lpadding="\xb3";
	settingsmenu.rpadding="\xb3";
	settingsmenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	settingsmenu.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	settingsmenu.add("|User Config","U",24);
	settingsmenu.add("|Message Scan Config","M",24);
	settingsmenu.add("To |You Scan Config","Y",24);
	settingsmenu.add("Message |Pointers","P",24);
	settingsmenu.add("|File Xfer Config     -->","F",24);
		var xfercfgmenu=new Lightbar;
		xfercfgmenu.xpos=33;
		xfercfgmenu.ypos=6;
		xfercfgmenu.lpadding="\xb3";
		xfercfgmenu.rpadding="\xb3";
		xfercfgmenu.hotkeys=KEY_LEFT+"\b\x7f\x1b";
		xfercfgmenu.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
		xfercfgmenu.add("|Set New Scan Time","S",28);
		xfercfgmenu.add("Toggle |Batch Flag","B",28);
		xfercfgmenu.add("Toggle |Extended Descriptions","E",28);
		xfercfgmenu.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
	settingsmenu.add("|Re-Init Message Pointers","R",24);
	settingsmenu.add("|Toggle Paging","T",24);
	settingsmenu.add("|Activity Alerts On/Off","A",24);
	settingsmenu.add("Minute |Bank","B",24);
	settingsmenu.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
mainbar.add("|Email","E");
	var emailmenu=new Lightbar;
	emailmenu.xpos=17;
	emailmenu.ypos=2;
	emailmenu.lpadding="\xb3";
	emailmenu.rpadding="\xb3";
	emailmenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	emailmenu.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	emailmenu.add("|Send Mail","S",24);
	emailmenu.add("Send |NetMail","N",24);
	emailmenu.add("Send |Feedback to Sysop","F",24);
	emailmenu.add("|Read Mail Sent To You","R",24);
	emailmenu.add("Read Mail |You Have Sent","Y",24);
	emailmenu.add("|Upload File To a Mailbox","U",24);
	emailmenu.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
mainbar.add("|Messages","M");
	var messagemenu=new Lightbar;
	messagemenu.xpos=24;
	messagemenu.ypos=2;
	messagemenu.lpadding="\xb3";
	messagemenu.rpadding="\xb3";
	messagemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	messagemenu.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	messagemenu.add("|New Message Scan","N",24);
	messagemenu.add("|Read Message Prompt","R",24);
	messagemenu.add("|Continuous New Scan","C",24);
	messagemenu.add("|Browse New Scan","B",24);
	messagemenu.add("|QWK Packet Transfer","Q",24);
	messagemenu.add("|Post a Message","P",24);
	messagemenu.add("Post |Auto-Message","A",24);
	messagemenu.add("|Find Text in Messages","F",24);
	messagemenu.add("|Scan For Messages To You","S",24);
	messagemenu.add("|Jump To New Sub-Board","J",24);
	messagemenu.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
mainbar.add("|Chat","C");
	var chatmenu=new Lightbar;
	chatmenu.xpos=34;
	chatmenu.ypos=2;
	chatmenu.lpadding="\xb3";
	chatmenu.rpadding="\xb3";
	chatmenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	chatmenu.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	chatmenu.add("|Join/Initiate Multinode Chat","J",39);
	chatmenu.add("Join/Initiate |Private Node to Node Chat","P",39);
	chatmenu.add("|Chat With The SysOp","C",39);
	chatmenu.add("|Talk With The System Guru","T",39);
	chatmenu.add("|Finger (Query) A Remote User or System","F",39);
	chatmenu.add("I|RC Chat","R",39);
	chatmenu.add("|InterBBS Instant Messages","I",39);
	chatmenu.add("|Toggle Split Screen Private Chat","S",39);
	chatmenu.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
mainbar.add("E|xternals","x");
	// Generate menus of available xtrn sections.
	var xtrnsec=new Lightbar;
	var bars40="\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4";
	xtrnsec.xpos=40;
	xtrnsec.ypos=2;
	xtrnsec.lpadding="\xb3";
	xtrnsec.rpadding="\xb3";
	xtrnsec.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	var xtrnsecs=new Array(xtrn_area.sec_list.length);
	var xtrnsecwidth=0;
	var j;
	var k;
	var xtrnsecprogwidth=0;
	for(j=0; j<xtrn_area.sec_list.length && j<console.screen_rows-2; j++) {
		xtrnsecprogwidth=0;
		if(xtrn_area.sec_list[j].name.length > xtrnsecwidth)
			xtrnsecwidth=xtrn_area.sec_list[j].name.length;
		// Generate the menu for each section
		xtrnsecs[j]=new Lightbar;
		xtrnsecs[j].hotkeys=KEY_RIGHT+"\b\x7f\x1b";
		for(k=0; k<xtrn_area.sec_list[j].prog_list.length; k++) {
			if(xtrn_area.sec_list[j].prog_list[k].name.length > xtrnsecprogwidth)
				xtrnsecprogwidth=xtrn_area.sec_list[j].prog_list[k].name.length;
		}
		if(xtrnsecprogwidth>37)
			xtrnsecprogwidth=37;
		if(xtrn_area.sec_list[j].prog_list.length+3+j <= console.screen_rows)
			xtrnsecs[j].ypos=j+2;
		else
			xtrnsecs[j].ypos=console.screen_rows-k-1;
		xtrnsecs[j].xpos=40-xtrnsecprogwidth-2;
		xtrnsecs[j].lpadding="\xb3";
		xtrnsecs[j].rpadding="\xb3";
		xtrnsecs[j].add("\xda"+bars40.substr(0,xtrnsecprogwidth)+"\xbf",undefined,undefined,"","");
		for(k=0; k<xtrn_area.sec_list[j].prog_list.length && k<console.screen_rows-3; k++)
			xtrnsecs[j].add(xtrn_area.sec_list[j].prog_list[k].name,k.toString(),xtrnsecprogwidth);
		xtrnsecs[j].add("\xc0"+bars40.substr(0,xtrnsecprogwidth)+"\xd9",undefined,undefined,"","");
	}
	xtrnsecwidth += 4;
	if(xtrnsecwidth>37)
		xtrnsecwidth=37;
	xtrnsec.add("\xda"+bars40.substr(0,xtrnsecwidth)+"\xbf",undefined,undefined,"","");
	for(j=0; j<xtrn_area.sec_list.length; j++)
		xtrnsec.add("<-- "+xtrn_area.sec_list[j].name,j.toString(),xtrnsecwidth);
	xtrnsec.add("\xc0"+bars40.substr(0,xtrnsecwidth)+"\xd9",undefined,undefined,"","");
mainbar.add("|Info","I");
	var infomenu=new Lightbar;
	infomenu.xpos=51;
	infomenu.ypos=2;
	infomenu.lpadding="\xb3";
	infomenu.rpadding="\xb3";
	infomenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b";
	infomenu.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	infomenu.add("System |Information","I",25);
	infomenu.add("Synchronet |Version Info","V",25);
	infomenu.add("Info on Current |Sub-Board","S",25);
	infomenu.add("|Your Statistics","Y",25);
	infomenu.add("<-- |User Lists","U",25);
		var userlists=new Lightbar;
		userlists.xpos=37;
		userlists.ypos=6;
		userlists.lpadding="\xb3";
		userlists.rpadding="\xb3";
		userlists.hotkeys=KEY_RIGHT+"\b\x7f\x1b";
		userlists.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
		userlists.add("|Logons Today","L",12);
		userlists.add("|Sub-Board","S",12);
		userlists.add("|All","A",12);
		userlists.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
	infomenu.add("|Text Files","T",25);
	infomenu.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
mainbar.add("|Goodbye","G");
mainbar.add("String Command",";");

draw_main(true);
var next_key='';
while(1) {
	var done=0;
	var key=next_key;
	next_key='';
	draw_main(false);
	if(key=='')
		key=mainbar.getval()
	if(key==KEY_DOWN)
		key=mainbar.items[mainbar.current].retval;
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
				if(str=='?')
					bbs.menu("sysmain");
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
						mainbar.current=mainbar.items.length-1;
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
							str=todo_padfname(str);
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
							mainbar.current++;
							next_key='S';
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
						str=todo_padfname(str);
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
							str=todo_padfname(str);
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
							str=todo_padfname(str);
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
							next_key='E';
							mainbar.current++;
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
									bbs.get_newscantime(bbs.new_file_time);
									draw_main(true);
									settingsmenu.draw();
									break;
								case 'B':
									user.settings ^= USER_BATCHFLAG;
									break;
								case 'E':
									user.settings ^= USER_EXTDESC;
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
						mainbar.current--;
						next_key='F';
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
						if(str==null || str=="")
							break;
						if(str=="Sysop")
							str="1";
						if(str.search(/\@/)!=-1)
							bbs.netmail(str);
						else {
							i=bbs.finduser(str);
							if(i>0)
								bbs.email(i,WM_EMAIL);
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
						if(str==null || str=="")
							break;
						if(str=="Sysop")
							str="1";
						if(str.search(/\@/)!=-1)
							bbs.netmail(str,WM_FILE);
						else {
							i=bbs.finduser(str);
							if(i>0)
								bbs.email(i,WM_EMAIL|WM_FILE);
						}
						draw_main(true);
						break;
					case KEY_RIGHT:
						mainbar.current++;
						done=1;
						next_key='M';
						break;
					case KEY_LEFT:
						mainbar.current--;
						done=1;
						next_key='S';
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
						mainbar.current++;
						done=1;
						next_key='C';
						break;
					case KEY_LEFT:
						mainbar.current--;
						done=1;
						next_key='E';
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
						mainbar.current++;
						done=1;
						next_key='x';
						break;
					case KEY_LEFT:
						mainbar.current--;
						done=1;
						next_key='M';
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
			while(1) {
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
					if(x_prog==KEY_RIGHT)
						break;
					if(x_proc=='\b' || x_prog=='\x7f' || x_prog=='\x1b')
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
							mainbar.current--;
							done=1;
							next_key='x';
							break infoloop;
						}
						// Fall-through
					case 'U':
						userlistloop: while(1) {
							switch(userlists.getval()) {
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
						mainbar.current++;
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
			exit(1);
	}
}

function todo_getfiles(lib, dir)
{
	var path=format("%s%s.ixb", file_area.lib_list[lib].dir_list[dir].data_dir, file_area.lib_list[lib].dir_list[dir].code);
	return(file_size(path)/22);	/* F_IXBSIZE */
}

function todo_padfname(fname) {
	var name='';
	var ext='';

	var dotpos=fname.lastIndexOf('.');
	if(dotpos > -1) {
		name=fname.substr(0,dotpos);
		ext=fname.substr(dotpos+1);
	}
	else {
		name=fname;
	}
	if(name.length > 8) {
		/* Hack... make long specs match */
		name=name.substr(0,7)+'*';
	}
	if(ext.length > 3) {
		/* Hack... make long specs match */
		ext=ext.substr(0,2)+'*';
	}
	return(format("%-8.8s-3.3s",name,ext));
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
