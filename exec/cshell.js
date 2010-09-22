/*
	JAVASCRIPT CHAT MENU SHELL - For Synchronet v3.15+ - Matt Johnson(2010)
	$ID: $ 

	
	*****************************************************************************
	*	IMPORTANT INFORMATION:
	*	Without customizing some binary "wallpaper" this shell is very boring
	*	If you wish to use this, I recommend creating a folder in your 
	*	system.text_dir called "cshell" ("/sbbs/text/cshell/") and another within 
	*	called "xtrn" ("/sbbs/text/cshell/xtrn/"). 
	*
	*	MAIN MENU WALLPAPER: 
	*		width: 60
	*		height: 22
	*		file: main.bin
	*	SUB-MENUS:
	*		width: 32
	*		height: 22
	*		files: message.bin, email.bin, system.bin, file.bin, etc....
	*	RIGHT WINDOW ICON:
	*		width: 16
	*		height: 10
	*		file: icon.bin
	******************************************************************************

	This shell relies on having commservice.js, commclient.js, and all other related
	"/sbbs/exec/" & "/sbbs/exec/load/" files up to date. The shell will automatically
	be linked to inter-BBS chat if you are running commservice.js as a node
	
	Much of the lightbar menu code has been taken from lbshell.js, by Deuce. 

*/

bbs.command_str='';	// clear STR (Contains the EXEC for default.js)
load("nodedefs.js");
load("lightbar.js");
load("str_cmds.js");
load("chateng.js");
load("graphic.js");
load("funclib.js");
load("clock.js");
load("cshell_menu.js");
load("cshell_obj.js");

const root=js.exec_dir;
var settings_file=new File(
	system.data_dir + 	"user/" + printPadded(user.number,4,"0","right") + ".shell.ini"
);

var screen_rows=console.screen_rows;
var screen_columns=console.screen_columns;
var full_redraw=false;
var orig_passthru=console.ctrlkey_passthru;
bbs.sys_status|=SS_MOFF;
bbs.sys_status|=SS_PAUSEOFF;	
console.ctrlkey_passthru="+KOPTU";

var cmdlist=new CommandList();
var settings=new Settings();
var shortcuts=new Favorites();
var bottom=new MainMenu();
var right=new RightWindow();
var center=new MainWindow();
var left=new SideMenu();

/* SHELL FUNCTIONS */
function init()
{
	loadSettings();
	loadFavorites();
	
	bottom.init();
	right.init();
	center.init();
	left.init();
	
	redraw();
}
function shell()
{
	while(1)
	{
		cycle();
				
		var cmd="";
		if(left.menu) {
			cmd=left.menu.getval();
			/* LEFT MENU COMMAND PROCESSING */
			switch(cmd)
			{
				case KEY_UP:
					lightBarUp(m);
					break;
				case KEY_DOWN:
					lightBarDown(m);
					break;
				case KEY_HOME:
					lightBarHome(m);
					break;
				case KEY_END:
					lightBarEnd(m);
					break;
				case KEY_LEFT:
					previousMenu();
					break;
				case KEY_RIGHT:
				case "Q":
				case "\x1b":
					hideSideMenu();
					center.restore();
					bottom.restore();
					continue;
				default:
					if(left.menu.items[cmd]) {
						var item_id=left.menu.items[cmd].id;
						left.process(item_id);
					}
					break;
			}
		}
		
		if(!cmd) cmd=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER,5);
		if(!cmd) continue;
		
		/* MAIN MENU PROCESS */
		switch(cmd)
		{
		case ctrl('R'): /* CTRL-R (Quick Redraw in SyncEdit) */
		case ctrl('O'): /* CTRL-O - Pause */
		case ctrl('U'): /* CTRL-U User List */
		case ctrl('T'): /* CTRL-T Time Info */
		case ctrl('K'): /* CTRL-K Control Key Menu */
		case ctrl('P'): /* Ctrl-P Messages */
			controlkeys.handle(key);
			break;
		case " ":
			redraw();
			break;
		case "\x1b":
		case "Q":
			return false;
		default:
			var sc=bottom.menu.items[cmd];
			if(sc && sc.enabled) {
				cmdlist[sc.command].apply(cmdlist[sc.command],sc.parameters);
			}
			break;
		}
		
		if(!left.menu) continue;
		var m=left.menu;
		if(!left.menu_shown) showSideMenu();
		if(!left.title_shown) left.drawTitle();
		
		/* DISPLAY CURRENT SECTION INFO */
		switch(left.currentmenu) {
			case "xtrnsec":
				showXtrnProgInfo(m);
				break;
			case "xtrnsecs":
				showXtrnSecInfo(m);
				break;
		}
	}
}
function cycle()
{
	right.cycle();
	center.cycle();
	left.cycle();
	if(full_redraw) {
		redraw();
	}
}

