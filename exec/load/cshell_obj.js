/* MAIN MENU */
function MainMenu()		
{
	this.x=1;
	this.y=console.screen_rows;
	this.menu;
	
	this.init=function()
	{
		var menu_items=[];
		menu_items["A"]=new Shortcut("M|ain","menu","main");
		menu_items["C"]=new Shortcut("|Chat","chat","");
		menu_items["D"]=new Shortcut("|Doors","menu","xtrnsecs");
		menu_items["E"]=new Shortcut("|Email","menu","email");
		menu_items["F"]=new Shortcut("|Files","menu","file");
		menu_items["M"]=new Shortcut("|Messages","menu","message");
		menu_items["V"]=new Shortcut("Fa|vorites","menu","favorites");
		menu_items["S"]=new Shortcut("|Settings","menu","settings");
		menu_items["L"]=new Shortcut("|Logoff","logoff","");
		this.menu=new Menu_bottom(menu_items,9,console.screen_rows);	
	}
	this.restore=function()
	{
		this.redraw();
	}
	this.redraw=function()
	{
		console.gotoxy(this);
		console.cleartoeol(settings.shell_bg);
		console.right();
		console.attributes=BG_LIGHTGRAY;
		console.putmsg("\1k\xDDMENU:",P_SAVEATR);
		console.attributes=settings.shell_bg;
		console.putmsg("\1k\xDD",P_SAVEATR);
		this.menu.draw();
	}
	this.clear=function()
	{
		console.gotoxy(this);
		console.cleartoeol(settings.shell_bg);
	}
	
}
function Shortcut() 
{
	this.enabled=true;
	
	this.text=arguments[0];
	this.command=arguments[1];
	this.parameters=[];

	for(var i=2;i<arguments.length;i++) {
		this.parameters.push(arguments[i]);
	}
}

/* RIGHT WINDOW */
function RightWindow()
{
	this.clock=new DigitalClock();
	this.graphic;
	this.x=console.screen_columns-18;
	this.y=2;
	
	this.init=function()
	{
		this.clock.init(this.x,this.y+10,LIGHTBLUE);
		this.graphic=new Graphic(16,10);
		this.graphic.load(system.text_dir + "cshell/logo.16x10.bin");
	}
	this.redraw=function()
	{
		drawSeparator(console.screen_columns-18,2,console.screen_rows-2);
		this.clock.update(true);
		this.graphic.draw(this.x+1,this.y);
	}
	this.cycle=function()
	{
		this.clock.update();
	}
}

