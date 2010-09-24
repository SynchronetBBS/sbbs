/* MENU OBJECTS */
menuobj=[];
menuobj["main"]=function() {
	this.title="MAIN";
	this.items=new Array();
	this.addcmd("Files","F",user.compare_ars("REST T"));
	this.addcmd("Messages","M");
	this.addcmd("Email","E",user.compare_ars("REST SE"));
	this.addcmd("Chat","C",user.compare_ars("REST C"));
	this.addcmd("Settings","S");
	this.addcmd("Externals","X",user.compare_ars("REST X"));
	this.addcmd("View","V");
	this.node_action=NODE_MAIN;
	set_hotkeys(this);
	fill_menu(this);
}
menuobj["favorites"]=function() {
	this.title="FAVORITES";
	this.items=new Array();
	this.addcmd(" type +/- from any menu",undefined,true);
	this.addcmd(" to add/remove favorites",undefined,true);
	this.addcmd("",undefined,true);
	for (var f=0;f<favorites.items.length;f++) {
		if(favorites.items[f].itemID >= 0) {
			this.addcmd(favorites.items[f].itemTitle,f);
		}
		else this.addcmd(favorites.items[f].menuTitle,f);
	}
	set_hotkeys(this);
	fill_menu(this);
	this.node_action=NODE_MAIN;
}
menuobj["delfavorite"]=function() {
	this.title="EDIT FAVORITES";
	this.items=new Array();
	this.addcmd(" choose item to remove",undefined,true);
	this.addcmd(" from your favorites",undefined,true);
	this.addcmd("",undefined,true);
	for (var f=0;f<favorites.items.length;f++) {
		if(favorites.items[f].itemID >= 0) {
			this.addcmd(favorites.items[f].itemTitle,f);
		}
		else this.addcmd(favorites.items[f].menuTitle,f);
	}
	set_hotkeys(this);
	fill_menu(this);
}
menuobj["addfavorite"]=function() {
	this.title="ADD FAVORITE";
	this.items=new Array();
	this.addcmd("Add Menu","M");
	this.addcmd(favorites.mt,undefined,true);
	this.addcmd("",undefined,true);
	this.addcmd("Add Program","P");
	this.addcmd(favorites.it,undefined,true);
	set_hotkeys(this);
	fill_menu(this);
}
menuobj["file"]=function() {
	this.title="FILES";
	this.items=new Array();
	this.addcmd("Change Directory","C");
	this.addcmd("List Files","L");
	this.addcmd("Scan for New Files","N");
	this.addcmd("Search Filenames","F");
	this.addcmd("Search Text in Desc.","T");
	this.addcmd("Download file(s)","D",user.compare_ars("REST D")
			|| (!file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_download));
	this.addcmd("Upload file(s)","U",user.compare_ars("REST U")
			|| ((!file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload)
			&& file_area.upload_dir==undefined));
	this.addcmd("Remove/Edit Files","R");
	this.addcmd("View/Edit Batch","B"
		// Disabled if you can't upload or download.
		// Disabled if no upload dir and no batch queue
		,(user.compare_ars("REST U AND REST D"))
			|| (bbs.batch_upload_total <= 0  
				&& bbs.batch_dnload_total <= 0 
				&& file_area.upload_dir==undefined));
	this.addcmd("View","V");
	this.addcmd("Settings","S");
	set_hotkeys(this);
	fill_menu(this);
	this.node_action=NODE_XFER;
}
menuobj["filedir"]=function() {
	this.title="FILES";
	this.items=new Array();
	this.addcmd("All File Areas","A");
	this.addcmd("Library","L");
	this.addcmd("Directory","D");
	this.addcmd("Chg New Scan Date","N");
	set_hotkeys(this);
	fill_menu(this);
	this.node_action=NODE_XFER;
}
menuobj["settings"]=function() {
	this.title="SETTINGS";
	this.items=new Array();
	this.addcmd("User Configuration","U");
	this.addcmd("Minute Bank","B");
	set_hotkeys(this);
	fill_menu(this);
	this.node_action=NODE_DFLT;
}
menuobj["email"]=function() {
	this.title="E-MAIL";
	this.items=new Array();
	this.addcmd("Send Mail","S");
	this.addcmd("Read Inbox","R");
	this.addcmd("Read Sent Messages","M",user.compare_ars("REST K"));
	set_hotkeys(this);
	fill_menu(this);
}
menuobj["message"]=function() {
	this.title="MESSAGES : " + msg_area.grp_list[bbs.curgrp].name.toUpperCase();
	this.items=new Array();
	this.addcmd("Change Sub","C");
	this.addcmd("Read Messages","R");
	this.addcmd("Scan New Messages","N");
	this.addcmd("Scan Messages To You","Y");
	this.addcmd("Search Message Text","T");
	this.addcmd("Post Message","P",user.compare_ars("REST P"));
	if(user.compare_ars("REST N") && (msg_area.grp_list[bbs.curgrp].sub_list[bbs.crusub] & (SUB_QNET|SUB_PNET|SUB_FIDO)))
		this.items[7].disabed=true;
	this.addcmd("Read/Post Auto-Msg","A");
	this.addcmd("QWK Transfer Menu","Q");
	this.addcmd("View Sub Info","V");
	set_hotkeys(this);
	fill_menu(this);
}
menuobj["chat"]=function() {
	this.title="CHAT";
	this.items=new Array();
	this.addcmd("Multinode Chat","M");
	this.addcmd("Private Chat","P");
	this.addcmd("Chat With The SysOp","C");
	this.addcmd("Chat With Guru","T");
	this.addcmd("Finger User/System","F");
	this.addcmd("IRC Chat","R");
	this.addcmd("Instant Messages","I");
	this.addcmd("Settings","S");
	set_hotkeys(this);
	fill_menu(this);
	this.node_action=NODE_CHAT;
}
menuobj["xtrnsecs"]=function() {
	this.title="EXTERNAL PROGRAMS";
	this.items=new Array();
	for(j=0; j<xtrn_area.sec_list.length; j++)
		this.addcmd(xtrn_area.sec_list[j].name,j.toString());
	set_hotkeys(this);
	fill_menu(this);
	this.node_action=NODE_XTRN;
}
menuobj["xtrnsec"]=function() {
	this.title="XTRN : " + xtrn_area.sec_list[left.xtrnsec].name;
	this.items=new Array();
	for(j=0; j<xtrn_area.sec_list[left.xtrnsec].prog_list.length && j<console.screen_rows-3; j++)
		this.addcmd(xtrn_area.sec_list[left.xtrnsec].prog_list[j].name,j.toString());
	set_hotkeys(this);
	fill_menu(this);
	this.node_action=NODE_XTRN;
}
menuobj["info"]=function() {
	this.title="INFO";
	this.items=new Array();
	this.addcmd("System Info","I");
	this.addcmd("Synchronet Version","V");
	this.addcmd("Info on Sub-Board","S");
	this.addcmd("Your Statistics","Y");
	this.addcmd("User Lists","U");
	this.addcmd("Text Files","T");
	set_hotkeys(this);
	fill_menu(this);
}
menuobj["userlist"]=function() {
	this.title="USERS";
	this.items=new Array();
	this.addcmd("Logons Today","L");
	this.addcmd("Sub-Board","S");
	this.addcmd("All","A");
	set_hotkeys(this);
	fill_menu(this);
}
menuobj["emailtarget"]=function() {
	this.title="E-MAIL";
	this.info=system.text_dir + "cshell/email.ini";
	this.items=new Array();
	this.addcmd('Sysop','S',user.compare_ars("REST S"));
	this.addcmd('Local User','L',user.compare_ars("REST E"));
	this.addcmd('To Local User w/Attach','A',user.compare_ars("REST E"));
	this.addcmd('To Remote User','R',user.compare_ars("REST E OR REST M"));
	this.addcmd('To Remote User w/Attach','T',user.compare_ars("REST E OR REST M"));
	set_hotkeys(this);
	fill_menu(this);
}
menuobj["download"]=function() {
	this.title="DOWNLOAD";
	this.items=new Array();
	this.addcmd('Batch','B',bbs.batch_dnload_total<=0);
	this.addcmd('By Name/File spec','N');
	this.addcmd('From User','U');
	set_hotkeys(this);
	fill_menu(this);
	this.node_action=NODE_XFER;
}
menuobj["upload"]=function() {
	this.title="UPLOAD";
	this.items=new Array();
	if(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload || file_area.upload_dir==undefined) {
		this.addcmd('To Current Dir','C',!file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload);
	}
	else {
		this.addcmd('To Upload Dir','P');
	}
	this.addcmd('To Sysop','S',file_area.sysop_dir==undefined);
	this.addcmd('To User(s)','U',file_area.user_dir==undefined);
	set_hotkeys(this);
	fill_menu(this);
	this.node_action=NODE_XFER;
}
menuobj["fileinfo"]=function() {
	this.title="FILES";
	this.items=new Array();
	this.addcmd('File Contents','C');
	this.addcmd('File Information','I');
	this.addcmd('File Transfer Policy','P');
	this.addcmd('Directory Info','D');
	this.addcmd('Users with Access to Dir','U');
	this.addcmd('Your File Transfer Statistics','S');
	set_hotkeys(this);
	fill_menu(this);
	this.node_action=NODE_XFER;
}
menuobj["filesettings"]=function() {
	this.title="FILE SETTINGS";
	this.items=new Array();
	this.addcmd('Set Batch Flagging '+(user.settings&USER_BATCHFLAG?'Off':'On'),'B');
	this.addcmd('Set Extended Descriptions '+(user.settings&USER_EXTDESC?'Off':'On'),'S');
	set_hotkeys(this);
	fill_menu(this);
	this.node_action=NODE_DFLT;
}
menuobj["newmsgscan"]=function() {
	this.title="MESSAGE SCAN";
	this.items=new Array();
	this.addcmd('All Message Areas','A');
	this.addcmd("Current Group",'G');
	this.addcmd('Current Sub','S');
	this.addcmd('Change Scan Config','C');
	this.addcmd('Change Scan Pointers','P');
	this.addcmd('Reset Scan Pointers','R');
	set_hotkeys(this);
	fill_menu(this);
}
menuobj["yourmsgscan"]=function() {
	this.title="MESSAGE SCAN";
	this.items=new Array();
	this.addcmd('All Message Areas','A');
	this.addcmd("Current Group",'G');
	this.addcmd('Current Sub','S');
	this.addcmd('Change Scan Config','C');
	set_hotkeys(this);
	fill_menu(this);
}
menuobj["searchmsgtxt"]=function() {
	this.title="MESSAGE SCAN";
	this.items=new Array();
	this.addcmd('All Message Areas','A');
	this.addcmd("Current Group",'G');
	this.addcmd('Current Sub','S');
	set_hotkeys(this);
	fill_menu(this);
}
menuobj["chatsettings"]=function() {
	this.title="CHAT SETTINGS";
	this.items=new Array();
	this.addcmd("Set Split Screen "+(user.chat_settings&CHAT_SPLITP?"Off":"On"),'S');
	this.addcmd("Set Availability "+(user.chat_settings&CHAT_NOPAGE?"On":"Off"),'V');
	this.addcmd("Set Alerts "+(user.chat_settings&CHAT_NOACT?"On":"Off"),'A');
	set_hotkeys(this);
	fill_menu(this);
	this.node_action=NODE_DFLT;
}

