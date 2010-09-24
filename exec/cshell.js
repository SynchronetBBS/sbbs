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
	*	WALLPAPER: 
	*		file: <menu_name>.<width>x<height>.bin
	*	RIGHT WINDOW ICON:
	*		file: icon.16x10.bin
	*
	*	NOTE: for a standard 80 x 24 terminal, menu wallpaper has a maximum
	*	size of 34 x 22, and the main wallpaper has a maximum size of 60 x 22,
	*	though these numbers largely depend on the size of the side menu and
	*	right window. The max width can be determined by this formula:
	*		WALLPAPER WIDTH = 
	*			SCREEN COLUMNS - RIGHT WINDOW WIDTH - (SIDE MENU WIDTH + 1) - 2
	*
	*		DEFAULT SIDE MENU WIDTH: 25 
	*		DEFAULT RIGHT WINDOW WIDTH: 18
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
var favorites=new Favorites();
var bottom=new BottomLine();
var right=new RightWindow();
var center=new MainWindow();
var left=new LeftWindow();

/* SHELL FUNCTIONS */
function init()
{
	loadSettings();
	
	bottom.init();
	right.init();
	center.init();
	left.init();
	
	if(favorites.items.length > 0) {
		loadMenu("favorites");
		showLeftWindow();
	}
	redraw();
}
function shell()
{
	while(1) {
		cycle();
				
		var cmd="";
		if(left.menu) {
			cmd=left.menu.getval();
			/* LEFT MENU COMMAND PROCESSING */
			switch(cmd)
			{
			case ctrl('R'): /* CTRL-R (Quick Redraw in SyncEdit) */
			case ctrl('O'): /* CTRL-O - Pause */
			case ctrl('U'): /* CTRL-U User List */
			case ctrl('T'): /* CTRL-T Time Info */
			case ctrl('K'): /* CTRL-K Control Key Menu */
			case ctrl('P'): /* Ctrl-P Messages */
				controlkeys.handle(key);
				continue;
			case KEY_UP:
				lightBarUp(left.menu);
				break;
			case KEY_DOWN:
				lightBarDown(left.menu);
				break;
			case KEY_HOME:
				lightBarHome(left.menu);
				break;
			case KEY_END:
				lightBarEnd(left.menu);
				break;
			case KEY_LEFT:
				previousMenu();
				break;
			case KEY_RIGHT:
			case "Q":
			case "\x1b":
				hideLeftWindow();
				center.restore();
				bottom.restore();
				continue;
			case "+":
				if(left.currentmenu == "favorites" || 
					left.currentmenu == "addfavorite" ||
					left.currentmenu == "removefavorite") {
					break;
				}
				favorites.mi=left.currentmenu;
				favorites.mt=left.menu.title;
				favorites.ii=left.menu.current;
				favorites.it=left.menu.items[favorites.ii].text.substr(3);
				loadMenu("addfavorite");
				break;
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
			continue;
		case " ":
			redraw();
			break;
		case "-":
			if(!favorites.items.length > 0) {
				break;
			}
			loadMenu("delfavorite");
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
		if(!left.menu_shown) showLeftWindow();
		if(!left.title_shown) left.drawTitle();
		
		if(menuinfo[left.currentmenu]) {
			menuinfo[left.currentmenu]();
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
	
	if(menuinfo[left.currentmenu]) {
		menuinfo[left.currentmenu]();
	}
	
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
	var title=splitPadded(sysname,sysop,console.screen_columns," ");

	console.home();
	console.attributes=settings.shell_bg;
	console.putmsg(title,P_SAVEATR);
}
function drawOutline()
{
	console.attributes=BG_BLACK + settings.shell_fg;
	var outline=splitPadded("\xDD","\xDE",console.screen_columns," ");
	for(var l=2;l<console.screen_rows;l++) {
		console.gotoxy(1,l);
		console.putmsg(outline,P_SAVEATR);
	}
}
function displayInfo(text)
{
	console.putmsg(text);
	console.popxy();
	console.down();
	console.pushxy();
}
function setPosition(x,y)
{
	console.gotoxy(x,y);
	console.pushxy();
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

/* LEFT MENU */
function hideLeftWindow()
{
	expandCenter(settings.menu_width+1,"left");
	delete left.menu;
	left.currentmenu="";
	left.menu_shown=false;
	left.title_shown=false;
	left.previous=[];
}
function showLeftWindow()
{
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
function addcmd(text,id,disabled)
{
	this.add(text,undefined,settings.menu_width,undefined,undefined,disabled);
	this.items[this.items.length-1].id=id;
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

/* MENU FUNCTIONS */
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
	full_redraw=true;
	
	/* We are going to a line-mode thing... re-enable CTRL keys. */
}

/* USER SETTINGS */
function saveSettings() 
{
	settings_file.open('w+',false);
	if(!settings_file.is_open) {
		log("error opening user settings",LOG_WARNING);
		return;
	}
	for(var f=0;f<favorites.items.length;f++) {
		var fav=favorites.items[f];
		var value=
			fav.menuID + "," + 
			fav.menuTitle + "," + 
			fav.itemID + "," + 
			fav.itemTitle + "," + 
			fav.xtrnsec;
		settings_file.iniSetValue("favorites",f,value);
	}
	settings_file.close();
}
function loadSettings()
{
	if(file_exists(settings_file.name)) {
		settings_file.open('r',true);
		if(!settings_file.is_open) {
			log("error opening user settings",LOG_WARNING);
			return;
		}
		var set=settings_file.iniGetObject("settings");
		var fav=settings_file.iniGetObject("favorites");
		settings=new Settings(set);
		favorites=new Favorites(fav);
		settings_file.close();
	}
}

init();
shell();