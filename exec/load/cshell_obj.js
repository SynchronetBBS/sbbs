/* MAIN MENU */
function BottomLine()		
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
		this.menu=new MainMenu(menu_items,9,console.screen_rows);	
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
function MainMenu(items,x,y)
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
function CommandList()
{
	this["menu"]=loadMenu;
	this["chat"]=chatInput;
	this["logoff"]=logoff;
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
	this.width=18;
	this.logo=new Graphic(this.width-2,10);
	this.alerts=[];
	this.update=true;
	
	this.x=console.screen_columns-this.width;
	this.y=2;
	
	this.init=function()
	{
		this.clock.init(this.x,this.y+10,LIGHTBLUE);
		this.logo.load(system.text_dir + "cshell/logo.16x10.bin");
	}
	this.redraw=function()
	{
		drawSeparator(console.screen_columns-this.width,2,console.screen_rows-2);
		this.clock.update(true);
		this.logo.draw(this.x+1,this.y);
		this.drawAlerts();
	}
	this.cycle=function()
	{
		this.clock.update();
		if(this.update) {
			this.drawAlerts();
		}
	}
	this.drawAlerts=function()
	{
		this.update=false;
	}
}

/* MAIN WINDOW */
function MainWindow()
{
	this.in_chat=false;
	this.wp=false;
	this.wp_shown=true;
	this.chat=new ChatEngine(root);
	
	this.init=function()
	{
		this.chat.init(settings.main_width,settings.main_height-3,2,5);
		this.chat.input_line.init(9,console.screen_rows,console.screen_columns-9,"\0017","\1k");
		this.chat.joinChan("BBS MAIN",user.alias,user.name);
		this.loadWallPaper(directory(system.text_dir + "cshell/main.*.bin")[0]);
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
	this.clear=function()
	{
		var posx=center.chat.chatroom.x;
		var posy=center.chat.chatroom.y-3;
		clearBlock(posx,posy,center.chat.chatroom.columns,center.chat.chatroom.rows+3);
		this.wp=false;
		this.wp_shown=false;
	}
	this.cycle=function()
	{
		this.chat.cycle();
	}
	this.restore=function()
	{
		this.loadWallPaper(directory(system.text_dir + "cshell/main.*.bin")[0]);
		this.redraw();
	}
	this.redraw=function()
	{
		if(this.in_chat) {
			this.drawChat();
		} else if(this.wp) {
			this.drawWallPaper();
		} else {
			this.clear();
		}
	}
	
	this.drawChat=function()
	{
		this.drawTitle();
		this.chat.redraw();
	}
	this.loadWallPaper=function(file)
	{
		var width=this.chat.chatroom.columns;
		var height=this.chat.chatroom.rows+3;
		if(file_exists(file)) {
			var size=file_getname(file).split(".")[1].split("x");
			if(size[0]) width=Number(size[0]);
			if(size[1]) height=Number(size[1]);
		}
		this.wp=new Graphic(width,height);
		this.wp.load(file);
	}
	this.drawWallPaper=function()
	{
		var posx=this.chat.chatroom.x;
		var posy=this.chat.chatroom.y-3;
		var gapx=this.chat.chatroom.columns-this.wp.width;
		var gapy=(this.chat.chatroom.rows+3)-this.wp.height;
		if(gapx != 0 || gapy != 0) {
			posx+=parseInt(gapx/2,10);
			posy+=parseInt(gapy/2,10);
		}
		if(this.wp.width > this.chat.chatroom.columns || 
			this.wp.height > this.chat.chatroom.rows+3) return false;
		this.wp.draw(posx,posy);
		this.wp_shown=true;
	}
}

/* SIDE MENU */
function LeftWindow()
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
	this.xtrnsec=0;
	this.currentmenu;
	this.menu;
	this.process;

	this.init=function()
	{
		SideBar.prototype=new Lightbar;
		for(var m in menuobj) {
			menuobj[m].prototype=new SideBar;
		}
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
	this.loadMenu=function()
	{
		if(this.currentmenu) {
			if(this.currentmenu==arguments[0]) return false;
			this.previous.push(this.currentmenu);
		}
		this.currentmenu=arguments[0];
		this.process=menucmd[arguments[0]];
		this.menu=new menuobj[arguments[0]];
		if(this.menu.no_action) {
			bbs.node_action=this.menu.node_action;
		}
		this.title_shown=false;
		if(center.wp_shown) center.clear();
	}
}
function SideBar()
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
		+"\b\x7f\x1b<>Q+-"
		+ctrl('O')
		+ctrl('U')
		+ctrl('T')
		+ctrl('K')
		+ctrl('P');
	for(var i in bottom.menu.items) {
		this.hotkeys+=i;
	}
	this.addcmd=addcmd;
	this.wp_shown=false;
}

/* DEFAULT USER SETTINGS */
function Settings(list)
{
	this.shell_bg=BG_BROWN;
	
	this.main_hkey_color=YELLOW;
	this.main_text_color=BLACK;
	
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

	this.main_height=console.screen_rows-2;
	this.main_width=console.screen_columns-20;
}
function Favorites(list)
{
	this.items=[];
	
	for(var s in list) {
		var parameters=list[s].split(",");
		var menuID=parameters.shift();
		var menuTitle=parameters.shift();
		var itemID=parameters.shift();
		var itemTitle=parameters.shift();
		var xtrnsec=parameters.shift();
		
		this.items.push(new Favorite(menuID,menuTitle,itemID,itemTitle,xtrnsec));
	}
}
function Favorite(menuID,menuTitle,itemID,itemTitle,xtrnsec)
{
	this.menuID=menuID;
	this.menuTitle=menuTitle;
	this.itemID=itemID;
	this.itemTitle=itemTitle;
	this.xtrnsec=xtrnsec;
}
