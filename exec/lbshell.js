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

var mainbar=new Lightbar;
mainbar.direction=1;
mainbar.xpos=1;
mainbar.ypos=1;
mainbar.add("|File","F");
	var filemenu=new Lightbar;
	filemenu.xpos=1;
	filemenu.ypos=1;
	filemenu.add("|File","-");
	filemenu.add(" |Batch Download","B",21);
	filemenu.add(" |Download","D",21);
	filemenu.add(" File |Info","I",21);
		var fileinfo=new Lightbar;
		fileinfo.xpos=22;
		fileinfo.ypos=4;
		fileinfo.add(" <---","-",34);
		fileinfo.add(" File |Transfer Policy","T",34);
		fileinfo.add(" Information on Current |Directory","D",34);
		fileinfo.add(" |Users With Access to Current Dir","U",34);
		fileinfo.add(" |Your File Transfer Statistics","Y",34);
	filemenu.add(" |Extended File Info","E",21);
	filemenu.add(" |Search Descriptions","S",21);
	filemenu.add(" Search |Filenames","F",21);
	filemenu.add(" |Change Directory","C",21);
	filemenu.add(" |List files","L",21);
	filemenu.add(" |New File Scan","N",21);
	filemenu.add(" |Remove/Edit File","R",21);
	filemenu.add(" |Upload File","U",21);
	filemenu.add(" |View File","V",21);
mainbar.add("|Settings","S");
	var settingsmenu=new Lightbar;
	settingsmenu.xpos=6;
	settingsmenu.ypos=1;
	settingsmenu.add("|Settings","-");
	settingsmenu.add(" |User Config","U",27);
	settingsmenu.add(" |Message Scan Config","M",27);
	settingsmenu.add(" To |You Scan Config","C",27);
	settingsmenu.add(" Message |Pointers","P",27);
	settingsmenu.add(" |File Xfer Config","F",27);
	settingsmenu.add(" |Re-Init Message Pointers","R",27);
	settingsmenu.add(" |Toggle Paging","T",27);
	settingsmenu.add(" |Activity Alerts On/Off","A",27);
mainbar.add("|Email","E");
	var emailmenu=new Lightbar;
	emailmenu.xpos=15;
	emailmenu.ypos=1;
	emailmenu.add("|Email","-");
	emailmenu.add(" |Send Mail","S",27);
	emailmenu.add(" Send |NetMail","N",27);
	emailmenu.add(" Send |Feedback to Sysop","F",27);
	emailmenu.add(" |Read Mail Sent To You","R",27);
	emailmenu.add(" Read Mail |You Have Sent","Y",27);
	emailmenu.add(" |Upload File To a Mailbox","U",27);
mainbar.add("|Messages","M");
	var messagemenu=new Lightbar;
	messagemenu.xpos=21;
	messagemenu.ypos=1;
	messagemenu.add("|Messages","-");
	messagemenu.add(" |New Message Scan","N",27);
	messagemenu.add(" |Read Message Prompt","R",27);
	messagemenu.add(" |Continuous New Scan","C",27);
	messagemenu.add(" |Browse New Scan","B",27);
	messagemenu.add(" |QWK Packet Transfer","Q",27);
	messagemenu.add(" |Post a Message","P",27);
	messagemenu.add(" Post |Auto-Message","A",27);
	messagemenu.add(" |Find Text in Messages","F",27);
	messagemenu.add(" |Scan For Messages To You","S",27);
	messagemenu.add(" |Jump To New Sub-Board","J",27);
mainbar.add("|Chat","C");
	var chatmenu=new Lightbar;
	chatmenu.xpos=30;
	chatmenu.ypos=1;
	chatmenu.add("|Chat","-");
	chatmenu.add(" |Join/Initiate Multinode Chat","J",42);
	chatmenu.add(" Join/Initiate |Private Node to Node Chat","P",42);
	chatmenu.add(" |Chat With The SysOp","C",42);
	chatmenu.add(" |Talk With The System Guru","T",42);
	chatmenu.add(" |Finger (Query) A Remote User or System","F",42);
	chatmenu.add(" I|RC Chat","R",42);
	chatmenu.add(" |InterBBS Instant Messages","I",42);
	chatmenu.add(" |Split Screen Private Chat","S",42);
mainbar.add("E|xternals","X");
mainbar.add("|Goodbye","G");

