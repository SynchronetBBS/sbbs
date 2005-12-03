// default.js

// Default Command Shell for Synchronet Version 4.00a+

// $Id$

// @format.tab-size 4, @format.use-tabs true

//###############################################################################
//# This shell is an imitation of the Version 1c command set/structure	    	#
//#									      			           				    #
//# It also serves as an example of a complex command shell using JavaScript    #
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
load("str_cmds.js");
bbs.command_str='';	// Clear STR (Contains the EXEC for default.js)
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
	console.putmsg("-c\r\nþ bhMain ncþ h",P_SAVEATR);
	if(user.compare_ars("exempt T"))
		console.putmsg("@TUSED@");
	else
		console.putmsg("@TLEFT@");
	console.putmsg(" nc[h@GN@nc] @GRP@ [h@SN@nc] @SUB@: n");

	// Get key (with / extended commands allowed)
	str=get_next_key();

	if(user.compare_ars("RIP"))
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
	if(user.compare_ars("SYSOP or EXEMPT Q or I or N")) {
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
			console.putmsg("\r\nchBrowse/New Message Scan\r\n");
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
			console.putmsg("\r\nchFind Text in Messages\r\n");
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
								console.putmsg('*');
							else
								console.putmsg(' ');
							if(i<9)
								console.putmsg(' ');
							if(i<99)
								console.putmsg(' ');
							// We use console.putmsg to expand ^A, @, etc
							console.putmsg(format(bbs.text(CfgGrpLstFmt),i+1,msg_area.grp_list[i].description));
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
							console.putmsg('*');
						else
							console.putmsg(' ');
						if(i<9)
							console.putmsg(' ');
						if(i<99)
							console.putmsg(' ');
						console.putmsg(format(bbs.text(SubLstFmt),i+1, msg_area.grp_list[j].sub_list[i].description,"",msgbase.total_msgs));
						msgbase.close();
					}
				}
				console.mnemonics(format(bbs.text(JoinWhichSub),bbs.cursub+1));
				i=get_next_num(msg_area.grp_list[j].sub_list.length,false);
				if(i==-1) {
					if(msg_area.grm_list.length==1) {
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
			console.putmsg("\r\nchNew Message Scan\r\n");
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
			console.putmsg("\r\nchScan for Messages Posted to You\r\n");
			bbs.scan_subs(SCAN_TOYOU);
			continue main;

		case '/S':
			console.putmsg("\r\nchScan for Messages Posted to You\r\n");
			bbs.scan_subs(SCAN_TOYOU,true);
			continue main;

		case 'U':
			console.putmsg("\r\nchList Users\r\n");
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
			console.putmsg("\r\nchContinuous New Message Scan\r\n");
			bbs.scan_subs(SCAN_NEW|SCAN_CONST);
			continue main;

		case '/Z':
			bbs.scan_subs(SCAN_NEW|SCAN_CONST,true);
			continue main;

		case '*':
			// ToDo... see line 209 in execmsg.cpp
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
						console.putmsg('*');
					else
						console.putmsg(' ');
					if(i<9)
						console.putmsg(' ');
					if(i<99)
						console.putmsg(' ');
					console.putmsg(format(bbs.text(SubLstFmt),i+1, msg_area.grp_list[bbs.curgrp].sub_list[i].description,"",msgbase.total_msgs));
					msgbase.close();
				}
			}
			continue main;

		case '/*':
			// ToDo... see line 193 in execmsg.cpp
			if(msg_area.grp_list.length) {
				var i=0;
				if(file_exists(system.text_dir+"menu/grps.*"))
					bbs.menu("grps");
				else {
					console.putmsg(bbs.text(GrpLstHdr),P_SAVEATR);
					for(i=0; i<msg_area.grp_list.length; i++) {
						if(i==bbs.curgrp)
							console.putmsg('*');
						else
							console.putmsg(' ');
						if(i<9)
							console.putmsg(' ');
						if(i<99)
							console.putmsg(' ');
						// We use console.putmsg to expand ^A, @, etc
						console.putmsg(format(bbs.text(GrpLstFmt),i+1,msg_area.grp_list[i].description,"",msg_area.grp_list[i].sub_list.length));
					}
				}
			}
			continue main;

		case '&':
			main_cfg();
			continue main;

		case '#':
			console.putmsg("\r\nchType the actual number, not the symbol.\r\n");
			continue main;

		case '/#':
			console.putmsg("\r\nchType the actual number, not the symbol.\r\n");
			continue main;
	}