/* MAIN WINDOW */
function MainWindow()
{
	this.in_chat=false;
	this.wallpaper=false;
	this.chat=new ChatEngine(root);
	
	this.init=function()
	{
		this.chat.init(settings.main_width,settings.main_height-3,2,5);
		this.chat.input_line.init(9,console.screen_rows,console.screen_columns-9,"\0017","\1k");
		this.chat.joinChan("BBS MAIN",user.alias,user.name);
		this.loadWallPaper(system.text_dir + "cshell/main.*.bin");
		this.chat.chatroom.active=false;
	}
	this.drawTitle=function()
	{
		var scope="";
		for(var c in this.chat.chatroom.channels) {
			scope+=c + " ";
		}
		if(center.chat.input_line.scope == flag_global) {
			scope="#global";
		}
		var str=splitPadded(" CHAT: " + scope,"TOGGLE: TAB ",this.chat.chatroom.columns," ");
		drawTitle(this.chat.chatroom.x,this.chat.chatroom.y-3,str);
	}
	this.toggleChannel=function()
	{
		this.chat.input_line.toggle();
		this.drawTitle();
	}
	this.cycle=function()
	{
		this.chat.cycle();
	}
	this.restore=function()
	{
		this.loadWallPaper(system.text_dir + "cshell/main.*.bin");
		this.redraw();
	}
	this.redraw=function()
	{
		if(this.in_chat) {
			this.drawChat();
		} else {
			this.drawWallPaper();
		}
	}
	
	this.drawChat=function()
	{
		this.drawTitle();
		this.chat.redraw();
	}
	this.loadWallPaper=function(file)
	{
		file=directory(file);
		var width=0;
		var height=0;
		if(file.length) {
			var size=file_getname(file[0]).split(".")[1].split("x");
			width=Number(size[0]);
			height=Number(size[1]);
		}
		this.wallpaper=new Graphic(width,height);
		this.wallpaper.load(file);
	}
	this.drawWallPaper=function()
	{
		this.wallpaper.draw(this.chat.chatroom.x,this.chat.chatroom.y-3);
	}
}
function SideMenu()
{
	var msg_rows=0;
	var msg_timeouts=new Array();
	var lastmessage_time=0;
	var lastmessage_type=0;
	var hangup_now=false;
	var done=0;
	
	this.menu_shown=false;
	this.title_shown=false;

	this.previous=[];
	this.curr_xtrnsec=0;
	this.currentmenu;
	this.menu;
	this.process;

	this.init=function()
	{
		Menu_sidebar.prototype=new Lightbar;
		Menu_main.prototype=new Menu_sidebar;
		Menu_file.prototype=new Menu_sidebar;
		Menu_filedir.prototype=new Menu_sidebar;
		Menu_settings.prototype=new Menu_sidebar;
		Menu_email.prototype=new Menu_sidebar;
		Menu_message.prototype=new Menu_sidebar;
		Menu_chat.prototype=new Menu_sidebar;
		Menu_xtrnsecs.prototype=new Menu_sidebar;
		Menu_xtrnsec.prototype=new Menu_sidebar;
		Menu_info.prototype=new Menu_sidebar;
		Menu_userlist.prototype=new Menu_sidebar;
		Menu_emailtarget.prototype=new Menu_sidebar;
		Menu_download.prototype=new Menu_sidebar;
		Menu_upload.prototype=new Menu_sidebar;
		Menu_fileinfo.prototype=new Menu_sidebar;
		Menu_filesettings.prototype=new Menu_sidebar;
		Menu_newmsgscan.prototype=new Menu_sidebar;
		Menu_yourmsgscan.prototype=new Menu_sidebar;
		Menu_searchmsgtxt.prototype=new Menu_sidebar;
		Menu_chatsettings.prototype=new Menu_sidebar;
		Menu_favorites.prototype=new Menu_sidebar;
	}
	this.cycle=function()
	{
		/* ToDo: is this needed? */
	}
	this.redraw=function()
	{
		if(this.menu) {
			this.drawTitle();
			this.menu.draw();
			drawSeparator(settings.menu_width+2,2,console.screen_rows-2);
		}	
	}
	this.reload=function()
	{
		this.loadMenu(this.currentmenu);
	}
	this.drawTitle=function()
	{
		if(!this.menu) return;
		var str=printPadded(" " + this.menu.title,this.menu.width," ");
		drawTitle(this.menu.xpos,this.menu.ypos-3,str);
		this.title_shown=true;
	}
	this.previousMenu=function()
	{
		if(this.previous.length) {
			delete this.currentmenu;
			this.loadMenu(this.previous.pop());
		}
	}
	this.loadMenu=function(name,value)
	{
		if(this.currentmenu) {
			if(this.currentmenu==name) return false;
			this.previous.push(this.currentmenu);
		}
		this.currentmenu=name;
		switch(name)
		{
			case "main":
				this.menu=new Menu_main;
				this.process=do_mainmenu;
				bbs.node_action=NODE_MAIN;
				break;
			case "favorites":
				this.menu=new Menu_favorites;
				this.process=do_favorites;
				break;
			case "xtrnsecs":
				this.menu=new Menu_xtrnsecs;
				this.process=do_xtrnsecs;
				bbs.node_action=NODE_XTRN;
				break;
			case "xtrnsec":
				this.menu=new Menu_xtrnsec(value);
				this.process=do_xtrnsec;
				bbs.node_action=NODE_XTRN;
				break;
			case "file":
				this.menu=new Menu_file;
				this.process=do_filemenu;
				bbs.node_action=NODE_XFER;
				center.loadWallPaper(system.text_dir + "cshell/file.*.bin");
				break;
			case "message":
				this.menu=new Menu_message;
				this.process=do_messagemenu;
				center.loadWallPaper(system.text_dir + "cshell/message.*.bin");
				break;
			case "chat":
				this.menu=new Menu_chat;
				this.process=do_chatmenu;
				bbs.node_action=NODE_CHAT;
				center.loadWallPaper(system.text_dir + "cshell/chat.*.bin");
				break;
			case "email":
				this.menu=new Menu_email;
				this.process=do_emailmenu;
				center.loadWallPaper(system.text_dir + "cshell/email.*.bin");
				break;
			case "emailtarget":
				this.menu=new Menu_emailtarget;
				this.process=do_emailtargetmenu;
				center.loadWallPaper(system.text_dir + "cshell/email.*.bin");
				break;
			case "fileinfo":
				this.menu=new Menu_fileinfo;
				this.process=do_fileinfo;
				bbs.node_action=NODE_XFER;
				center.loadWallPaper(system.text_dir + "cshell/file.*.bin");
				break;
			case "settings":
				this.menu=new Menu_settings;
				this.process=do_settingsmenu;
				bbs.node_action=NODE_DFLT;
				center.loadWallPaper(system.text_dir + "cshell/settings.*.bin");
				break;
			case "filedir":
				this.menu=new Menu_filedir(value);
				this.process=do_filedirmenu;
				bbs.node_action=NODE_XFER;
				center.loadWallPaper(system.text_dir + "cshell/file.*.bin");
				break;
			case "info":
				this.menu=new Menu_info;
				this.process=do_infomenu;
				center.loadWallPaper(system.text_dir + "cshell/info.*.bin");
				break;
			case "userlist":
				this.menu=new Menu_userlist;
				this.process=do_userlistmenu;
				center.loadWallPaper(system.text_dir + "cshell/userlist.*.bin");
				break;
			case "fileinfo":
				this.menu=new Menu_fileinfo;
				this.process=do_fileinfo;
				center.loadWallPaper(system.text_dir + "cshell/file.*.bin");
				break;
			case "chatsettings":
				this.menu=new Menu_chatsettings;
				this.process=do_chatsettings;
				center.loadWallPaper(system.text_dir + "cshell/chat.*.bin");
				break;
			case "searchmsgtxt":
				this.menu=new Menu_searchmsgtxt;
				this.process=do_searchmsgtxt;
				center.loadWallPaper(system.text_dir + "cshell/message.*.bin");
				break;
			case "scantoyou":
				this.menu=new Menu_yourmsgscan;
				this.process=do_scantoyou;
				center.loadWallPaper(system.text_dir + "cshell/message.*.bin");
				break;
			case "newscan":
				this.menu=new Menu_newmsgscan;
				this.process=do_newscan;
				center.loadWallPaper(system.text_dir + "cshell/message.*.bin");
				break;
			case "download":
				this.menu=new Menu_download;
				this.process=do_downloadmenu;
				center.loadWallPaper(system.text_dir + "cshell/file.*.bin");
				break;
			case "filesettings":
				this.menu=new Menu_filesettings(value);
				this.process=do_filesettings;
				center.loadWallPaper(system.text_dir + "cshell/file.*.bin");
				break;
			case "upload":
				this.menu=new Menu_upload;
				this.process=do_uploadmenu;
				center.loadWallPaper(system.text_dir + "cshell/file.*.bin");
				break;
			default:
				log("menu not found: " + name);
				return false;
		}
		this.title_shown=false;
	}
	
}

