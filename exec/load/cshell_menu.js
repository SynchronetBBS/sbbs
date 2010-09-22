/* MAIN MENU OBJECT */
function Menu_bottom(items,x,y)
{
	this.items=items;
	this.x=x;
	this.y=y;
	
	this.disable=function(item)
	{
		this.items[item].enabled=false;
	}
	this.enable=function(item)
	{
		this.items[item].enabled=true;
	}
	this.clear=function()
	{
		console.gotoxy(this);
		console.cleartoeol(settings.shell_bg);
	}
	this.draw=function()
	{
		console.gotoxy(this);
		console.pushxy();
		console.cleartoeol(settings.shell_bg);
		console.popxy();
		for(var i in this.items) {
			if(this.items[i].enabled==true) {
				var hc=settings.main_hkey_color;
				var tc=settings.main_text_color;
				var bg=settings.shell_bg;
				
				var item=this.items[i];
				for(var c=0;c<item.text.length;c++) {
					if(item.text[c]=="|") {
						console.attributes=bg + hc;
						c++;
					} else {
						console.attributes=bg + tc;
					}
					console.write(item.text[c]);
				}
				console.write(" ");
			}
		}
		console.attributes=ANSI_NORMAL;
	}
}

/* SIDE MENU OBJECTS */
function Menu_sidebar()
{
	this.lpadding="";
	this.rpadding="";
	this.fg=settings.menu_fg;
	this.bg=settings.menu_bg;
	this.hfg=settings.menu_hfg;
	this.hbg=settings.menu_hbg;
	this.width=settings.menu_width;
	this.xpos=settings.menu_x;
	this.ypos=settings.menu_y;
	this.hotkeys=
		KEY_LEFT
		+KEY_RIGHT
		+KEY_UP
		+KEY_DOWN
		+"\b\x7f\x1b<>Q"
		+ctrl('O')
		+ctrl('U')
		+ctrl('T')
		+ctrl('K')
		+ctrl('P');
	for(var i in bottom.menu.items) {
		this.hotkeys+=i;
	}
	this.addcmd=Item_addcmd;
}

