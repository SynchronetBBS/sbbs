// default.js

// Default Command Shell for Synchronet Version 4.00a+

// $Id: classic_shell.js,v 1.17 2019/07/26 02:22:29 rswindell Exp $

// @format.tab-size 4, @format.use-tabs true

//###############################################################################
//# This shell is an imitation of the Version 1c command set/structure	    	#
//#									      			           				    #
//# It also serves as an example of how to not write a complex command shell	#
//# using JavaScript.  Seriously.  Don't look at this.                          #
//#									      									    #
//###############################################################################

//# Tips:
//#
//#	Tabstops should be set to 4 to view/edit this file
//#	If your editor does not support control characters,
//#		use \1 for Ctrl-A codes
//#	All lines starting with // are considered comments and are ignored
//#	Left margins (indents) are not relevant and used only for clarity
//#	Almost everything is not case sensitive with the exception of @-codes

//################################# Begins Here #################################

load("sbbsdefs.js");
load("coldfuncs.js");
bbs.command_str='';	// Clear STR (Contains the EXEC for default.js)
load("str_cmds.js");
var str;

// Set return point for main menu commands (via end_cmd)
main:
while(1) {
	// Display TEXT\MENU\MAIN.* if not in expert mode
	if(!(user.settings & USER_EXPERT)) {
		console.clear();
		bbs.menu("main");
	}

	// Update node status
	bbs.node_action=NODE_MAIN;
	// async

	bbs.main_cmds++;

	// Display main Prompt
	console.print("-c\r\nþ bhMain ncþ h");
	if(bbs.compare_ars("exempt T"))
		console.putmsg("@TUSED@",P_SAVEATR);
	else
		console.putmsg("@TLEFT@",P_SAVEATR);
	console.putmsg(" nc[h@GN@nc] @GRP@ [h@SN@nc] @SUB@: n",P_SAVEATR);

	// Get key (with / extended commands allowed)
	str=get_next_key();

	if(bbs.compare_ars("RIP"))
		console.getlines();

	// Do nothing for control keys and space
	switch(str) {
		case "\001":
		case "\015":
		case "\023":
		case " ":
			continue main;
	}
		
	// Write command to log file
	bbs.log_key(str,true);

	switch(str) {
		case "1":
		case "2":
		case "3":
		case "4":
		case "5":
		case "6":
		case "7":
		case "8":
		case "9":
			bbs.cursub=get_next_num(msg_area.grp_list[bbs.curgrp].sub_list.length,true)-1;
			continue main;

	// Hitting /number changes current group
		case "/1":
		case "/2":
		case "/3":
		case "/4":
		case "/5":
		case "/6":
		case "/7":
		case "/8":
		case "/9":
			bbs.curgrp=get_next_num(msg_area.grp_list.length,true)-1;
			continue main;
	}

	if(!(user.settings & USER_COLDKEYS))
		console.write(str);


	switch(str) {
		case '<':
		case '{':
		case '-':
			if(bbs.cursub==0)
				bbs.cursub=msg_area.grp_list[bbs.curgrp].sub_list.length-1;
			else
				bbs.cursub--;
			continue main;
		case '}':
		case '>':
		case '+':
		case '=':
			if(bbs.cursub==msg_area.grp_list[bbs.curgrp].sub_list.length-1)
				bbs.cursub=0;
			else
				bbs.cursub++;
			continue main;

		case '[':
			if(bbs.curgrp==0)
				bbs.curgrp=msg_area.grp_list.length-1;
			else
				bbs.curgrp--;
			continue main;
		case ']':
			if(bbs.curgrp==msg_area.grp_list.length-1)
				bbs.curgrp=0;
			else
				bbs.curgrp++;
			continue main;

	// String commands start with a semicolon
		case ';':
			// Difference from default.src... do NOT force upper-case!
			// Upper-case pisses of *nix people.
			str=get_next_str("",40,0,false);
			str_cmds(str);
			continue main;

		case 'T':
			if(file_exists(system.text_dir+"menu/tmessage.*"))
				bbs.menu("tmessage");
			file_transfers();
			continue main;
	}

	if(!(user.settings & USER_COLDKEYS))
		console.crlf();

	console.line_counter=0;

	switch(str) {
		case '?':
			if(user.settings & USER_EXPERT)
				bbs.menu("main");
			continue main;
	}

	// Sysop Menu
	if(bbs.compare_ars("SYSOP or EXEMPT Q or I or N")) {
		switch(str) {
			case '!':
				bbs.menu("sysmain");
				continue main;
		}
	}


	// Commands
	switch(str) {
		case 'A':
			bbs.auto_msg();
			continue main;

		case 'B':
			console.print("\r\nchBrowse/New Message Scan\r\n");
			bbs.scan_subs(SCAN_NEW|SCAN_BACK);
			continue main;

		case 'C':
			load("chat_sec.js");
			continue main;

		case 'D':
			bbs.user_config();
			continue main;

		case 'E':
			email();
			continue main;

		case 'F':
			console.print("\r\nchFind Text in Messages\r\n");
			bbs.scan_subs(SCAN_FIND);
			continue main;

		case '/F':
			bbs.scan_subs(SCAN_FIND,true);
			continue main;

		case 'G':
			bbs.text_sec();
			continue main;

		case 'I':
			main_info();
			continue main;

		case 'J':
			if(!msg_area.grp_list.length)
				continue main;
			while(1) {
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
								console.print('*');
							else
								console.print(' ');
							if(i<9)
								console.print(' ');
							if(i<99)
								console.print(' ');
							console.putmsg(format(bbs.text(CfgGrpLstFmt),i+1,msg_area.grp_list[i].description),P_SAVEATR);
						}
					}
					console.mnemonics(format(bbs.text(JoinWhichGrp),bbs.curgrp+1));
					j=get_next_num(msg_area.grp_list.length,false);
					if(j<0)
						continue main;
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
							console.print('*');
						else
							console.print(' ');
						if(i<9)
							console.print(' ');
						if(i<99)
							console.print(' ');
						console.putmsg(format(bbs.text(SubLstFmt),i+1, msg_area.grp_list[j].sub_list[i].description,"",msgbase.total_msgs),P_SAVEATR);
						msgbase.close();
					}
				}
				console.mnemonics(format(bbs.text(JoinWhichSub),bbs.cursub+1));
				i=get_next_num(msg_area.grp_list[j].sub_list.length,false);
				if(i==-1) {
					if(msg_area.grp_list.length==1) {
						bbs.curgrp=orig_grp;
						continue main;
					}
					continue;
				}
				if(!i)
					i=bbs.cursub;
				else
					i--;
				bbs.cursub=i;
				continue main;
			}
			// This never actually happens...
			continue main;

		case '/L':
			bbs.list_nodes();
			continue main;

		case 'M':
			bbs.time_bank();
			continue main;

		case 'N':
			console.print("\r\nchNew Message Scan\r\n");
			bbs.scan_subs(SCAN_NEW);
			continue main;

		case '/N':
			bbs.scan_subs(SCAN_NEW,true);
			continue main;

		case 'O':
			if(bbs.batch_dnload_total) {
				if(console.yesno(bbs.text(DownloadBatchQ))) {
					bbs.batch_download();
					bbs.logoff();
				}
			}
			else
				bbs.logoff();
			continue main;

		case '/O':
			if(bbs.batch_dnload_total) {
				if(console.yesno(bbs.text(DownloadBatchQ))) {
					bbs.batch_download();
					bbs.hangup();
				}
			}
			else
				bbs.hangup();
			continue main;

		case 'P':
			bbs.post_msg();
			continue main;

		case 'Q':
			bbs.qwk_sec();
			continue main;

		case 'R':
			bbs.scan_posts();
			continue main;

		case 'S':
			console.print("\r\nchScan for Messages Posted to You\r\n");
			bbs.scan_subs(SCAN_TOYOU);
			continue main;

		case '/S':
			console.print("\r\nchScan for Messages Posted to You\r\n");
			bbs.scan_subs(SCAN_TOYOU,true);
			continue main;

		case 'U':
			console.print("\r\nchList Users\r\n");
			console.mnemonics("\r\n~Logons Today, ~Sub-board, or ~All: ");
			switch(get_next_keys("LSA",false)) {
				case 'L':
					bbs.list_logons();
					break;
				case 'S':
					bbs.list_users(UL_SUB);
					break;
				case 'A':
					bbs.list_users(UL_ALL);
					break;
			}
			// fall-through for CR, Ctrl-C, etc
			continue main;

		case '/U':
			bbs.list_users(UL_ALL);
			continue main;
			
		case 'X':
			bbs.xtrn_sec();
			continue main;

		case 'Z':
			console.print("\r\nchContinuous New Message Scan\r\n");
			bbs.scan_subs(SCAN_NEW|SCAN_CONST);
			continue main;

		case '/Z':
			bbs.scan_subs(SCAN_NEW|SCAN_CONST,true);
			continue main;

		case '*':
			if(!msg_area.grp_list.length)
				continue main;
			if(file_exists(system.text_dir+"menu/subs"+(bbs.cursub+1)))
				bbs.menu("subs"+(bbs.cursub+1));
			else {
				var i;

				console.clear();
				console.putmsg(format(bbs.text(SubLstHdr), msg_area.grp_list[bbs.curgrp].description),P_SAVEATR);
				for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++) {
					var msgbase=new MsgBase(msg_area.grp_list[bbs.curgrp].sub_list[i].code);
					if(msgbase==undefined)
						continue;
					if(!msgbase.open())
						continue;
					if(i==bbs.cursub)
						console.print('*');
					else
						console.print(' ');
					if(i<9)
						console.print(' ');
					if(i<99)
						console.print(' ');
					console.putmsg(format(bbs.text(SubLstFmt),i+1, msg_area.grp_list[bbs.curgrp].sub_list[i].description,"",msgbase.total_msgs),P_SAVEATR);
					msgbase.close();
				}
			}
			continue main;

		case '/*':
			if(msg_area.grp_list.length) {
				var i=0;
				if(file_exists(system.text_dir+"menu/grps.*"))
					bbs.menu("grps");
				else {
					console.putmsg(bbs.text(GrpLstHdr),P_SAVEATR);
					for(i=0; i<msg_area.grp_list.length; i++) {
						if(i==bbs.curgrp)
							console.print('*');
						else
							console.print(' ');
						if(i<9)
							console.print(' ');
						if(i<99)
							console.print(' ');
						// We use console.putmsg to expand ^A, @, etc
						console.putmsg(format(bbs.text(GrpLstFmt),i+1,msg_area.grp_list[i].description,"",msg_area.grp_list[i].sub_list.length),P_SAVEATR);
					}
				}
			}
			continue main;

		case '&':
			main_cfg();
			continue main;

		case '#':
			console.print("\r\nchType the actual number, not the symbol.\r\n");
			continue main;

		case '/#':
			console.print("\r\nchType the actual number, not the symbol.\r\n");
			continue main;
	}