/* SHORTCUT COMMAND FUNCTIONS */
function CommandList()
{
	this["menu"]=loadMenu;
	this["chat"]=chatInput;
	this["xtrn"]=loadXtrn;
	this["logoff"]=logoff;
}

/* DEFAULT USER SETTINGS */
function Settings(list)
{
	this.shell_bg=BG_BROWN;
	this.shell_fg=BROWN;
	
	this.main_hkey_color=YELLOW;
	this.main_text_color=BLACK;
	this.main_height=screen_rows-2;
	this.main_width=screen_columns-20;
	
	this.menu_fg=LIGHTGRAY;
	this.menu_bg=BLACK;
	this.menu_hfg=LIGHTCYAN;
	this.menu_hbg=BLUE;
	this.menu_width=25;
	this.menu_x=2;
	this.menu_y=5;
	
	this.chat_local_color=CYAN;
	this.chat_remote_color=GREEN;
	this.chat_global_color=MAGENTA;
	this.chat_private_color=YELLOW;
	
	for(var s in list) {
		this[s]=list[s];
	}
}
function Favorites(list)
{
	for(var s in list) {
		var hkey=getHotkey(s);
		var parameters=list[s].split(",");
		this[hkey]=new Shortcut(s,parameters.shift());
		this[hkey].parameters=parameters;
	}
}