while(1) {
	var done=0;
	console.attributes=7;
	console.clear();
	console.attributes=0x17;
	console.clearline();
	switch(mainbar.getval()) {
		case 'F':
			done=0;
			while(!done) {
				file: switch(filemenu.getval()) {
					case '-':
						done=1;
						break;
					case 'B':
						console.attributes=7;
						console.clear();
						bbs.batch_menu();
						console.attributes=7;
						console.clear();
						console.attributes=0x17;
						console.clearline();
						mainbar.draw();
						filemenu.draw();
						break;
					case 'D':
						download: do {
							console.attributes=7;
							console.clear();
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
						console.attributes=7;
						console.clear();
						console.attributes=0x17;
						console.clearline();
						mainbar.draw();
						filemenu.draw();
						break;
					case 'I':
						var info_done=0;
						while(!info_done) {
							switch(fileinfo.getval()) {
								case 'T':
									console.attributes=7;
									console.clear();
									bbs.xfer_policy();
									break;
								case 'Y':
									console.attributes=7;
									console.clear();
									bbs.user_info();
									break;
								case 'D':
									console.attributes=7;
									console.clear();
									bbs.dir_info();
									break;
								case 'U':
									console.attributes=7;
									console.clear();
									bbs.list_users(UL_DIR);
									break;
								case '-':
									info_done=1;
									break;
							}
							if(info_done) {
								console.attributes=7;
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
							}
							else {
								console.attributes=7;
								console.clear();
								console.attributes=0x17;
								console.clearline();
								mainbar.draw();
								filemenu.draw();
								fileinfo.draw();
							}
						}
						break;
					case 'E':
						console.attributes=7;
						console.clear();
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
						console.attributes=7;
						console.clear();
						console.attributes=0x17;
						console.clearline();
						mainbar.draw();
						filemenu.draw();
						break;
					case 'S':
						console.attributes=7;
						console.clear();
						console.putmsg("\r\nchFind Text in File Descriptions (no wildcards)\r\n");
						bbs.scan_dirs(FL_FINDDESC);
						console.attributes=7;
						console.clear();
						console.attributes=0x17;
						console.clearline();
						mainbar.draw();
						filemenu.draw();
						break;
					case 'F':
						console.attributes=7;
						console.clear();
						console.putmsg("\r\nchSearch for Filename(s)\r\n");
						bbs.scan_dirs(FL_NO_HDR);
						console.attributes=7;
						console.clear();
						console.attributes=0x17;
						console.clearline();
						mainbar.draw();
						filemenu.draw();
						break;
					case 'C':
						console.attributes=7;
						console.clear();
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
						console.attributes=7;
						console.clear();
						console.attributes=0x17;
						console.clearline();
						mainbar.draw();
						filemenu.draw();
						break;
					case 'L':
						console.attributes=7;
						console.clear();
						i=bbs.list_files();
						if(i!=-1) {
							if(i==0)
								console.putmsg(bbs.text(EmptyDir),P_SAVEATR);
							else
								console.putmsg(bbs.text(NFilesListed,i),P_SAVEATR);
						}
						console.attributes=7;
						console.clear();
						console.attributes=0x17;
						console.clearline();
						mainbar.draw();
						filemenu.draw();
						break;
					case 'N':
						console.attributes=7;
						console.clear();
						console.putmsg("\r\nchNew File Scan\r\n");
						bbs.scan_dirs(FL_ULTIME);
						console.attributes=7;
						console.clear();
						console.attributes=0x17;
						console.clearline();
						mainbar.draw();
						filemenu.draw();
						break;
					case 'R':
						console.attributes=7;
						console.clear();
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
						console.attributes=7;
						console.clear();
						console.attributes=0x17;
						console.clearline();
						mainbar.draw();
						filemenu.draw();
						break;
					case 'U':
						console.attributes=7;
						console.clear();
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
						console.attributes=7;
						console.clear();
						console.attributes=0x17;
						console.clearline();
						mainbar.draw();
						filemenu.draw();
						break;
					case 'V':
						console.attributes=7;
						console.clear();
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
						console.attributes=7;
						console.clear();
						console.attributes=0x17;
						console.clearline();
						mainbar.draw();
						filemenu.draw();
						break;
				}
			}
			break;
		case 'S':
			done=0;
			while(!done) {
				switch(settingsmenu.getval()) {
					case '-':
						done=1;
						break;
				}
			}
			break;
		case 'E':
			done=0;
			while(!done) {
				switch(emailmenu.getval()) {
					case '-':
						done=1;
						break;
				}
			}
			break;
		case 'M':
			done=0;
			while(!done) {
				switch(messagemenu.getval()) {
					case '-':
						done=1;
						break;
				}
			}
			break;
		case 'C':
			done=0;
			while(!done) {
				switch(chatmenu.getval()) {
					case '-':
						done=1;
						break;
				}
			}
			break;
		case 'X':
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