// fall through
	console.print("\r\nchUnrecognized command.");
	if(user.settings & USER_EXPERT)
		console.print(" Hit 'i?nch' for a menu.");
	console.crlf();
}

// shouldn't hit next line
alert("Problem in command shell.");
console.pause();
bbs.hangup();
exit(1);

//############################### E-mail Section ################################

function email()
{
	var key;
	var i;
	while(1) {
		if(!(user.settings & USER_EXPERT))
			bbs.menu("e-mail");

		// async

		console.print("\r\nyhE-mail: n");
		key=get_next_keys("?SRFNUKQ\r");
		bbs.log_key(key);
		switch(key) {
			case '?':
				if(user.settings & USER_EXPERT)
					bbs.menu("e-mail");
				break;

			case 'S':
				console.print("_\r\nbhE-mail (User name or number): w");
				str=get_next_str("",40,K_UPRLWR,false);
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
				break;

			case 'U':
				console.print("_\r\nbhE-mail (User name or number): w");
				str=get_next_str("",40,K_UPRLWR,false);
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
				break;

			case 'R':
				bbs.read_mail(MAIL_YOUR);
				break;

			case 'F':
				bbs.email(1,WM_EMAIL,bbs.text(ReFeedback));
				break;

			case 'N':
				if(console.noyes("\r\nAttach a file"))
					i=WM_FILE;
				else
					i=0;
				console.putmsg(bbs.text(EnterNetMailAddress),P_SAVEATR);
				str=get_next_str("",60,K_LINE,false);
				if(str!=null && str !="")
					bbs.netmail(str,i);
				break;

			case 'K':
				bbs.read_mail(MAIL_SENT);
				break;

			case 'Q':
			default:
				return;
		}
	}
	return
}