function Menu_main()
{
	this.title="MAIN";
	this.items=new Array();
	this.addcmd("Files","F",user.compare_ars("REST T"));
	this.addcmd("Messages","M");
	this.addcmd("Email","E",user.compare_ars("REST SE"));
	this.addcmd("Chat","C",user.compare_ars("REST C"));
	this.addcmd("Settings","S");
	this.addcmd("Externals","X",user.compare_ars("REST X"));
	this.addcmd("View","V");
	set_hotkeys(this);
	fill_menu(this);
}
function Menu_favorites()
{
	this.title="FAVORITES";
	this.items=new Array();
	this.addcmd("Add Favorite","+",true);
	set_hotkeys(this);
	fill_menu(this);
}
function Menu_file()
{
	this.title="FILES";
	this.items=new Array();
	this.addcmd("[" + file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.toUpperCase() + "]",undefined,true);
	this.addcmd("",undefined,true);
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
}
function Menu_filedir(changenewscan)
{
	this.title="FILES";
	this.items=new Array();
	this.addcmd("[" + file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.toUpperCase()+"]",undefined,true);
	this.addcmd("",undefined,true);
	this.addcmd("All File Areas","A");
	this.addcmd("Library","L");
	this.addcmd("Directory","D");
	if(changenewscan)
		this.addcmd("Chg New Scan Date","N");
	set_hotkeys(this);
	fill_menu(this);
}
function Menu_settings()
{
	this.title="SETTINGS";
	this.items=new Array();
	this.addcmd("User Configuration","U");
	this.addcmd("Minute Bank","B");
	set_hotkeys(this);
	fill_menu(this);
}
function Menu_email()
{
	this.title="E-MAIL";
	this.items=new Array();
	this.addcmd("Send Mail","S");
	this.addcmd("Read Inbox","R");
	this.addcmd("Read Sent Messages","M",user.compare_ars("REST K"));
	set_hotkeys(this);
	fill_menu(this);
}
function Menu_message()
{
	this.title="MESSAGES : " + msg_area.grp_list[bbs.curgrp].name.toUpperCase();
	this.items=new Array();
	this.addcmd("[" + msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.toUpperCase()+"]",undefined,true);
	this.addcmd("",undefined,true);
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
function Menu_chat()
{
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
}
function Menu_xtrnsecs()
{
	this.title="GAMES";
	this.items=new Array();
	for(j=0; j<xtrn_area.sec_list.length; j++)
		this.addcmd(xtrn_area.sec_list[j].name,j.toString());
	set_hotkeys(this);
	fill_menu(this);
}
function Menu_xtrnsec(sec)
{
	this.title="GAMES : " + xtrn_area.sec_list[sec].name;
	this.items=new Array();
	for(j=0; j<xtrn_area.sec_list[sec].prog_list.length && j<console.screen_rows-3; j++)
		this.addcmd(xtrn_area.sec_list[sec].prog_list[j].name,j.toString());
	set_hotkeys(this);
	fill_menu(this);
}
function Menu_info()
{
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
function Menu_userlist()
{
	this.title="USERS";
	this.items=new Array();
	this.addcmd("Logons Today","L");
	this.addcmd("Sub-Board","S");
	this.addcmd("All","A");
	set_hotkeys(this);
	fill_menu(this);
}
function Menu_emailtarget()
{
	this.title="E-MAIL";
	this.items=new Array();
	this.addcmd('Sysop','S',user.compare_ars("REST S"));
	this.addcmd('Local User','L',user.compare_ars("REST E"));
	this.addcmd('To Local User w/Attach','A',user.compare_ars("REST E"));
	this.addcmd('To Remote User','R',user.compare_ars("REST E OR REST M"));
	this.addcmd('To Remote User w/Attach','T',user.compare_ars("REST E OR REST M"));
	set_hotkeys(this);
	fill_menu(this);
}
function Menu_download()
{
	this.title="DOWNLOAD";
	this.items=new Array();
	this.addcmd('Batch','B',bbs.batch_dnload_total<=0);
	this.addcmd('By Name/File spec','N');
	this.addcmd('From User','U');
	set_hotkeys(this);
	fill_menu(this);
}
function Menu_upload()
{
	this.title="UPLOAD";
	this.items=new Array();
	this.addcmd("[" + file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.toUpperCase()+"]",undefined,true);
	this.addcmd("",undefined,true);
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
}
function Menu_fileinfo()
{
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
}
function Menu_filesettings(value)
{
	this.current=value;
	this.title="FILE SETTINGS";
	this.items=new Array();
	this.addcmd('Set Batch Flagging '+(user.settings&USER_BATCHFLAG?'Off':'On'),'B');
	this.addcmd('Set Extended Descriptions '+(user.settings&USER_EXTDESC?'Off':'On'),'S');
	set_hotkeys(this);
	fill_menu(this);
}
function Menu_newmsgscan()
{
	this.title="MESSAGE SCAN";
	this.items=new Array();
	this.addcmd("[" + msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.toUpperCase()+"]",undefined,true);
	this.addcmd("",undefined,true);
	this.addcmd('All Message Areas','A');
	this.addcmd("Current Group",'G');
	this.addcmd('Current Sub','S');
	this.addcmd('Change Scan Config','C');
	this.addcmd('Change Scan Pointers','P');
	this.addcmd('Reset Scan Pointers','R');
	set_hotkeys(this);
	fill_menu(this);
}
function Menu_yourmsgscan()
{
	this.title="MESSAGE SCAN";
	this.items=new Array();
	this.addcmd("[" + msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.toUpperCase()+"]",undefined,true);
	this.addcmd("",undefined,true);
	this.addcmd('All Message Areas','A');
	this.addcmd("Current Group",'G');
	this.addcmd('Current Sub','S');
	this.addcmd('Change Scan Config','C');
	set_hotkeys(this);
	fill_menu(this);
}
function Menu_searchmsgtxt()
{
	this.title="MESSAGE SCAN";
	this.items=new Array();
	this.addcmd("[" + msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.toUpperCase()+"]",undefined,true);
	this.addcmd("",undefined,true);
	this.addcmd('All Message Areas','A');
	this.addcmd("Current Group",'G');
	this.addcmd('Current Sub','S');
	set_hotkeys(this);
	fill_menu(this);
}
function Menu_chatsettings()
{
	this.title="CHAT SETTINGS";
	this.items=new Array();
	this.addcmd("Set Split Screen "+(user.chat_settings&CHAT_SPLITP?"Off":"On"),'S');
	this.addcmd("Set Availability "+(user.chat_settings&CHAT_NOPAGE?"On":"Off"),'V');
	this.addcmd("Set Alerts "+(user.chat_settings&CHAT_NOACT?"On":"Off"),'A');
	set_hotkeys(this);
	fill_menu(this);
}

/* MENU COMMANDS */
function do_mainmenu(key)
{
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
function do_favorites(key)
{
	switch(key) {
		case '+':
			add_favorite();
			break;
	}
}
function do_infomenu(key)
{
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
function do_userlistmenu(key)
{
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
function do_xtrnsecs(key)
{
	this.curr_xtrnsec=Number(key);
	this.loadMenu("xtrnsec",this.curr_xtrnsec);
}
function do_xtrnsec(key)
{
	clear_screen();
	var current_passthru=console.ctrlkey_passthru;
	bbs.exec_xtrn(xtrn_area.sec_list[this.curr_xtrnsec].prog_list[Number(key)].number);
	console.ctrlkey_passthru=current_passthru;
	full_redraw=true;
}
function do_filemenu(key)
{
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
			this.loadMenu("newfiles",true);
			break;
		case 'F':
			this.loadMenu("searchfilenames",true);
			break;
		case 'T':
			this.loadMenu("searchfiledesc",true);
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
			this.loadMenu("filesettings",cur);
			break;
		default:
			break;
	}
}
function do_fileinfo(key)
{
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
function do_filedirmenu1(key)
{
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
function do_filedirmenu2(key)
{
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
function do_filedirmenu3(key)
{
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
function do_uploadmenu(key)
{
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
function do_downloadmenu(key)
{
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
function do_filesettings(key)
{
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
function do_messagemenu(key)
{
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
function do_searchmsgtxt(key)
{
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
function do_scantoyou(key)
{
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
function do_newscan(key)
{
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
function do_emailtargetmenu(key)
{
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
function do_emailmenu(key)
{
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
function do_chatmenu(key)
{
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
function do_chatsettings(key)
{
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
function do_settingsmenu(key)
{
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

/* MENU FUNCTIONS */
function Item_addcmd(text,id,disabled)
{
	this.add(text,undefined,settings.menu_width,undefined,undefined,disabled);
	this.items[this.items.length-1].id=id;
}
function set_hotkeys(lb)
{
	/* USE FIRST AVAILABLE HOTKEY AS TRIGGER */
	/* RETURN VALUE = ITEM INDEX FOR MENU COMMAND REFERENCE */
	var hotkeys="1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	var index=0;
	for(var i=0;i<lb.items.length;i++) {
		if(lb.items[i].disabled) continue;
		
		while(bottom.menu.items[hotkeys[index]]) index++;
		lb.items[i].text="|" + hotkeys[index++] + " " + lb.items[i].text;
		lb.items[i].retval=i;
	}
}
function fill_menu(lb)
{
	if(left.previous.length) offset=5;
	else offset=4;
	
	while(lb.items.length<settings.main_height-offset)
	{
		lb.add("","",settings.menu_width,undefined,undefined,true);
	}
	
	if(left.previous.length) lb.add(format_opt("Previous Menu",settings.menu_width,-1),KEY_LEFT,settings.menu_width);
	lb.add(format_opt("Main Menu",settings.menu_width,0),KEY_RIGHT,settings.menu_width);
}
function format_opt(str, width, expand)
{
	var spaces80="                                                                               ";
	if(expand == -1) {
		opt='|< ';
		var cleaned=str;
		cleaned=cleaned.replace(/\|/g,'');
		opt+=str+spaces80.substr(0,width-cleaned.length-2);
		return opt;
	} else if(expand == 0) {
		opt=str;
		var cleaned=opt;
		cleaned=cleaned.replace(/\|/g,'');
		opt+=spaces80.substr(0,width-cleaned.length-2);
		opt+=' |>';
		return opt;
	} else if(expand == 1) {
		opt=str;
		var cleaned=opt;
		cleaned=cleaned.replace(/\|/g,'');
		opt+=spaces80.substr(0,width-cleaned.length-2);
		opt+=' |+';
		return opt;
	}
	return(str);
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

	bbs.sys_status&=~SS_MOFF;
	bbs.sys_status&=~SS_PAUSEOFF;
	console.clear(ANSI_NORMAL);
	
	/* We are going to a line-mode thing... re-enable CTRL keys. */
}