/* GLOBAL FUNCTIONS */
function getHotkey(item)
{
	var index=item.indexOf("|")+1;
	return item[index].toUpperCase();
}	
function redraw()
{
	console.clear(ANSI_NORMAL);
	drawTopline();
	drawOutline();
	
	center.redraw();
	right.redraw();
	left.redraw();
	bottom.redraw();
	
	full_redraw=false;
}
function drawTitle(x,y,str)
{
	console.attributes=BG_LIGHTGRAY + BLACK;
	var top=printPadded("",str.length,"\xDF");
	var middle=str;
	var bottom=printPadded("",str.length,"\xDC");
	console.gotoxy(x,y);
	console.pushxy();
	console.putmsg(top,P_SAVEATR);
	console.popxy();
	console.down();
	console.pushxy();
	console.putmsg(middle,P_SAVEATR);
	console.popxy();
	console.down();
	console.putmsg(bottom,P_SAVEATR);
}
function drawSeparator(x,y)
{
	console.gotoxy(x,y);
	console.pushxy();
	console.attributes=BG_BLACK + settings.shell_fg;
	for(var i=0;i<settings.main_height;i++) {
		console.putmsg("\xB3",P_SAVEATR);
		console.popxy();
		console.down();
		console.pushxy();
	}
}
function drawTopline()
{
	var sysname=" \1k" + system.name + " : \1b" + system.location;
	var sysop="\1kSysOp : \1b" + system.operator + " ";
	var title=splitPadded(sysname,sysop,80," ");

	console.home();
	console.attributes=settings.shell_bg;
	console.putmsg(title,P_SAVEATR);
}
function drawOutline()
{
	console.home();
	console.down();
	console.attributes=BG_BLACK + settings.shell_fg;
	var outline=splitPadded("\xDD","\xDE",80," ");
	for(var l=0;l<22;l++) {
		console.putmsg(outline,P_SAVEATR);
	}
}

/* MAIN MENU */
function hideChat()
{
	center.chat.chatroom.active=false;
	center.redraw();
	bottom.restore();
}
function showChat()
{
	center.in_chat=true;
	center.chat.chatroom.active=true;
	center.redraw();
	console.gotoxy(1,center.chat.input_line.y);
	console.cleartoeol(settings.shell_bg);
	console.right();
	console.attributes=BG_LIGHTGRAY;
	console.putmsg("\1k\xDDCHAT:",P_SAVEATR);
	console.attributes=settings.shell_bg;
	console.putmsg("\1k\xDD",P_SAVEATR);
	center.chat.input_line.clear();
}
 
/* MAIN WINDOW */
function stretchCenter(height)
{
	var rows=this.rows;
	rows+=height;
	center.chat.resize(undefined,rows,undefined,undefined);
}
function expandCenter(width,side)
{
	log("expanding chat: " + width);
	var cols=center.chat.chatroom.columns;
	var x=center.chat.chatroom.x;
	if(side=="left"){
		x-=width;
		cols+=width;
	}
	if(side=="right"){
		cols+=width;
	}
	center.chat.resize(cols,undefined,x,undefined);
}

/* MENU ITEM INFORMATION */
function showXtrnSecInfo(m)
{
	if(xtrn_area.sec_list[left.curr_xtrnsec]) {
		center.loadWallPaper(settings.main_width-(settings.menu_width+1),settings.main_height,
							system.text_dir + "cshell/xtrn/" + 
							xtrn_area.sec_list[left.curr_xtrnsec].code + ".bin");
		center.redraw();
	}
}
function showXtrnProgInfo(m)
{
	if(xtrn_area.sec_list[left.curr_xtrnsec].prog_list[m.current]) {
		center.loadWallPaper(settings.main_width-(settings.menu_width+1),settings.main_height,
							system.text_dir + "cshell/xtrn/" + 
							xtrn_area.sec_list[left.curr_xtrnsec].prog_list[m.current].code + ".bin");
		center.redraw();
	}
}