//############################ Main Info Section	###############################

function main_info()
{
	var key;

	while(1) {
		if(!(user.settings & USER_EXPERT))
			bbs.menu("maininfo");

		// async

		console.print("\r\nyhInfo: n");
		key=get_next_keys("?QISVY\r");
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

			case 'Q':
			default:
				return;
		}
	}
}

//########################### Main Config Section  ##############################

function main_cfg()
{
	var key;
	var sub;

	while(1) {
		if(!(user.settings & USER_EXPERT))
			bbs.menu("maincfg");
		
		// async
		console.print("\r\nyhConfig: n");
		key=get_next_keys("?QNPIS\r");
		bbs.log_key(key);

		switch(key) {
			case '?':
				if(user.settings & USER_EXPERT)
					bbs.menu("maincfg");
				break;

			case 'N':
				bbs.cfg_msg_scan(SCAN_CFG_NEW);
				break;

			case 'S':
				bbs.cfg_msg_scan(SCAN_CFG_TOYOU);
				break;

			case 'P':
				bbs.cfg_msg_ptrs();
				break;

			case 'I':
				bbs.reinit_msg_ptrs();
				break;

			default:
				return;
		}
	}
}				


//########################### File Transfer Section #############################

function file_transfers()
{
	var key;

	if(bbs.compare_ars("file_cmds=0")) {
		if(user.settings & USER_ASK_NSCAN) {
			console.crlf();
			console.crlf();
			if(console.yesno("Search all libraries for new files"))
				bbs.scan_dirs(FL_ULTIME, true);
		}
	}

file_transfers:
	while(1) {
		if(!(user.settings & USER_EXPERT)) {
			console.clear();
			bbs.menu("transfer");
		}

		// Update node status
		bbs.node_action=NODE_XFER;

		// async

		bbs.file_cmds++;

		// Display main Prompt
		console.print("-c\r\nþ bhFile ncþ h");
		if(bbs.compare_ars("exempt T"))
			console.putmsg("@TUSED@",P_SAVEATR);
		else
			console.putmsg("@TLEFT@",P_SAVEATR);
		console.putmsg(" nc(h@LN@nc) @LIB@ (h@DN@nc) @DIR@: n",P_SAVEATR);

		// Get key (with / extended commands allowed)
		str=get_next_key();

		if(bbs.compare_ars("RIP"))
			console.getlines();

		// Do nothing for control keys and space
		switch(str) {
			case "\001":
			case "\015":
			case "\023":
			case " ":
				continue file_transfers;
		}

		// Write command to log file
		bbs.log_key(str,true);

		// Hitting number changes current sub-board
		switch(str) {
			case "1":
			case "2":
			case "3":
			case "4":
			case "5":
			case "6":
			case "7":
			case "8":
			case "9":
				bbs.curdir=get_next_num(file_area.lib_list[bbs.curlib].dir_list.length,true);
				continue file_transfers;

		// Hitting /number changes current group
			case "/1":
			case "/2":
			case "/3":
			case "/4":
			case "/5":
			case "/6":
			case "/7":
			case "/8":
			case "/9":
				bbs.curlib=get_next_num(file_area.lib_list.length,true)-1;
				continue file_transfers;
		}

		// Show the key hit
		if(!(user.settings & USER_COLDKEYS))
			printf(str);

		switch(str) {
			case '>':
			case '}':
			case '+':
			case '=':
				if(bbs.curdir>=file_area.lib_list[bbs.curlib].dir_list.length-1)
					bbs.curdir=0;
				else
					bbs.curdir++;
				continue file_transfers;

			
			case '<':
			case '{':
			case '-':
				if(bbs.curdir==0)
					bbs.curdir=file_area.lib_list[bbs.curlib].dir_list.length-1;
				else
					bbs.curdir--;
				continue file_transfers;
				
			case ']':
				if(bbs.curlib == file_area.lib_list.length-1)
					bbs.curlib=0;
				else
					bbs.curlib++;
				continue file_transfers;

			case '[':
				if(bbs.curlib == 0)
					bbs.curlib=file_area.lib_list.length-1;
				else
					bbs.curlib--;
				continue file_transfers;

			// String commands start with a semicolon
			case ';':
				// Difference from default.src... do NOT force upper-case!
				// Upper-case pisses of *nix people.
				str=get_next_str("",40,0,false);
				str_cmds(str);
				continue file_transfers;

			case 'Q':
				return;
		}

		if(!(user.settings & USER_COLDKEYS))
			console.crlf();

		console.line_counter=0;

		// Menu
		switch(str) {
			case '?':
				if(user.settings & USER_EXPERT)
					bbs.menu("transfer");
				continue file_transfers;
		}

		if(bbs.compare_ars("SYSOP")) {
			switch(str) {
				case '!':
					bbs.menu("sysxfer");
					continue file_transfers;
			}
		}
		
		// Commands
		switch(str) {
			case 'B':
				bbs.batch_menu();
				continue file_transfers;

			case 'C':
				load("chat_sec.js");
				continue file_transfers;

			case 'D':
				console.print("\r\nchDownload File(s)\r\n");
				if(bbs.batch_dnload_total>0) {
					if(console.yesno(bbs.text(DownloadBatchQ))) {
						bbs.batch_download();
						continue file_transfers;
					}
				}
				str=bbs.get_filespec();
				if(str==null)
					continue file_transfers;
				if(file_area.lib_list.length==0)
					continue file_transfers;
				if(user.security.restrictions&UFLAG_D) {
					console.putmsg(bbs.text(R_Download),P_SAVEATR);
					continue file_transfers;
				}
				if(!bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FI_DOWNLOAD)) {
					var s=0;
					console.putmsg(bbs.text(SearchingAllDirs),P_SAVEATR);
					for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
						if(i!=bbs.curdir &&
								(s=bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FI_DOWNLOAD))!=0) {
							if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
								continue file_transfers;
							}
						}
					}
					console.putmsg(bbs.text(SearchingAllLibs),P_SAVEATR);
					for(i=0; i<file_area.lib_list.length; i++) {
						if(i==bbs.curlib)
							continue;
						for(j=0; j<file_area.lib_list[i].dir_list.length; j++) {
							if((s=bbs.list_file_info(file_area.lib_list[i].dir_list[j].number, str, FI_DOWNLOAD))!=0) {
								if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
									continue file_transfers;
								}
							}
						}
					}
				}
				continue file_transfers;

			case '/D':
				if(file_area.user_dir==undefined)
					console.putmsg(bbs.text(NoUserDir),P_SAVEATR);
				else {
					if(bbs.compare_ars("rest D"))
						console.putmsg(bbs.text(R_Download),P_SAVEATR);
					else {
						console.crlf();
						if(!bbs.list_file_info(file_area.user_dir, FI_USERXFER))
							console.putmsg(bbs.text(NoFilesForYou),P_SAVEATR);
					}
				}
				continue file_transfers;

			case 'E':
				console.print("\r\nchList Extended File Information\r\n");
				str=bbs.get_filespec();
				if(str==null)
					continue file_transfers;
				if(!bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FI_INFO)) {
					var s=0;
					console.putmsg(bbs.text(SearchingAllDirs),P_SAVEATR);
					for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
						if(i!=bbs.curdir &&
								(s=bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FI_INFO))!=0) {
							if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
								continue file_transfers;
							}
						}
					}
					console.putmsg(bbs.text(SearchingAllLibs),P_SAVEATR);
					for(i=0; i<file_area.lib_list.length; i++) {
						if(i==bbs.curlib)
							continue;
						for(j=0; j<file_area.lib_list[i].dir_list.length; j++) {
							if((s=bbs.list_file_info(file_area.lib_list[i].dir_list[j].number, str, FI_INFO))!=0) {
								if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
									continue file_transfers;
								}
							}
						}
					}
				}
				continue file_transfers;

			case 'F':
				console.print("\r\nchFind Text in File Descriptions (no wildcards)\r\n");
				bbs.scan_dirs(FL_FINDDESC);
				continue file_transfers;

			case '/F':
				bbs.scan_dirs(FL_FINDDESC,true);
				continue file_transfers;
			
			case 'I':
				file_info();
				continue file_transfers;

			case 'J':
				if(!file_area.lib_list.length)
					continue file_transfers;
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
									console.print('*');
								else
									console.print(' ');
								if(i<9)
									console.print(' ');
								if(i<99)
									console.print(' ');
								// We use console.putmsg to expand ^A, @, etc
								console.putmsg(format(bbs.text(CfgLibLstFmt),i+1,file_area.lib_list[i].description),P_SAVEATR);
							}
						}
						console.mnemonics(format(bbs.text(JoinWhichLib),bbs.curlib+1));
						j=get_next_num(file_area.lib_list.length,false);
						if(j<0)
							continue file_transfers;
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
								console.print('*');
							else
								console.print(' ');
							if(i<9)
								console.print(' ');
							if(i<99)
								console.print(' ');
							console.putmsg(format(bbs.text(DirLstFmt),i+1, file_area.lib_list[j].dir_list[i].description,"",todo_getfiles(j,i)),P_SAVEATR);
						}
					}
					console.mnemonics(format(bbs.text(JoinWhichDir),bbs.curdir+1));
					i=get_next_num(file_area.lib_list[j].dir_list.length,false);
					if(i==-1) {
						if(file_area.lib_list.length==1) {
							bbs.curlib=orig_lib;
							continue file_transfers;
						}
						continue;
					}
					if(!i)
						i=bbs.curdir;
					else
						i--;
					bbs.curdir=i;
					continue file_transfers;
				}
				// This never actually happens...
				continue file_transfers;

			case 'L':
				i=bbs.list_files();
				if(i==-1)
					continue file_transfers;
				if(i==0)
					console.putmsg(bbs.text(EmptyDir),P_SAVEATR);
				else
					console.putmsg(format(bbs.text(NFilesListed),i),P_SAVEATR);
				continue file_transfers;

			case '/L':
				bbs.list_nodes();
				continue file_transfers;

			case 'N':
				console.print("\r\nchNew File Scan\r\n");
				bbs.scan_dirs(FL_ULTIME);
				continue file_transfers;

			case '/N':
				bbs.scan_dirs(FL_ULTIME,true);
				continue file_transfers;

			case 'O':
				if(bbs.batch_dnload_total && console.yesno(bbs.text(DownloadBatchQ))) {
					bbs.batch_download();
					bbs.logoff();
				}
				continue file_transfers;

			case '/O':
				if(bbs.batch_dnload_total) {
					if(console.yesno(bbs.text(DownloadBatchQ))) {
						bbs.batch_download();
						bbs.hangup();
					}
				}
				else
					bbs.hangup();
				continue file_transfers;

			case 'R':
				console.print("\r\nchRemove/Edit File(s)\r\n");
				str=bbs.get_filespec();
				if(str==null)
					continue file_transfers;
				if(!bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FI_REMOVE)) {
					var s=0;
					console.putmsg(bbs.text(SearchingAllDirs),P_SAVEATR);
					for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
						if(i!=bbs.curdir &&
								(s=bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FI_REMOVE))!=0) {
							if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
								continue file_transfers;
							}
						}
					}
					console.putmsg(bbs.text(SearchingAllLibs),P_SAVEATR);
					for(i=0; i<file_area.lib_list.length; i++) {
						if(i==bbs.curlib)
							continue;
						for(j=0; j<file_area.lib_list[i].dir_list.length; j++) {
							if((s=bbs.list_file_info(file_area.lib_list[i].dir_list[j].number, str, FI_REMOVE))!=0) {
								if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
									continue file_transfers;
								}
							}
						}
					}
				}
				continue file_transfers;

			case 'S':
				console.print("\r\nchSearch for Filename(s)\r\n");
				bbs.scan_dirs(FL_NO_HDR);
				continue file_transfers;

			case '/S':
				bbs.scan_dirs(FL_NO_HDR,true);
				continue file_transfers;

			case 'T':
				bbs.temp_xfer();
				continue file_transfers;

			case 'U':
				console.print("\r\nchUpload File\r\n");
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
				continue file_transfers;

			case '/U':
				console.print("\r\nchUpload File to User\r\n");
				if(file_area.upload_dir == undefined)
					console.putmsg(bbs.text(NoUserDir),P_SAVEATR);
				else
					bbs.upload_file(file_area.upload_dir.number);
				continue file_transfers;

			case 'V':
				console.print("\r\nchView File(s)\r\n");
				str=bbs.get_filespec();
				if(str==null)
					continue file_transfers;
				if(!bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FL_VIEW)) {
					console.putmsg(bbs.text(SearchingAllDirs),P_SAVEATR);
					for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
						if(i==bbs.curdir)
							continue;
						if(bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FL_VIEW))
							break;
					}
					if(i<file_area.lib_list[bbs.curlib].dir_list.length)
						continue file_transfers;
					console.putmsg(bbs.text(SearchingAllLibs),P_SAVEATR);
					for(i=0; i<file_area.lib_list.length; i++) {
						if(i==bbs.curlib)
							continue;
						for(j=0; j<file_area.lib_list[i].dir_list.length; j++) {
							if(bbs.list_files(file_area.lib_list[i].dir_list[j].number, str, FL_VIEW))
								continue file_transfers;
						}
					}
				}
				continue file_transfers;

			case 'Z':
				console.print("\r\nchUpload File to Sysop\r\n");
				if(file_area.sysop_dir == undefined)
					console.putmsg(bbs.text(NoSysopDir),P_SAVEATR);
				else
					bbs.upload_file(file_area.sysop_dir.number);
				continue file_transfers;

			case '*':
				if(!file_area.lib_list.length)
					continue file_transfers;
				str=format("%smenu/dirs%u.*", system.text_dir, file_area.lib_list[bbs.curlib].number+1);
				if(file_exists(str)) {
					str=format("menu/dirs%u.*", file_area.lib_list[bbs.curlib].number+1);
					bbs.menu(str);
					continue file_transfers;
				}
				console.crlf();
				console.putmsg(format(bbs.text(DirLstHdr),file_area.lib_list[bbs.curlib].description),P_SAVEATR);
				for(i=0;i<file_area.lib_list[bbs.curlib].dir_list.length;i++) {
					if(i==bbs.curdir)
						console.print('*');
					else
						console.print(' ');
					str=format(bbs.text(DirLstFmt),i+1
						,file_area.lib_list[bbs.curlib].dir_list[i].description,""
						,todo_getfiles(bbs.curlib,i));
					if(i<9)
						console.print(' ');
					if(i<99)
						console.print(' ');
					console.putmsg(str,P_SAVEATR);
				}
				continue file_transfers;

			case '/*':
				if(!file_area.lib_list.length)
					continue file_transfers;
				if(file_exists(system.text_dir+'menu/libs.*')) {
					bbs.menu('libs');
					continue file_transfers;
				}
				console.putmsg(bbs.text(LibLstHdr),P_SAVEATR);
				for(i=0;i<file_area.lib_list.length;i++) {
					if(i==bbs.curlib)
						console.print('*');
					else
						console.print(' ');
					if(i<9)
						console.print(' ');
					console.putmsg(format(bbs.text(LibLstFmt),i+1,file_area.lib_list[i].description,"",file_area.lib_list[i].dir_list.length),P_SAVEATR);
				}
				continue file_transfers;

			case '&':