// fall through
	console.putmsg("\r\nchUnrecognized command.",P_SAVEATR);
	if(user.settings & USER_EXPERT)
		console.putmsg(" Hit 'i?nch' for a menu.");
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

		console.putmsg("\r\nyhE-mail: n");
		key=get_next_keys("?SRFNUKQ\r");
		bbs.log_key(key);
		switch(key) {
			case '?':
				if(user.settings & USER_EXPERT)
					bbs.menu("e-mail");
				break;

			case 'S':
				console.putmsg("_\r\nbhE-mail (User name or number): w");
				str=get_next_str("",40,K_UPRLWR,false);
				if(str==null || str=="")
					break;
				if(str=="Sysop")
					str="1";
				if(str.search(/\@/)!=-1)
					bbs.netmail(str);
				else {
					i=finduser(str);
					if(i>0)
						bbs.email(i,WM_EMAIL);
				}
				break;

			case 'U':
				console.putmsg("_\r\nbhE-mail (User name or number): w");
				str=get_next_str("",40,K_UPRLWR,false);
				if(str==null || str=="")
					break;
				if(str=="Sysop")
					str="1";
				if(str.search(/\@/)!=-1)
					bbs.netmail(str,WM_FILE);
				else {
					i=finduser(str);
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

		console.putmsg("\r\nyhInfo: n");
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
		console.putmsg("\r\nyhConfig: n");
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

	if(user.compare_ars("file_cmds=0")) {
		if(user.settings & USER_ASK_NSCAN) {
			console.crlf();
			console.crlf();
			if(console.yesno("Search all libraries for new files"))
				bbs.scan_dirs(FL_ULTIME, true);
		}
	}

file_transfers:
	while(1) {
		if(user.settings & USER_EXPERT) {
			console.clear();
			bbs.menu("transfer");
		}

		// Update node status
		bbs.node_action=NODE_XFER;

		// async

		bbs.file_cmds++;

		// Display main Prompt
		console.putmsg("-c\r\nþ bhFile ncþ h",P_SAVEATR);
		if(user.compare_ars("exempt T"))
			console.putmsg("@TUSED@");
		else
			console.putmsg("@TLEFT@");
		console.putmsg(" nc(h@LN@nc) @LIB@ (h@DN@nc) @DIR@: n");

		// Get key (with / extended commands allowed)
		str=get_next_key();

		if(user.compare_ars("RIP"))
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
				bbs.curdir++;
				if(bbs.curdir>=file_area.lib_list[bbs.cur_lib].dir_list.length)
					bbs.curdir=0;
				continue file_transfers;

			
			case '<':
			case '{':
			case '-':
				if(bbs.curdir<=0)
					bbs.curdir=file_area.lib_list[bbs.cur_lib].dir_list.length;
				bbs.curdir--;
				continue file_transfers;
				
			case ']':
				bbs.curlib++;
				if(bbs.curlib >= file_area.lib_list.length)
					bbs.curlib=0;
				continue file_transfers;

			case '[':
				if(bbs.curlib <= 0)
					bbs.curlib=file_area.lib_list.length;
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

		if(user.compare_ars("SYSOP")) {
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
				console.putmsg("\r\nchDownload File(s)\r\n");
				if(bbs.batch_dnload_total>0) {
					if(console.yesno(bbs.text(DownloadBatchQ))) {
						bbs.batch_download();
					}
				}
				/* ToDo
					getfilespec
					if_true
						file_download	// See line 290 in execfile.cpp
						end_if
				*/
				continue file_transfers;

			case '/D':
				/* ToDo: file_download_user
				   see line 312 in execfile.cpp */
				continue file_transfers;

			case 'E':
				console.putmsg("\r\nchList Extended File Information\r\n");
				str=bbs.get_filespec();
				if(str==null)
					continue file_transfers;
				str=str.replace(/^(.*)(\..*)?$/,
					function(s, p1, p2, oset, s) {
						if(p2==undefined)
							return(format("%-8.8s    ",p1));
						return(format("%-8.8s%-4.4s",p1,p2));
					}
				);
				/* ToDo
					file_list_extended
					See execfile.cpp:406 */
				continue file_transfers;

			case 'F':
				console.putmsg("\r\nchFind Text in File Descriptions (no wildcards)\r\n");
				bbs.scan_dirs(FL_FINDDESC);
				continue file_transfers;

			case '/F':
				bbs.scan_dirs(FL_FINDDESC,true);
				continue file_transfers;
			
			case 'I':
				file_info();
				continue file_transfers;

			case 'J':
				/* ToDo: execfile.cpp:50 */
				continue file_transfers;

			case 'L':
				i=bbs.list_files();
				if(i==-1)
					continue file_transfers;
				if(i==0)
					console.putmsg(bbs.text(EmptyDir),P_SAVEATR);
				else
					console.putmsg(bbs.text(NFilesListed,i),P_SAVEATR);
				continue file_transfers;

			case '/L':
				bbs.list_nodes();
				continue file_transfers;

			case 'N':
				console.putmsg("\r\nchNew File Scan\r\n");
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
				if(bbs.batch_dnload_total && console.yesno(bbs.text(DownloadBatchQ))) {
					bbs.batch_download();
					bbs.logout();
				}
				continue file_transfers;

			case 'R':
				console.putmsg("\r\nchRemove/Edit File(s)\r\n");
				/* ToDo: execfile.cpp:452
				getfilespec
				if_true
					file_remove
					end_if */
				continue file_transfers;

			case 'S':
				console.putmsg("\r\nchSearch for Filename(s)\r\n");
				bbs.scan_dirs(FL_NO_HDR);
				continue file_transfers;

			case '/S':
				bbs.scan_dirs(FL_NO_HDR,true);
				continue file_transfers;

			case 'T':
				bbs.temp_xfer();
				continue file_transfers;

			case 'U':
				console.putmsg("\r\nchUpload File\r\n");
				if(file_exists(system.text_dir+"menu/upload.*"))
					bbs.menu("upload");
				if(file_area.lib_list.length) {
					if(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload)
						i=file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number;
					/* else  ToDo: cfg.upload_dir not available to JS */
				}
				/* else  ToDo: cfg.upload_dir not available to JS */
				bbs.upload_file(i);
				continue file_transfers;

			case '/U':
				console.putmsg("\r\nchUpload File to User\r\n");
				/* ToDo: file_upload_user
				   (user_dir not available) */
				continue file_transfers;

			case 'V':
				console.putmsg("\r\nchView File(s)\r\n");
				/* ToDo execfile.cpp:370
				getfilespec
				if_true
					file_view
					end_if
				end_cmd */
				continue file_transfers;

			case 'Z':
				console.putmsg("\r\nchUpload File to Sysop\r\n");
				/* ToDo: sysop_dir not available 
				file_upload_sysop */
				continue file_transfers;

			case '*':
				if(!file_areas.lib_list.length)
					continue file_transfers;
				str=format("%smenu/dirs%u.*", system.text_dir, file_areas.lib_list[bbs.curlib].number+1);
				if(file_exist(str)) {
					str=format("menu/dirs%u.*", file_areas.lib_list[bbs.curlib].number+1);
					bbs.menu(str);
					continue file_transfers;
				}
				console.crlf();
				console.putmsg(format(bbs.text(DirLstHdr),file_area.lib_list[bbs.curlib].name),P_SAVEATR);
				for(i=0;i<file_area.lib_list[bbs.curlib].dir_list.length;i++) {
					if(i==bbs.curdir)
						outchar('*');
					else
						outchar(' ');
					str=format(bbs.text(DirLstFmt),i+1
						,file_area.lib_list[bbs.curlib].dir_list[i].name,""
						/* ToDo: getfiles not implemented */
						,-1);
					if(i<9)
						console.putmsg(' ');
					if(i<99)
						console.putmsg(' ');
					console.putmsg(str);
				}
				continue file_transfers;

			case '*':
				/* ToDo: execfile.cpp:200
					file_show_directories
					end_cmd	*/
				continue file_transfers;

			case '/*':
				/* ToDo execfile.cpp:184
					file_show_libraries
					end_cmd */
				continue file_transfers;

			case '&':
xfercfg:
				while(1) {
					if(!(user.settings & USER_EXPERT))
						bbs.menu(xfercfg);
					// async
					console.putmsg("\r\nyhConfig: n",P_SAVEATR);
					key=get_next_keys("?QBEP\r");
					bbs.log_key(key);
					switch(key) {
						case '?':
							if(user.settings & USER_EXPERT)
								bbs.menu("xfercfg");
							break;

						case 'P':
							bbs.get_newscantime(bbs.new_file_time);
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
				console.putmsg("\r\nchType the actual number, not the symbol.\r\n");
				continue file_transfers;

			case '/#':
				console.putmsg("\r\nchType the actual number, not the symbol.\r\n");
				continue file_transfers;

			default:
				// fall through
				console.putmsg("\r\nchUnrecognized command.",P_SAVEATR);
				if(user.settings & USER_EXPERT)
					console.putmsg(" Hit 'i?nch' for a menu.");
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
		console.putmsg("\r\nyhInfo: n");
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
//end of DEFAULT.JS