/* MENU COMMANDS */
var menucmd=[];
menucmd["main"]=function(key) {
	switch(key) {
		case 'F':
			this.loadMenu("file");
			break;
		case 'S':
			this.loadMenu("settings");
			break;
		case 'E':
			this.loadMenu("email");
			break;
		case 'M':
			this.loadMenu("message");
			break;
		case 'C':
			this.loadMenu("chat");
			break;
		case 'X':
			this.loadMenu("xtrnsecs");
			break;
		case 'V':
			this.loadMenu("info");
			break;
	}
}
menucmd["favorites"]=function(key) {
	var fav=favorites.items[key];
	if(fav) {
		this.xtrnsec=fav.xtrnsec;
		this.loadMenu(fav.menuID);
		if(fav.itemID >= 0) {
			key=this.menu.items[fav.itemID].id;
			this.process(key);
		}
	}
}
menucmd["addfavorite"]=function(key) {
	if(favorites.mi == "favorites") {
		/* dont allow favorites of favorites */
		return false;
	}
	var index=favorites.length;

	if(key == 'M') {
		favorites.items.push(new Favorite(
			favorites.mi,
			favorites.mt,
			undefined,
			undefined,
			this.xtrnsec
		));
	} else if(key == 'P') {
		favorites.items.push(new Favorite(
			favorites.mi,
			favorites.mt,
			favorites.ii,
			favorites.it,
			this.xtrnsec
		));
	}
	
	saveSettings();
	this.loadMenu("favorites");
}
menucmd["delfavorite"]=function(key) {
	var fav=favorites.items[key];
	if(fav) {
		favorites.items.splice(key,1);
	}
	saveSettings();
	this.loadMenu("favorites");
}
menucmd["info"]=function(key) {
	switch(key) 
	{
		case 'I':
			clear_screen();
			bbs.sys_info();
			console.pause();
			this.redraw();
			break;
		case 'V':
			clear_screen();
			bbs.ver();
			console.pause();
			this.redraw();
			break;
		case 'S':
			clear_screen();
			bbs.sub_info();
			console.pause();
			this.redraw();
			break;
		case 'Y':
			clear_screen();
			bbs.user_info();
			console.pause();
			this.redraw();
			break;
		case 'U':
			this.loadMenu("userlist");
			break;
		case 'T':
			clear_screen();
			bbs.text_sec();
			this.redraw();
			break;
	}
}
menucmd["userlist"]=function(key) {
	switch(key) 
	{
		case 'L':
			clear_screen();
			bbs.list_logons();
			console.pause();
			this.redraw();
			break;
		case 'S':
			clear_screen();
			bbs.list_users(UL_SUB);
			console.pause();
			this.redraw();
			break;
		case 'A':
			clear_screen();
			bbs.list_users(UL_ALL);
			console.pause();
			this.redraw();
			break;
	}
}
menucmd["xtrnsecs"]=function(key) {
	this.xtrnsec=Number(key);
	this.loadMenu("xtrnsec");
}
menucmd["xtrnsec"]=function(key) {
	clear_screen();
	var current_passthru=console.ctrlkey_passthru;
	bbs.exec_xtrn(xtrn_area.sec_list[this.xtrnsec].prog_list[Number(key)].number);
	console.ctrlkey_passthru=current_passthru;
	full_redraw=true;
}
menucmd["file"]=function(key) {
	var i;
	var j;
	switch(key) 
	{
		case 'C':
			clear_screen();
			changedir: 
			do 
			{
				if(!file_area.lib_list.length)
					break changedir;
				while(1) {
					var orig_lib=bbs.curlib;
					i=0;
					j=0;
					if(file_area.lib_list.length>1) {
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
					console.line_counter=0;
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
			this.redraw();
			break;
		case 'L':
			clear_screen();
			bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number);
			console.pause();
			this.redraw();
			break;
		case 'N':
			this.loadMenu("filedir");
			this.process=menucmd["scannewfile"];
			break;
		case 'F':
			this.loadMenu("filedir");
			this.process=menucmd["scanfilenames"];
			break;
		case 'T':
			this.loadMenu("filedir");
			this.process=menucmd["scanfiledesc"];
			break;
		case 'D':
			this.loadMenu("download");
			break;
		case 'U':
			this.loadMenu("upload");
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
			this.redraw();
			break;
		case 'B':
			clear_screen();
			bbs.batch_menu();
			this.redraw();
			break;
		case 'V':
			this.loadMenu("fileinfo");
			break;
		case 'S':
			this.loadMenu("filesettings");
			break;
		default:
			break;
	}
}
menucmd["fileinfo"]=function(key) {
	switch(key) 
	{
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
						break;
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
			this.redraw();
			break;
		case 'I':
			clear_screen();
			console.putmsg("\r\nchView File Information\r\n");
			str=bbs.get_filespec();
			if(str!=null) 
			{
				if(!bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FI_INFO)) {
					console.putmsg(bbs.text(SearchingAllDirs));
					for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
						if(i==bbs.curdir)
							continue;
						if(bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FI_INFO))
							break;
					}
					if(i<file_area.lib_list[bbs.curlib].dir_list.length)
						break;
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
			this.redraw();
			break;
		case 'P':
			clear_screen();
			bbs.xfer_policy();
			console.pause();
			this.redraw();
			break;
		case 'D':
			clear_screen();
			bbs.dir_info();
			console.pause();
			this.redraw();
			break;
		case 'U':
			clear_screen();
			bbs.list_users(UL_DIR);
			console.pause();
			this.redraw();
			break;
		case 'S':
			break;
		default:
			left.menu.nodraw=true;
			break;
	}
}
menucmd["scannewfile"]=function(key) {
	switch(key) 
	{
		case 'A':
			clear_screen();
			console.putmsg("\r\nchNew File Scan (All)\r\n");
			bbs.scan_dirs(FL_ULTIME,true);
			console.pause();
			this.redraw();
			break;
		case 'L':
			/* Scan this lib only */
			clear_screen();
			console.putmsg("\r\nchNew File Scan (Lib)\r\n");
			for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++)
				bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[i].number,FL_ULTIME);
			console.pause();
			this.redraw();
			break;
		case 'D':
			/* Scan this dir only */
			clear_screen();
			console.putmsg("\r\nchNew File Scan (Dir)\r\n");
			bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number,FL_ULTIME);
			console.pause();
			this.redraw();
			break;
		case 'N':
			// ToDo: Don't clear screen here, just do one line
			clear_screen();
			bbs.new_file_time=bbs.get_newscantime(bbs.new_file_time);
			this.redraw();
			break;
		default:	// Anything else will escape.
			left.menu.nodraw=true;
			break;
	}
}
menucmd["scanfilenames"]=function(key) {
	switch(key)
	{
		case 'A':
			clear_screen();
			console.putmsg("\r\nchSearch for Filename(s) (All)\r\n");
			var spec=bbs.get_filespec();
			for(i=0; i<file_area.lib_list.length; i++) {
				for(j=0;j<file_area.lib_list[i].dir_list.length;j++)
					bbs.list_files(file_area.lib_list[i].dir_list[j].number,spec,0);
			}
			console.pause();
			this.redraw();
			break;
		case 'L':
			/* Scan this lib only */
			clear_screen();
				console.putmsg("\r\nchSearch for Filename(s) (Lib)\r\n");
			var spec=bbs.get_filespec();
			for(j=0;j<file_area.lib_list[bbs.curlib].dir_list.length;j++)
				bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[j].number,spec,0);
			console.pause();
			this.redraw();
			break;
		case 'D':
			/* Scan this dir only */
			clear_screen();
			console.putmsg("\r\nchSearch for Filename(s) (Dir)\r\n");
			var spec=bbs.get_filespec();
			bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number,spec,0);
			console.pause();
			this.redraw();
			break;
		default:	// Anything else will escape.
			left.menu.nodraw=true;
			break;
	}
}
menucmd["scanfiledesc"]=function(key) {
	switch(key) 
	{
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
			this.redraw();
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
			this.redraw();
			break;
		case 'D':
			/* Scan this dir only */
			clear_screen();
			console.putmsg("\r\nchSearch for Text in Description(s) (Dir)\r\n");
			console.putmsg(bbs.text(SearchStringPrompt));
			var spec=console.getstr(40,K_LINE|K_UPPER);
			bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number,spec,FL_FINDDESC);
			console.pause();
			this.redraw();
			break;
		default:	// Anything else will escape.
			left.menu.nodraw=true;
			break;
	}
}
menucmd["upload"]=function(key) {
	switch(key) {
		case 'C':	// Current dir
			clear_screen();
			bbs.upload_file(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number);
			this.redraw();
			break;
		case 'P':	// Menu_upload dir
			clear_screen();
			bbs.upload_file(file_area.upload_dir);
			this.redraw();
			break;
		case 'S':	// Sysop dir
			clear_screen();
			bbs.upload_file(file_area.sysop_dir);
			this.redraw();
			break;
		case 'U':	// To user
			clear_screen();
			bbs.upload_file(file_area.user_dir);
			this.redraw();
		default:
			left.menu.nodraw=true;
			break;
	}
}
menucmd["download"]=function(key) {
	switch(key) 
	{
		case 'B':
			clear_screen();
			bbs.batch_download();
			this.redraw();
			break;
		case 'N':
			clear_screen();
			var spec=bbs.get_filespec();
			bbs.list_file_info(bbs.curdir,spec,FI_DOWNLOAD);
			this.redraw();
			break;
		case 'U':
			clear_screen();
			bbs.list_file_info(bbs.curdir,spec,FI_USERXFER);
			this.redraw();
			break;
		default:
			left.menu.nodraw=true;
			break
	}
}
menucmd["filesettings"]=function(key) {
	switch(key) 
	{
		case 'B':
			user.settings ^= USER_BATCHFLAG;
			break;
		case 'S':
			user.settings ^= USER_EXTDESC;
			break;
		default:
			left.menu.nodraw=true;
			break;
	}
}
menucmd["message"]=function(key) {
	var i;
	var j;
	message: 
	switch(key) 
	{
		case 'C':
			clear_screen();
			if(!msg_area.grp_list.length)
				break;
			msgjump: 
			while(1) 
			{
				var orig_grp=bbs.curgrp;
				var i=0;
				var j=0;
				if(msg_area.grp_list.length>1) {
					console.putmsg(bbs.text(CfgGrpLstHdr),P_SAVEATR);
					for(i=0; i<msg_area.grp_list.length; i++) 
					{
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
				console.line_counter=0;
				console.clear();
				console.putmsg(format(bbs.text(SubLstHdr), msg_area.grp_list[j].description),P_SAVEATR);
				for(i=0; i<msg_area.grp_list[j].sub_list.length; i++) 
				{
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
				console.mnemonics(format(bbs.text(JoinWhichSub),bbs.cursub+1));
				i=console.getnum(msg_area.grp_list[j].sub_list.length);
				if(i==-1) 
				{
					if(msg_area.grp_list.length==1) 
					{
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
			this.redraw();
			break;
		case 'R':
			clear_screen();
			bbs.scan_posts();
			this.redraw();
			break;
		case 'N':
			this.loadMenu("newscan");
			break;
		case 'Y':
			this.loadMenu("scantoyou");
			break;
		case 'T':
			this.loadMenu("searchmsgtxt");
			break;
		case 'P':
			clear_screen();
			bbs.post_msg();
			this.redraw();
			break;
		case 'A':
			clear_screen();
			bbs.auto_msg();
			this.redraw();
			break;
		case 'Q':
			clear_screen();
			bbs.qwk_sec();
			this.redraw();
			break;
		case 'V':
			clear_screen();
			bbs.sub_info();
			console.pause();
			this.redraw();
			break;
	}
}
menucmd["searchmsgtxt"]=function(key) {
	switch(key) 
	{
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
			this.redraw();
			break;
		case 'G':
			clear_screen();
			console.putmsg("\r\n\x01c\x01hMessage Search\r\n");
			str=console.getstr("",40,K_LINE|K_UPPER);
			for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++)
				bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_FIND, str);
			this.redraw();
			break;
		case 'S':
			clear_screen();
			console.putmsg("\r\n\x01c\x01hMessage Search\r\n");
			str=console.getstr("",40,K_LINE|K_UPPER);
			bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_FIND, str);
			this.redraw();
			break;
		default:
			left.menu.nodraw=true;
			break;
	}
}
menucmd["yourmsgscan"]=function(key) {
	switch(key) 
	{
		case 'A':
			clear_screen();
			console.putmsg("\r\n\x01c\x01hYour Message Scan\r\n");
			for(j=0; j<msg_area.grp_list.length; j++) {
				for(i=0; i<msg_area.grp_list[j].sub_list.length; i++)
					bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_TOYOU);
			}
			this.redraw();
			break;
		case 'G':
			clear_screen();
			console.putmsg("\r\n\x01c\x01hYour Message Scan\r\n");
			for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++)
				bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_TOYOU);
			this.redraw();
			break;
		case 'S':
			clear_screen();
			console.putmsg("\r\n\x01c\x01hYour Message Scan\r\n");
			bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_TOYOU);
			this.redraw();
			break;
		case 'C':
			clear_screen();
			bbs.cfg_msg_scan(SCAN_CFG_TOYOU);
			this.redraw();
			break;
		default:
			left.menu.nodraw=true;
			break;
	}
}
menucmd["newmsgscan"]=function(key) {
	switch(key) 
	{
		case 'A':
			clear_screen();
			console.putmsg("\r\n\x01c\x01hNew Message Scan\r\n");
			for(j=0; j<msg_area.grp_list.length; j++) {
				for(i=0; i<msg_area.grp_list[j].sub_list.length; i++)
					bbs.scan_posts(msg_area.grp_list[j].sub_list[i].number, SCAN_NEW);
			}
			this.redraw();
			break;
		case 'G':
			clear_screen();
			console.putmsg("\r\n\x01c\x01hNew Message Scan\r\n");
			for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++)
				bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_NEW);
			this.redraw();
			break;
		case 'S':
			clear_screen();
			console.putmsg("\r\n\x01c\x01hNew Message Scan\r\n");
			bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_NEW);
			this.redraw();
			break;
		case 'C':
			clear_screen();
			bbs.cfg_msg_scan(SCAN_CFG_NEW);
			this.redraw();
			break;
		case 'P':
			clear_screen();
			bbs.cfg_msg_ptrs(SCAN_CFG_NEW);
			this.redraw();
			break;
		case 'R':
			bbs.reinit_msg_ptrs()
			break;
		default:
			left.menu.nodraw=true;
			break;
	}
}
menucmd["emailtarget"]=function(key) {
	switch(key) 
	{
		case 'S':
			clear_screen();
			bbs.email(1,WM_EMAIL,bbs.text(ReFeedback));
			this.redraw();
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
			this.redraw();
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
			this.redraw();
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
			this.redraw();
			break;
		case 'T':
			clear_screen();
			console.putmsg("\x01_\r\n\x01b\x01hE-mail (User name or number): \x01w");
			str=console.getstr("",40,K_UPRLWR);
			if(str!=null && str!="")
				bbs.netmail(str,WM_FILE);
			this.redraw();
			break;
	}
}
menucmd["email"]=function(key) {
	var cur=1;
	this.menu.current=cur;
	var i;
	var j;
	switch(key) 
	{
		case 'S':
			this.loadMenu("emailtarget");
			break;
		case 'R':
			clear_screen();
			bbs.read_mail(MAIL_YOUR);
			console.pause();
			this.redraw();
			break;
		case 'M':
			clear_screen();
			bbs.read_mail(MAIL_SENT);
			console.pause();
			this.redraw();
			break;
	}
	cur=this.menu.current;
}
menucmd["chat"]=function(key) {
	var cur=1;
	this.menu.current=cur;
	var i;
	var j;
	chat: 
	switch(key)
	{
		case 'M':
			clear_screen();
			bbs.multinode_chat();
			this.redraw();
			break;
		case 'P':
			clear_screen();
			bbs.private_chat();
			this.redraw();
			break;
		case 'C':
			clear_screen();
			if(!bbs.page_sysop())
				bbs.page_guru();
			this.redraw();
			break;
		case 'T':
			clear_screen();
			bbs.page_guru();
			this.redraw();
			break;
		case 'F':
			clear_screen();
			bbs.exec("?finger");
			console.pause();
			this.redraw();
			break;
		case 'R':
			clear_screen();
			write("\001n\001y\001hServer and channel: ");
			str="irc.synchro.net 6667 #bbs";
			str=console.getstr(str, 50, K_EDIT|K_LINE|K_AUTODEL);
			if(!console.aborted)
				bbs.exec("?irc -a "+str);
			this.redraw();
			break;
		case 'I':
			clear_screen();
			bbs.exec("?sbbsimsg");
			this.redraw();
			break;
		case 'S':
			this.loadMenu("chatsettings");
			break;
	}
	cur=this.menu.current;
}
menucmd["chatsettings"]=function(key) {
	switch(key) 
	{
		case 'S':
			if(user.chat_settings&CHAT_SPLITP)
			user.chat_settings ^= CHAT_SPLITP;
			break;
		case 'V':
			user.chat_settings ^= CHAT_NOPAGE;
			break;
		case 'A':
			user.chat_settings ^= CHAT_NOACT;
			break;
		default:
			left.menu.nodraw=true;
			break;
	}
}
menucmd["settings"]=function(key) {
	switch(key) 
	{
		case 'U':
			clear_screen();
			var oldshell=user.command_shell;
			bbs.user_config();
			/* Still using this shell? */
			if(user.command_shell != oldshell)
				exit(0);
			this.redraw();
			break;
		case 'B':
			clear_screen();
			bbs.time_bank();
			this.redraw();
			break;
	}
}