xfercfg:
				while(1) {
					if(!(user.settings & USER_EXPERT))
						bbs.menu("xfercfg");
					// async
					console.print("\r\nyhConfig: n");
					key=get_next_keys("?QBEP\r");
					bbs.log_key(key);
					switch(key) {
						case '?':
							if(user.settings & USER_EXPERT)
								bbs.menu("xfercfg");
							break;

						case 'P':
							bbs.new_file_time=bbs.get_newscantime(bbs.new_file_time);
							break;

						case 'B':
							user.settings ^= USER_BATCHFLAG;
							break;

						case 'E':
							user.settings ^= USER_EXTDESC;
							break;

						default:
							break xfercfg;
					}
				}
				continue file_transfers;

			case '#':
				console.print("\r\nchType the actual number, not the symbol.\r\n");
				continue file_transfers;

			case '/#':
				console.print("\r\nchType the actual number, not the symbol.\r\n");
				continue file_transfers;

			default:
				// fall through
				console.print("\r\nchUnrecognized command.");
				if(user.settings & USER_EXPERT)
					console.print(" Hit 'i?nch' for a menu.");
		}
		console.crlf();
	}
	// shouldn't hit next line
	alert("Problem in command shell.");
	console.pause();
	bbs.hangup();
}
	
//############################ File Info Section	###############################

function file_info()
{
	var key;

	while(1) {
		if(!(user.settings & USER_EXPERT))
			bbs.menu("xferinfo");

		// async
		console.print("\r\nyhInfo: n");
		key=get_next_keys("?TYDUQ\r");
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

//############################ Nasty Hacks #######################
function todo_getfiles(lib, dir)
{
	var path=format("%s%s.ixb", file_area.lib_list[lib].dir_list[dir].data_dir, file_area.lib_list[lib].dir_list[dir].code);
	return(file_size(path)/22);	/* F_IXBSIZE */
}
//end of DEFAULT.JS