/* LEFT MENU */
function hideSideMenu()
{
	log("hiding side menu");
	expandCenter(settings.menu_width+1,"left");
	delete left.menu;
	left.currentmenu="";
	left.menu_shown=false;
	left.title_shown=false;
	left.previous=[];
}
function showSideMenu()
{
	log("showing side menu");
	expandCenter(-(settings.menu_width+1),"left");
	drawSeparator(settings.menu_width+2,2,settings.main_height);
	left.menu_shown=true;
}
function lightBarUp(m)
{
	do {
		if(m.current==0)
			m.current=m.items.length;
		m.current--;
	} while(m.items[m.current].disabled || m.items[m.current].retval==undefined);
}
function lightBarHome(m)
{
	m.current=0;
	while(m.items[m.current].disabled || m.items[m.current].retval==undefined) {
		m.current++;
		if(m.current==m.items.length)
			m.current=0;
	}
}
function lighBarEnd(m)
{
	left.menu.current=left.menu.items.length-1;
	while(left.menu.items[left.menu.current].disabled || left.menu.items[left.menu.current].retval==undefined) {
		if(m.current==0)
			m.current=m.items.length;
		m.current--;
	}
}
function lightBarDown(m)
{
	do {
		m.current++;
		if(m.current==m.items.length)
			m.current=0;
	} while(m.items[m.current].disabled || m.items[m.current].retval==undefined);
}
function previousMenu()
{
	left.previousMenu();
}

/* USER SETTINGS */
function saveSettings() 
{
	if(file_exists(settings_file.name)) {
		settings_file.open('w+',true);
		if(!settings_file.is_open) {
			log("error opening user settings",LOG_WARNING);
			return;
		}
		settings_file.close();
	}
}
function saveFavorites()
{
	if(file_exists(settings_file.name)) {
		settings_file.open('w+',true);
		if(!settings_file.is_open) {
			log("error opening user settings",LOG_WARNING);
			return;
		}
		for each(var s in this.shortcuts) {
			var values=[s.command];
			settings_file.iniSetValue("shortcuts",s.text,s.parameters.join(","));
		}
		settings_file.close();
	}
}
function loadSettings()
{
	if(file_exists(settings_file.name)) {
		settings_file.open('r',true);
		if(!settings_file.is_open) {
			log("error opening user settings",LOG_WARNING);
			return;
		}
		var data=settings_file.iniGetObject("settings");
		settings_file.close();
		settings=new Settings(data);
	}
}
function loadFavorites()
{
	if(file_exists(settings_file.name)) {
		settings_file.open('r',true);
		if(!settings_file.is_open) {
			log("error opening user settings",LOG_WARNING);
			return;
		}
		var data=settings_file.iniGetObject("shortcuts");
		settings_file.close();
		shortcuts=new Favorites(data);
	}
}
function add_favorite()
{
 // ToDo: create an interface for adding custom user commands
}

/* COMMAND LIST FUNCTIONS */
function loadXtrn()
{
}
function loadMenu()
{
	return left.loadMenu.apply(left,arguments);
}
function chatInput()
{
	showChat();
	while(center.in_chat) {
		cycle();
		var key=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
		if(!key) continue;
		
		switch(key) {
		case '\r':
		case '\n':
			if(!center.chat.input_line.buffer.length) {
				center.in_chat=false;
			}
			break;
		case '\x09':	/* CTRL-I TAB */
			center.toggleChannel();
			continue;
		case '\x1b':
			center.in_chat=false;
			break;
		default:
			break;
		}
		center.chat.processKey(key);
	}
	hideChat();
}
function logoff()
{
	if(bbs.batch_dnload_total) {
		if(console.yesno(bbs.text(Menu_downloadBatchQ))) {
			bbs.batch_download();
			bbs.logoff();
		}
	} else bbs.hangup();
}


init();
shell();