/* MENU INFO */
var menuinfo=[];
menuinfo["main"]=function() {
	var wp=directory(system.text_dir + "cshell/mainmenu.*.bin")[0];
	if(wp) {
		center.loadWallPaper(wp);
		center.redraw();
	}
}
menuinfo["xtrnsec"]=function() {
	center.clear();
	if(!xtrn_area.sec_list[left.xtrnsec].prog_list[left.menu.current]) return false;
	var wp=directory(system.text_dir + "cshell/xtrn/" + 
			xtrn_area.sec_list[left.xtrnsec].prog_list[left.menu.current].code + ".*.bin")[0];
	if(wp) {
		center.loadWallPaper(wp);
		center.redraw();
	}
}
menuinfo["xtrnsecs"]=function() {
	center.clear();
	if(!xtrn_area.sec_list[left.xtrnsec]) return false;
	var wp=directory(system.text_dir + "cshell/xtrn/" + 
			xtrn_area.sec_list[left.xtrnsec].code + ".*.bin")[0];
	if(wp) {
		center.loadWallPaper(wp);
		center.redraw();
	}
}
menuinfo["message"]=function() {
	var wp=directory(system.text_dir + "cshell/message.*.bin")[0];
	if(wp) {
		center.loadWallPaper(wp);
		center.redraw();
		return;
	}
	var posx=center.chat.chatroom.x+1;
	var posy=center.chat.chatroom.y-2;
	setPosition(posx,posy);
	displayInfo("\1n\1gGROUP\1h:\1n " + msg_area.grp_list[bbs.curgrp].name);
	displayInfo("\1n\1gSUB  \1h:\1n " + msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name);
}
menuinfo["chat"]=function() {
	var wp=directory(system.text_dir + "cshell/chat.*.bin")[0];
	if(wp) {
		center.loadWallPaper(wp);
		center.redraw();
		return;
	}
	var posx=center.chat.chatroom.x+1;
	var posy=center.chat.chatroom.y-2;
	setPosition(posx,posy);
	displayInfo("\1n\1gChat Handle\1h:\1n " + user.handle);
}
menuinfo["file"]=function() {
	var wp=directory(system.text_dir + "cshell/file.*.bin")[0];
	if(wp) {
		center.loadWallPaper(wp);
		center.redraw();
		return;
	}
	var posx=center.chat.chatroom.x+1;
	var posy=center.chat.chatroom.y-2;
	setPosition(posx,posy);
	displayInfo("\1n\1gLIB \1h:\1n " + file_area.lib_list[bbs.curlib].description);
	displayInfo("\1n\1gDIR \1h:\1n " + file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].description);
}
menuinfo["email"]=function() {
	var wp=directory(system.text_dir + "cshell/email.*.bin")[0];
	if(wp) {
		center.loadWallPaper(wp);
		center.redraw();
		return;
	}
	var posx=center.chat.chatroom.x+1;
	var posy=center.chat.chatroom.y-2;
	setPosition(posx,posy);
	displayInfo("\1n\1gMessages sent\1h:\1n " + user.stats.total_emails);
	displayInfo("\1n\1gNew Messages\1h:\1n " + user.stats.mail_waiting);
}
menuinfo["userlist"]=function() {
	var wp=directory(system.text_dir + "cshell/userlist.*.bin")[0];
	if(wp) {
		center.loadWallPaper(wp);
		center.redraw();
		return;
	}
}


