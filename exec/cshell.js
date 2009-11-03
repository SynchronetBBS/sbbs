/*
	JAVASCRIPT ChAT MENU SHELL
	For Synchronet v3.15+
	Matt Johnson(2009)
*/

bbs.command_str='';	// Clear STR (Contains the EXEC for default.js)
load("nodedefs.js");
load("lightbar.js");
load("str_cmds.js");
load("chateng.js");
load("graphic.js");
load("clock.js");
load("msgwndw.js");

var root;
try { barfitty.barf(barf); } catch(e) { root = e.fileName; }
root = root.replace(/[^\/\\]*$/,"") + "chat/";
mkdir(root);

var logger=new Logger(root,"cshell");
var orig_passthru=console.ctrlkey_passthru;
bbs.sys_status |= SS_PAUSEOFF;	
var fullredraw=false;
var screen_rows=console.screen_rows;
var screen_columns=console.screen_columns;
var windows={"info":"userlist","userlist":"chat","chat":"info"};

function Main()
{
	var use_bg=false;
	var clearinput=true;
	menulist.LoadMenu("main");
	
	function Main()
	{
		console.ctrlkey_passthru="+KOPTU";
		chatroom.Welcome();
		while(1)
		{
			Cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k)
			{
				if(clearinput) 
				{
					chatroom.ClearInputLine();
					clearinput=false;
				} 
				switch(k.toUpperCase())
				{
					case '\x09':	/* CTRL-I TAB...*/
						NextWindow("chat");
						break;
					case ctrl('O'): /* CTRL-O - Pause */
					case ctrl('U'): /* CTRL-U User List */
					case ctrl('T'): /* CTRL-T Time Info */
					case ctrl('K'): /* CTRL-K Control Key Menu */
					case ctrl('P'): /* Ctrl-P Messages */
						controlkeys.handle(k);
						break;
					case KEY_LEFT:
						Menu();
						break;
					case KEY_RIGHT:
						break;
					case '\x12':	/* CTRL-R (Quick Redraw in SyncEdit) */
						Redraw();
						break;
					case ';':
						if(!chatroom.chat.buffer.length) 
						{
							if(!console.aborted) 
							{
								var str=console.getstr("",40,K_EDIT);
								chatroom.Alert("Command (? For Help): ");
								if(str=='?') {
									if(!user.compare_ars("SYSOP") || (bbs.sys_status&SS_TMPSYSOP))
										str='HELP';
								}
								if(str=='?') {
									//TODO: list sysop commands in chat window
								}
								else {
									var oldshell=user.command_shell;
									str_cmds(str);
									/* Still using this shell? */
									if(user.command_shell != oldshell)
										Exit();
								}
							}					
						}
						else if(!Chat(k,chatroom.chat)) return;
						break;
					case "/":
						if(!chatroom.chat.buffer.length) 
						{
							chatroom.ListCommands();
							chatroom.Menu();
							chatroom.Redraw();
						}
						else if(!Chat(k,chatroom.chat)) return;
						break;
					case "\x1b":	
						Exit();
						break;
					default:
						if(!Chat(k,chatroom.chat)) return;
						break;
				}
			}
		}
	}
	function Menu()
	{
		var diff=0;
		var width=0;
		var next_key='';
		while(1) 
		{
			Cycle();
			var key=next_key;
			next_key='';
			if(key=='')
			{
				diff=menulist.menu.width-width+2;
				chatroom.Expand(-(diff),"left");
				key=menulist.menu.getval()
			}
			else menulist.menu.draw();
			width=menulist.menu.width+2;
			switch(key)
			{
				case ctrl('R'):	/* CTRL-R (Quick Redraw in SyncEdit) */
				case ctrl('O'): /* CTRL-O - Pause */
				case ctrl('U'): /* CTRL-U User List */
				case ctrl('T'): /* CTRL-T Time Info */
				case ctrl('K'): /* CTRL-K Control Key Menu */
				case ctrl('P'): /* Ctrl-P Messages */
					controlkeys.handle(key);
					break;
				case KEY_LEFT:
					menulist.PreviousMenu();
					break;
				case '\x09':	/* CTRL-I TAB... ToDo expand to spaces */
				case KEY_RIGHT:
				case "\x1b":
					diff=menulist.menu.width+2;
					chatroom.Expand(diff,"left");
					return;
				default:
					menulist.process(key);
					break;
			}
		}
	}
	Main();
}
function Cycle()
{
	if(console.screen_columns!=screen_columns)
	{
		Init();
		screen_rows=console.screen_rows;
		screen_columns=console.screen_columns;
	}
	else if(console.screen_rows!=screen_rows)
	{
		var r=console.screen_rows-screen_rows;
		screen_rows=console.screen_rows;
		chatroom.Stretch(r);
		menulist.Reload();
		userlist.Resize(undefined,r);
		Redraw();
	}
	chatroom.Cycle();
	sysinfo.Cycle();
	userlist.UpdateList();
}
function Exit()
{
	QuitQueue(KillFile,KillThread);
	bbs.sys_status&=~SS_PAUSEOFF;
	console.ctrlkey_passthru=orig_passthru;
	console.clear();
	exit(0);
}
function Redraw()
{
	console.clear();
	userlist.Redraw();
	chatroom.Redraw();
	sysinfo.Redraw();
	clock.Redraw();
}
function NextWindow()
{
	
}
function ChatRoom()
{
	this.x;
	this.y;
	this.rows;
	this.columns;
	this.menu;
	this.chat=new ChatEngine(root,"chatshell",logger);

	this.Init=function(x,y,c,r)
	{
		this.rows=r?r:console.screen_rows;
		this.columns=c?c:console.screen_columns;
		this.x=x?x:2;
		this.y=y?y:2;
		this.chat.Init("Main",true,this.columns,this.rows,this.x,this.y,false,true,"\1k\1h",true);
		this.InitMenu();
	}
	this.Welcome=function()
	{
		var welcome=[];
		welcome.push("\1b\1hWelcome to " + system.name + "!");
		welcome.push("\1k\1h-----------------------------------------------------");
		welcome.push("\1n\1cThis menu shell is a work in progress. Press the left");
		welcome.push("\1n\1carrow key at any time for the main menu or '/' for an");
		welcome.push("\1n\1cin-chat menu. Just start typing to chat with other users.");
		welcome.push("\1n\1cYou can quit and go back to the regular command shell");
		welcome.push("\1n\1cat any time by pressing 'Escape'. Right arrow key returns");
		welcome.push("\1n\1cto chat. Let me know of any bugs or issues. -- MCMLXXIX");
		welcome.push("\1n\1cshell: " + user.command_shell);
		this.chat.DisplayInfo(welcome);
		this.Alert("\1r\1h[Press 'Y' if you understand]");
		while(console.inkey(K_UPPER)!="Y");
		this.Redraw();
		
	}
	this.Alert=function(text)
	{
		this.chat.Alert(text);
	}
	this.ClearInputLine=function()
	{
		this.chat.ClearInputLine();
	}
	this.Cycle=function()
	{
		this.chat.Cycle();
	}
	this.Redraw=function()
	{
		this.chat.Redraw();
	}
	this.Stretch=function(height)
	{
		var rows=this.rows;
		rows+=height;
		this.Resize(undefined,rows);
	}
	this.Expand=function(width,side)
	{
		var cols=this.columns;
		var x=this.x;
		if(side=="left")
		{
			x-=width;
			cols+=width;
		}
		if(side=="right")
		{
			cols+=width;
		}
		this.Resize(cols,undefined,x);
	}
	this.Resize=function(cols,rows,x,y)
	{
		if(x)
		{
			this.x=x;
		}
		if(y)
		{
			this.y=y;
		}
		if(cols)
		{
			this.columns=cols;
		}
		if(rows)
		{
			this.rows=rows;
		}
		this.chat.Resize(this.x,this.y,this.columns,this.rows);
	}
	this.InitMenu=function()
	{
		this.menu=new Menu(	this.chat.input_line.x,this.chat.input_line.y,"\1n","\1c\1h");
		var menu_items=[		"~Logoff Fast"					, 
								"~Help"							,
								"Toggle ~user list"				,
								"Chat ~room list"				,
								"~Join chat room"				,
								"Re~draw"						];
		this.menu.add(menu_items);
	}
	this.ListCommands=function()
	{
		var list=this.menu.getList();
		this.chat.DisplayInfo(list);
	}
	this.Menu=function()
	{
		this.menu.displayHorizontal();
		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
		this.chat.ClearInputLine();
		if(this.menu.items[k] && this.menu.items[k].enabled) 
			switch(k.toUpperCase())
			{
				case "H":
					this.Help();
					break;
				case "C":
					if(sysinfo.clock.hidden)
					{
						sysinfo.clock.Unhide();
						userlist.Resize(undefined,-(sysinfo.clock.rows+2),sysinfo.clock.x,sysinfo.clock.y+sysinfo.clock.rows+2);
					}
					else 
					{
						sysinfo.clock.Hide();
						userlist.Resize(undefined,sysinfo.clock.rows+2,sysinfo.clock.x,sysinfo.clock.y);
					}
					break;
				case "U":
					if(userlist.hidden)
					{
						this.Expand(-(userlist.columns+2),"right");
						userlist.Unhide();
					}
					else
					{
						this.Expand(userlist.columns+2,"right");
						userlist.Hide();
					}
					break;
				case "R":
					this.ListChatRooms();
					break;
				case "J":
					this.Alert("\1nEnter room name: ");
					var room=console.getstr(20,K_NOSPIN|K_NOCRLF|K_UPRLWR);
					if(room.length)	this.chat.Init(room,true,this.columns,this.rows,this.x,this.y,false,true,"\1k\1h",true);
					break;
				case "D":
					Redraw();
					break;
				case "L":
					QuitQueue(KillFile,KillThread);
					bbs.hangup();
					break;
				default:
					break;
			}
	}
	this.ListChatRooms=function()
	{
		var array=userlist.ChannelList();
		this.chat.DisplayInfo(array);
		this.Alert("\1r\1h[Press any key]");
		while(console.inkey()=="");
		this.Redraw();
	}
	this.Help=function()
	{
		//TODO: write help file
		this.Welcome();
	}
	function Menu(x,y,color,hkey_color)		
	{								
		this.items=[];
		this.color=color;
		this.hkey_color=hkey_color;
		this.x=x;
		this.y=y;

		this.disable=function(items)
		{
			for(item in items)
			{
				this.items[items[item]].enabled=false;
			}
		}
		this.enable=function(items)
		{
			for(item in items)
			{
				this.items[items[item]].enabled=true;
			}
		}
		this.getHotKey=function(item)
		{
			var keyindex=item.indexOf("~")+1;
			return(item.charAt(keyindex));
		}	
		this.add=function(items)
		{
			for(i=0;i<items.length;i++)
			{
				var hotkey=this.getHotKey(items[i]);
				this.items[hotkey.toUpperCase()]=new MenuItem(items[i],this.color,hotkey,this.hkey_color);
			}
		}
		this.countEnabled=function()
		{
			var items=[];
			for(i in this.items)
			{
				if(this.items[i].enabled) items.push(i);
			}
			return items;
		}
		this.displayItems=function()
		{
			var enabled=this.countEnabled();
			if(!enabled.length) return false;
			console.gotoxy(this.x,this.y);
			for(e=0;e<enabled.length;e++)
			{
				console.putmsg(this.items[enabled[e]].text);
				if(e<enabled.length-1) write(console.ansi(ANSI_NORMAL) + " ");
			}
		}
		this.getList=function()
		{
			var list=[];
			list.push(this.color + "\1hMenu Commands:");
			var items=this.countEnabled();
			for(item in items)
			{
				var cmd=this.items[items[item]];
				var text=(cmd.displayColor + "[" + cmd.keyColor + cmd.hotkey.toUpperCase() + cmd.displayColor + "] ");
				text+=cmd.item.replace(("~" + cmd.hotkey) , (cmd.hotkey));
				list.push(text);
			}
			return list;
		}
		this.displayHorizontal=function()
		{
			var enabled=this.countEnabled();
			if(!enabled.length) return false;
			console.gotoxy(this.x,this.y);
			console.putmsg(this.color + "[");
			for(e=0;e<enabled.length;e++)
			{
				console.putmsg(this.hkey_color + this.items[enabled[e]].hotkey.toUpperCase());
				if(e<enabled.length-1) console.putmsg(this.color + ",");
			}
			console.putmsg(this.color + "]");
		}
	}
	function MenuItem(item,color,hotkey,hkey_color)
	{							
		this.item=color + item;
		this.displayColor=color;
		this.keyColor=hkey_color;
		this.hotkey=hotkey;
		this.enabled=true;
		
		this.Init=function()
		{
			this.text=this.item.replace(("~" + this.hotkey) , (this.keyColor + this.hotkey + this.displayColor));
		}
		this.Init();
	}
}
function MenuList()
{
	var bars80="\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4";
	var spaces80="                                                                               ";
	var msg_rows=0;
	var msg_timeouts=new Array();
	var menus_displayed=new Array();
	var lastmessage_time=0;
	var lastmessage_type=0;
	var hangup_now=false;
	var done=0;
	var previous=[];
	var curr_xtrnsec=0;
	const LBShell_Attr=0x37;
	const posx=1;
	const posy=8;
	
	this.currentmenu;
	this.menu;
	this.process;
	
	function Mainbar()
	{
		/* ToDo: They all need this... feels like a bug to ME */
		this.items=new Array();
		this.direction=0;
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		var width=16;
		this.width=width;
		this.add(top_bar(width),undefined,undefined,"","");
		this.hotkeys=KEY_RIGHT+"\x1b"+";"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add("|File","F",width,undefined,undefined,user.compare_ars("REST T"));
		this.add("|Messages","M",width);
		this.add("|Email","E",width,undefined,undefined,user.compare_ars("REST SE"));
		this.add("|Chat","C",width,undefined,undefined,user.compare_ars("REST C"));
		this.add("|Settings","S",width);
		this.add("|Online Games","O",width,undefined,undefined,user.compare_ars("REST X"));
		this.add("|View","V",width);
		this.add("|Quit","Q",width);
		this.add("Commands",";",width);
		while(this.items.length<console.screen_rows-(this.ypos+1))
		{
			this.add("","",width);
		}
		this.add(format_opt("Return to Chat",width,true),"",width);
		this.add(bottom_bar(width),undefined,undefined,"","");
		
		
	}
	Mainbar.prototype=new Lightbar;
	function Filemenu()
	{
		this.items=new Array();
		// Width of longest line with no dynamic variables
		var width=0;
		if(width < 14+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length)
			width=14+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length;
		this.width=width;
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add(top_bar(width),undefined,undefined,"","");
		this.add("|Change Directory","C",width
		);
		this.add("|List Dir ("+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name+")","L",width);
		this.add("Scan for |New Files","N",width);
		this.add("Search |Filenames","F",width);
		this.add("Search |Text in Desc.","T",width);
		this.add("|Download file(s)","D",width,undefined,undefined,user.compare_ars("REST D")
				|| (!file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_download));
		this.add("|Upload file(s)","U",width,undefined,undefined,user.compare_ars("REST U")
				|| ((!file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload)
				&& file_area.upload_dir==undefined));
		this.add("|Remove/Edit Files","R",width);
		this.add("View/Edit |Batch Queue","B",width,undefined,undefined
			// Disabled if you can't upload or download.
			// Disabled if no upload dir and no batch queue
			,(user.compare_ars("REST U AND REST D"))
				|| (bbs.batch_upload_total <= 0  
					&& bbs.batch_dnload_total <= 0 
					&& file_area.upload_dir==undefined));
		this.add("|View","V",width);
		this.add("|Settings","S",width);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",width);
		}
		this.add("|< Previous Menu","",width);
		this.add(format_opt("Return to Chat",width,true),"",width);
		this.add(bottom_bar(width),undefined,undefined,"","");
		
	}
	Filemenu.prototype=new Lightbar;
	function Filedirmenu(changenewscan)
	{
		this.items=new Array();
		var width=changenewscan?20:0;

		if(width<10+file_area.lib_list[bbs.curlib].name.length)
			width=10+file_area.lib_list[bbs.curlib].name.length;
		if(width<12+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length)
			width=12+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length;
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add(top_bar(width),undefined,undefined,"","");
		this.add("|All File Areas","A",width);
		this.add("|Library ("+file_area.lib_list[bbs.curlib].name+")","L",width);
		this.add("|Directory ("+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name+")","D",width);
		if(changenewscan)
			this.add("Change New Scan |Date","N",width);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",width);
		}
		this.add("|< Previous Menu","",width);
		this.add(format_opt("Return to Chat",width,true),"",width);
	}
	Filedirmenu.prototype=new Lightbar;
	function Settingsmenu()
	{
		var width=18;
		this.width=width;

		this.items=new Array();
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add(top_bar(width),undefined,undefined,"","");
		this.add("|User Configuration","U",width);
		this.add("Minute |Bank","B",width);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",width);
		}
		this.add("|< Previous Menu","",width);
		this.add(format_opt("Return to Chat",width,true),"",width);
		this.add(bottom_bar(width),undefined,undefined,"","");
	}
	Settingsmenu.prototype=new Lightbar;
	function Emailmenu()
	{
		var width=19;
		this.width=width;

		this.items=new Array();
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add(top_bar(width),undefined,undefined,"","");
		this.add("|Send Mail","S",width);
		this.add("|Read Inbox","R",width);
		this.add("Read Sent |Messages","M",width,undefined,undefined,user.compare_ars("REST K"));
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",width);
		}
		this.add("|< Previous Menu","",width);
		this.add(format_opt("Return to Chat",width,true),"",width);
		this.add(bottom_bar(width),undefined,undefined,"","");
		
		
	}
	Emailmenu.prototype=new Lightbar;
	function Messagemenu()
	{
		this.items=new Array();
		var width=31;
		this.width=width;

		if(width<8+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length)
			width=8+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length
		this.items=new Array();
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add(top_bar(width),undefined,undefined,"","");
		this.add("|Change Sub","C",width);
		this.add("|Read "+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name,"R",width);
		this.add("Scan For |New Messages","N",width);
		this.add("Scan For Messages To |You","Y",width);
		this.add("Search For |Text in Messages","T",width);
		this.add("|Post In "+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name,"P",width,undefined,undefined,user.compare_ars("REST P"));
		if(user.compare_ars("REST N") && (msg_area.grp_list[bbs.curgrp].sub_list[bbs.crusub] & (SUB_QNET|SUB_PNET|SUB_FIDO)))
			this.items[6].disabed=true;
		this.add("Read/Post |Auto-Message","A",width);
		this.add("|QWK Packet Transfer Menu","Q",width);
		this.add("|View Information on Sub","V",width);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",width);
		}
		this.add("|< Previous Menu","",width);
		this.add(format_opt("Return to Chat",width,true),"",width);
		this.add(bottom_bar(width),undefined,undefined,"","");
		
		
	}
	Messagemenu.prototype=new Lightbar;
	function Chatmenu()
	{
		var width=27;
		this.width=width;

		this.items=new Array();
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add(top_bar(width),undefined,undefined,"","");
		this.add("|Multinode Chat","M",width);
		this.add("|Private Node to Node Chat","P",width);
		this.add("|Chat With The SysOp","C",width);
		this.add("|Talk With The System Guru","T",width);
		this.add("|Finger A Remote User/System","F",width);
		this.add("I|RC Chat","R",width);
		this.add("InterBBS |Instant Messages","I",width);
		this.add("|Settings","S",width);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",width);
		}
		this.add("|< Previous Menu","",width);
		this.add(format_opt("Return to Chat",width,true),"",width);
		this.add(bottom_bar(width),undefined,undefined,"","");
		
		
	}
	Chatmenu.prototype=new Lightbar;
	function Xtrnsecs()
	{
		var hotkeys="1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*():;<>";
		this.items=new Array();
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		var xtrnsecwidth=0;
		var j;
		for(j=0; j<xtrn_area.sec_list.length && j<console.screen_rows-2; j++) {
			if(xtrn_area.sec_list[j].name.length > xtrnsecwidth)
				xtrnsecwidth=xtrn_area.sec_list[j].name.length;
		}
		xtrnsecwidth += 4;
		if(xtrnsecwidth>30)
			xtrnsecwidth=30;
		this.width=xtrnsecwidth;
		this.add("\xda"+bars80.substr(0,xtrnsecwidth)+"\xbf",undefined,undefined,"","");
		for(j=0; j<xtrn_area.sec_list.length; j++)
			this.add("|"+hotkeys.substr(j,1)+" "+xtrn_area.sec_list[j].name,j.toString(),xtrnsecwidth);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",xtrnsecwidth);
		}
		this.add("|< Previous Menu","",xtrnsecwidth);
		this.add(format_opt("Return to Chat",xtrnsecwidth,true),"",xtrnsecwidth);
		this.add("\xc0"+bars80.substr(0,xtrnsecwidth)+"\xd9",undefined,undefined,"","");
		
		
	}
	Xtrnsecs.prototype=new Lightbar;
	function Xtrnsec(sec)
	{
		var hotkeys="1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*():;<>";
		this.items=new Array();
		var j=0;

		xtrnsecprogwidth=0;
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		// Figure out the correct width
		for(j=0; j<xtrn_area.sec_list[sec].prog_list.length; j++) {
			if(xtrn_area.sec_list[sec].prog_list[j].name.length > xtrnsecprogwidth)
				xtrnsecprogwidth=xtrn_area.sec_list[sec].prog_list[j].name.length;
		}
		xtrnsecprogwidth+=2;
		if(xtrnsecprogwidth>25)
			xtrnsecprogwidth=25;
		else if(xtrnsecprogwidth<17)
			xtrnsecprogwidth=17;
		this.width=xtrnsecprogwidth;
		this.ypos=posy;
		this.xpos=posx;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.add("\xda"+bars80.substr(0,xtrnsecprogwidth)+"\xbf",undefined,undefined,"","");
		for(j=0; j<xtrn_area.sec_list[sec].prog_list.length && j<console.screen_rows-3; j++)
			this.add("|"+hotkeys.substr(j,1)+" "+xtrn_area.sec_list[sec].prog_list[j].name,j.toString(),xtrnsecprogwidth);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",xtrnsecprogwidth);
		}
		this.add("|< Previous Menu","",xtrnsecprogwidth);
		this.add(format_opt("Return to Chat",xtrnsecprogwidth,true),"",xtrnsecprogwidth);
		this.add("\xc0"+bars80.substr(0,xtrnsecprogwidth)+"\xd9",undefined,undefined,"","");
		
		
	}
	Xtrnsec.prototype=new Lightbar;
	function Infomenu()
	{
		var width=25;
		this.width=width;
		this.items=new Array();
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
		this.add("System |Information","I",width);
		this.add("Synchronet |Version Info","V",width);
		this.add("Info on |Sub-Board","S",width);
		this.add("|Your Statistics","Y",width);
		this.add("|User Lists","U",width);
		this.add("|Text Files","T",width);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",width);
		}
		this.add("|< Previous Menu","",width);
		this.add(format_opt("Return to Chat",width,true),"",width);
		this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
		
		
	}
	Infomenu.prototype=new Lightbar;
	function Userlists()
	{
		this.items=new Array();
		this.width=12;
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_RIGHT+KEY_LEFT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add(top_bar(this.width),undefined,undefined,"","");
		this.add("|Logons Today","L",12);
		this.add("|Sub-Board","S",12);
		this.add("|All","A",12);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",12);
		}
		this.add("|< Previous Menu","",12);
		this.add(format_opt("Return to Chat",width,true),"",width);
		this.add(bottom_bar(this.width),undefined,undefined,"","");
		
		
	}
	Userlists.prototype=new Lightbar;
	function Emailtargetmenu()
	{
		this.items=new Array();
		this.width=30;
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add(top_bar(this.width),undefined,undefined,"","");
		this.add('To |Sysop','S',this.width,undefined,undefined,user.compare_ars("REST S"));
		this.add('To |Local User','L',this.width,undefined,undefined,user.compare_ars("REST E"));
		this.add('To Local User with |Attachment','A',this.width,undefined,undefined,user.compare_ars("REST E"));
		this.add('To |Remote User','R',this.width,undefined,undefined,user.compare_ars("REST E OR REST M"));
		this.add('To Remote User with A|ttachment','T',this.width,undefined,undefined,user.compare_ars("REST E OR REST M"));
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",this.width);
		}
		this.add("|< Previous Menu","",this.width);
		this.add(format_opt("Return to Chat",this.width,true),"",this.width);
		this.add(bottom_bar(this.width),undefined,undefined,"","");
		
		
	}
	Emailtargetmenu.prototype=new Lightbar;
	function Download()
	{
		this.items=new Array();
		this.width=17;
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add(top_bar(this.width),undefined,undefined,"","");
		this.add('|Batch','B',this.width,undefined,undefined,bbs.batch_dnload_total<=0);
		this.add('By |Name/File spec','N',this.width);
		this.add('From |User','U',this.width);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",this.width);
		}
		this.add("|< Previous Menu","",this.width);
		this.add(format_opt("Return to Chat",this.width,true),"",this.width);
		this.add(bottom_bar(this.width),undefined,undefined,"","");
		
		
	}
	Download.prototype=new Lightbar;
	function Upload()
	{
		this.items=new Array();
		this.width=19;
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		if(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload || file_area.upload_dir==undefined) {
			if(this.width<9+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length)
				this.width=9+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length;
		}
		this.add(top_bar(this.width),undefined,undefined,"","");
		if(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload || file_area.upload_dir==undefined) {
			this.add('To |Dir ('+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name+')','C',this.width,undefined,undefined,!file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload);
		}
		else {
			this.add('To Upload |Dir','P',this.width);
		}
		this.add('To |Sysop Only','S',this.width,undefined,undefined,file_area.sysop_dir==undefined);
		this.add('To Specific |User(s)','U',this.width,undefined,undefined,file_area.user_dir==undefined);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",this.width);
		}
		this.add("|< Previous Menu","",this.width);
		this.add(format_opt("Return to Chat",this.width,true),"",this.width);
		this.add(bottom_bar(this.width),undefined,undefined,"","");
		
		
	}
	Upload.prototype=new Lightbar;
	function Fileinfo()
	{
		this.items=new Array();
		this.width=32;
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add(top_bar(this.width),undefined,undefined,"","");
		this.add('File |Contents','C',this.width);
		this.add('File |Information','I',this.width);
		this.add('File Transfer |Policy','P',this.width);
		this.add('|Directory Info','D',this.width);
		this.add('|Users with Access to Dir','U',this.width);
		this.add('Your File Transfer |Statistics','S',this.width);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",this.width);
		}
		this.add("|< Previous Menu","",this.width);
		this.add(format_opt("Return to Chat",this.width,true),"",this.width);
		this.add(bottom_bar(this.width),undefined,undefined,"","");
		
		
	}
	Fileinfo.prototype=new Lightbar;
	function Filesettings(value)
	{
		this.items=new Array();
		this.width=28;
		if(user.settings&USER_EXTDESC)
			this.width++;
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add(top_bar(this.width),undefined,undefined,"","");
		this.add('Set Batch Flagging '+(user.settings&USER_BATCHFLAG?'Off':'On'),'B',this.width);
		this.add('Set Extended Descriptions '+(user.settings&USER_EXTDESC?'Off':'On'),'S',this.width);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",this.width);
		}
		this.add("|< Previous Menu","",this.width);
		this.add(format_opt("Return to Chat",this.width,true),"",this.width);
		this.add(bottom_bar(this.width),undefined,undefined,"","");
		this.current=value;
		
		
	}
	Filesettings.prototype=new Lightbar;
	function Newscan()
	{
		this.items=new Array();
		this.width=29;
		if(this.width<8+msg_area.grp_list[bbs.curgrp].name.length)
			this.width=8+msg_area.grp_list[bbs.curgrp].name.length;
		if(this.width<6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length)
			this.width=6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length;
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add(top_bar(this.width),undefined,undefined,"","");
		this.add('|All Message Areas','A',this.width);
		this.add("|Group ("+msg_area.grp_list[bbs.curgrp].name+")",'G',this.width);
		this.add('|Sub ('+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name+')','S',this.width);
		this.add('Change New Scan |Configuration','C',this.width);
		this.add('Change New Scan |Pointers','P',this.width);
		this.add('|Reset New Scan Pointers','R',this.width);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",this.width);
		}
		this.add("|< Previous Menu","",this.width);
		this.add(format_opt("Return to Chat",this.width,true),"",this.width);
		this.add(bottom_bar(this.width),undefined,undefined,"","");
		
		
	}
	Newscan.prototype=new Lightbar;
	function Scantoyou()
	{
		this.items=new Array();
		this.width=30;
		if(this.width<8+msg_area.grp_list[bbs.curgrp].name.length)
			this.width=8+msg_area.grp_list[bbs.curgrp].name.length;
		if(this.width<6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length)
			this.width=6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length;
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add(top_bar(this.width),undefined,undefined,"","");
		this.add('|All Message Areas','A',this.width);
		this.add("|Group ("+msg_area.grp_list[bbs.curgrp].name+")",'G',this.width);
		this.add('|Sub ('+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name+')','S',this.width);
		this.add('Change Your Scan |Configuration','C',this.width);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",this.width);
		}
		this.add("|< Previous Menu","",this.width);
		this.add(format_opt("Return to Chat",this.width,true),"",this.width);
		this.add(bottom_bar(this.width),undefined,undefined,"","");
		
		
	}
	Scantoyou.prototype=new Lightbar;
	function Searchmsgtxt()
	{
		this.items=new Array();
		this.width=17;
		if(this.width<8+msg_area.grp_list[bbs.curgrp].name.length)
			this.width=8+msg_area.grp_list[bbs.curgrp].name.length;
		if(this.width<6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length)
			this.width=6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length;
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add(top_bar(this.width),undefined,undefined,"","");
		this.add('|All Message Areas','A',this.width);
		this.add("|Group ("+msg_area.grp_list[bbs.curgrp].name+")",'G',this.width);
		this.add('|Sub ('+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name+')','S',this.width);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",this.width);
		}
		this.add("|< Previous Menu","",this.width);
		this.add(format_opt("Return to Chat",this.width,true),"",this.width);
		this.add(bottom_bar(this.width),undefined,undefined,"","");
		
		
	}
	Searchmsgtxt.prototype=new Lightbar;
	function Chatsettings()
	{
		this.items=new Array();
		this.width=24;
		if(user.chat_settings&CHAT_SPLITP)
			this.width++;
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add(top_bar(this.width),undefined,undefined,"","");
		this.add("Set |Split Screen Chat "+(user.chat_settings&CHAT_SPLITP?"Off":"On"),'S',this.width);
		this.add("Set A|vailability "+(user.chat_settings&CHAT_NOPAGE?"On":"Off"),'V',this.width);
		this.add("Set Activity |Alerts "+(user.chat_settings&CHAT_NOACT?"On":"Off"),'A',this.width);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",this.width);
		}
		this.add("|< Previous Menu","",this.width);
		this.add(format_opt("Return to Chat",this.width,true),"",this.width);
		this.add(bottom_bar(this.width),undefined,undefined,"","");
		
		
	}
	Chatsettings.prototype=new Lightbar;
	
	function show_mainmenu(key)
	{
		previous.push("main");
		switch(key) 
		{
			case ctrl('O'): /* CTRL-O - Pause */
			case ctrl('U'): /* CTRL-U User List */
			case ctrl('T'): /* CTRL-T Time Info */
			case ctrl('K'): /* CTRL-K Control Key Menu */
			case ctrl('P'): /* Ctrl-P Messages */
				handle_a_ctrlkey(key);
				break;
			case 'F':
				this.LoadMenu("file");
				break;
			case 'S':
				this.LoadMenu("settings");
				break;
			case 'E':
				this.LoadMenu("email");
				break;
			case 'M':
				this.LoadMenu("message");
				break;
			case 'C':
				this.LoadMenu("chat");
				break;
			case 'O':
				this.LoadMenu("xtrnsecs");
				break;
			case 'V':
				this.LoadMenu("info");
				break;
			case 'L':
				if(bbs.batch_dnload_total) {
					if(console.yesno(bbs.text(DownloadBatchQ))) {
						bbs.batch_download();
						bbs.logoff();
					}
				}
				else
					bbs.hangup();
			case 'Q':
					exit(1);
		}
	}
	function show_infomenu(key)
	{
		switch(key) 
		{
			case 'I':
				clear_screen();
				bbs.sys_info();
				console.pause();
				draw_main();
				break;
			case 'V':
				clear_screen();
				bbs.ver();
				console.pause();
				draw_main();
				break;
			case 'S':
				clear_screen();
				bbs.sub_info();
				console.pause();
				draw_main();
				break;
			case 'Y':
				clear_screen();
				bbs.user_info();
				console.pause();
				draw_main();
				break;
			case 'U':
				this.LoadMenu("userlist");
				break;
			case 'T':
				clear_screen();
				bbs.text_sec();
				draw_main();
				break;
		}
	}
	function show_userlistmenu(key)
	{
		switch(key) 
		{
			case 'L':
				clear_screen();
				bbs.list_logons();
				console.pause();
				draw_main();
				break;
			case 'S':
				clear_screen();
				bbs.list_users(UL_SUB);
				console.pause();
				draw_main();
				break;
			case 'A':
				clear_screen();
				bbs.list_users(UL_ALL);
				console.pause();
				draw_main();
				break;
		}
	}
	function show_xtrnsecs(key)
	{
		previous.push("xtrnsecs");
		curr_xtrnsec=parseInt(key);
		this.LoadMenu("xtrnsec",curr_xtrnsec);
	}
	function show_xtrnsec(key)
	{
		clear_screen();
		bbs.exec_xtrn(xtrn_area.sec_list[curr_xtrnsec].prog_list[parseInt(key)].number);
		draw_main();
	}
	function show_filemenu(key)
	{
		previous.push("file");
		var cur=1;
		var nd=false;
		var i;
		var j;
		this.menu.nodraw=nd;
		this.menu.current=cur;
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
				draw_main();
				break;
			case 'L':
				clear_screen();
				bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number);
				console.pause();
				draw_main();
				break;
			case 'N':
				this.LoadMenu("filedir1",true);
				break;
			case 'F':
				this.LoadMenu("filedir2",true);
				break;
			case 'T':
				this.LoadMenu("filedir3",true);
				break;
			case 'D':
				this.LoadMenu("download");
				break;
			case 'U':
				this.LoadMenu("upload");
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
				draw_main();
				break;
			case 'B':
				console.attributes=LBShell_Attr;
				clear_screen();
				bbs.batch_menu();
				draw_main();
				break;
			case 'V':
				this.LoadMenu("fileinfo");
				break;
			case 'S':
				this.LoadMenu("filesettings",cur);
				break;
			default:
				break;
		}
		cur=this.menu.current;
		nd=this.menu.nodraw;
	}
	function show_fileinfo(key)
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
				draw_main();
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
				draw_main();
				break;
			case 'P':
				clear_screen();
				bbs.xfer_policy();
				console.pause();
				draw_main();
				break;
			case 'D':
				clear_screen();
				bbs.dir_info();
				console.pause();
				draw_main();
				break;
			case 'U':
				clear_screen();
				bbs.list_users(UL_DIR);
				console.pause();
				draw_main();
				break;
			case 'S':
				break;
			default:
				this.menu.nodraw=true;
				break;
		}
	}
	function show_filedirmenu1(key)
	{
		switch(key) 
		{
			case 'A':
				clear_screen();
				console.putmsg("\r\nchNew File Scan (All)\r\n");
				bbs.scan_dirs(FL_ULTIME,true);
				console.pause();
				draw_main();
				break;
			case 'L':
				/* Scan this lib only */
				clear_screen();
				console.putmsg("\r\nchNew File Scan (Lib)\r\n");
				for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++)
					bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[i].number,FL_ULTIME);
				console.pause();
				draw_main();
				break;
			case 'D':
				/* Scan this dir only */
				clear_screen();
				console.putmsg("\r\nchNew File Scan (Dir)\r\n");
				bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number,FL_ULTIME);
				console.pause();
				draw_main();
				break;
			case 'N':
				// ToDo: Don't clear screen here, just do one line
				clear_screen();
				bbs.new_file_time=bbs.get_newscantime(bbs.new_file_time);
				draw_main();
				break;
			default:	// Anything else will escape.
				this.menu.nodraw=true;
				break;
		}
	}
	function show_filedirmenu2(key)
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
				draw_main();
				break;
			case 'L':
				/* Scan this lib only */
				clear_screen();
					console.putmsg("\r\nchSearch for Filename(s) (Lib)\r\n");
				var spec=bbs.get_filespec();
				for(j=0;j<file_area.lib_list[bbs.curlib].dir_list.length;j++)
					bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[j].number,spec,0);
				console.pause();
				draw_main();
				break;
			case 'D':
				/* Scan this dir only */
				clear_screen();
				console.putmsg("\r\nchSearch for Filename(s) (Dir)\r\n");
				var spec=bbs.get_filespec();
				bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number,spec,0);
				console.pause();
				draw_main();
				break;
			default:	// Anything else will escape.
				this.menu.nodraw=true;
				break;
		}
	}
	function show_filedirmenu3(key)
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
				draw_main();
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
				draw_main();
				break;
			case 'D':
				/* Scan this dir only */
				clear_screen();
				console.putmsg("\r\nchSearch for Text in Description(s) (Dir)\r\n");
				console.putmsg(bbs.text(SearchStringPrompt));
				var spec=console.getstr(40,K_LINE|K_UPPER);
				bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number,spec,FL_FINDDESC);
				console.pause();
				draw_main();
				break;
			default:	// Anything else will escape.
				this.menu.nodraw=true;
				break;
		}
	}
	function show_uploadmenu(key)
	{
		switch(key) {
			case 'C':	// Current dir
				clear_screen();
				bbs.upload_file(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number);
				draw_main();
				break;
			case 'P':	// Upload dir
				clear_screen();
				bbs.upload_file(file_area.upload_dir);
				draw_main();
				break;
			case 'S':	// Sysop dir
				clear_screen();
				bbs.upload_file(file_area.sysop_dir);
				draw_main();
				break;
			case 'U':	// To user
				clear_screen();
				bbs.upload_file(file_area.user_dir);
				draw_main();
			default:
				this.menu.nodraw=true;
				break;
		}
	}
	function show_downloadmenu(key)
	{
		switch(key) 
		{
			case 'B':
				clear_screen();
				bbs.batch_download();
				draw_main();
				break;
			case 'N':
				clear_screen();
				var spec=bbs.get_filespec();
				bbs.list_file_info(bbs.curdir,spec,FI_DOWNLOAD);
				draw_main();
				break;
			case 'U':
				clear_screen();
				bbs.list_file_info(bbs.curdir,spec,FI_USERXFER);
				draw_main();
				break;
			default:
				this.menu.nodraw=true;
				break
		}
	}
	function show_filesettings(key)
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
				this.menu.nodraw=true;
				break;
		}
	}
	function show_messagemenu(key)
	{
		previous.push("message");
		var cur=1;
		var nd=false;
		var i;
		var j;
		this.menu.current=cur;
		this.menu.nodraw=nd;
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
				draw_main();
				break;
			case 'R':
				clear_screen();
				bbs.scan_posts();
				draw_main();
				break;
			case 'N':
				this.LoadMenu("newscan");
				break;
			case 'Y':
				this.LoadMenu("scantoyou");
				break;
			case 'T':
				this.LoadMenu("searchmsgtxt");
				break;
			case 'P':
				clear_screen();
				bbs.post_msg();
				draw_main();
				break;
			case 'A':
				clear_screen();
				bbs.auto_msg();
				draw_main();
				break;
			case 'Q':
				clear_screen();
				bbs.qwk_sec();
				draw_main();
				break;
			case 'V':
				clear_screen();
				bbs.sub_info();
				console.pause();
				draw_main();
				break;
		}
		cur=messagemenu.current;
		nd=messagemenu.nodraw;
	}
	function show_searchmsgtxt(key)
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
				draw_main();
				break;
			case 'G':
				clear_screen();
				console.putmsg("\r\n\x01c\x01hMessage Search\r\n");
				str=console.getstr("",40,K_LINE|K_UPPER);
				for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++)
					bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_FIND, str);
				draw_main();
				break;
			case 'S':
				clear_screen();
				console.putmsg("\r\n\x01c\x01hMessage Search\r\n");
				str=console.getstr("",40,K_LINE|K_UPPER);
				bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_FIND, str);
				draw_main();
				break;
			default:
				this.menu.nodraw=true;
				break;
		}
	}
	function show_scantoyou(key)
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
				draw_main();
				break;
			case 'G':
				clear_screen();
				console.putmsg("\r\n\x01c\x01hYour Message Scan\r\n");
				for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++)
					bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_TOYOU);
				draw_main();
				break;
			case 'S':
				clear_screen();
				console.putmsg("\r\n\x01c\x01hYour Message Scan\r\n");
				bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_TOYOU);
				draw_main();
				break;
			case 'C':
				clear_screen();
				bbs.cfg_msg_scan(SCAN_CFG_TOYOU);
				draw_main();
				break;
			default:
				this.menu.nodraw=true;
				break;
		}
	}
	function show_newscan(key)
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
				draw_main();
				break;
			case 'G':
				clear_screen();
				console.putmsg("\r\n\x01c\x01hNew Message Scan\r\n");
				for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++)
					bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_NEW);
				draw_main();
				break;
			case 'S':
				clear_screen();
				console.putmsg("\r\n\x01c\x01hNew Message Scan\r\n");
				bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_NEW);
				draw_main();
				break;
			case 'C':
				clear_screen();
				bbs.cfg_msg_scan(SCAN_CFG_NEW);
				draw_main();
				break;
			case 'P':
				clear_screen();
				bbs.cfg_msg_ptrs(SCAN_CFG_NEW);
				draw_main();
				break;
			case 'R':
				bbs.reinit_msg_ptrs()
				break;
			default:
				this.menu.nodraw=true;
				break;
		}
	}
	function show_emailtargetmenu(key)
	{
		switch(key) 
		{
			case 'S':
				clear_screen();
				bbs.email(1,WM_EMAIL,bbs.text(ReFeedback));
				draw_main();
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
				draw_main();
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
				draw_main();
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
				draw_main();
				break;
			case 'T':
				clear_screen();
				console.putmsg("\x01_\r\n\x01b\x01hE-mail (User name or number): \x01w");
				str=console.getstr("",40,K_UPRLWR);
				if(str!=null && str!="")
					bbs.netmail(str,WM_FILE);
				draw_main();
				break;
		}
	}
	function show_emailmenu(key)
	{
		previous.push("email");
		var cur=1;
		this.menu.current=cur;
		var i;
		var j;
		switch(key) 
		{
			case 'S':
				this.LoadMenu("emailtarget");
				break;
			case 'R':
				clear_screen();
				bbs.read_mail(MAIL_YOUR);
				console.pause();
				draw_main();
				break;
			case 'M':
				clear_screen();
				bbs.read_mail(MAIL_SENT);
				console.pause();
				draw_main();
				break;
		}
		cur=this.menu.current;
	}
	function show_chatmenu(key)
	{
		previous.push("chat");
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
				draw_main();
				break;
			case 'P':
				clear_screen();
				bbs.private_chat();
				draw_main();
				break;
			case 'C':
				clear_screen();
				if(!bbs.page_sysop())
					bbs.page_guru();
				draw_main();
				break;
			case 'T':
				clear_screen();
				bbs.page_guru();
				draw_main();
				break;
			case 'F':
				clear_screen();
				bbs.exec("?finger");
				console.pause();
				draw_main();
				break;
			case 'R':
				clear_screen();
				write("\001n\001y\001hServer and channel: ");
				str="irc.synchro.net 6667 #Synchronet";
				str=console.getstr(str, 50, K_EDIT|K_LINE|K_AUTODEL);
				if(!console.aborted)
					bbs.exec("?irc -a "+str);
				draw_main();
				break;
			case 'I':
				clear_screen();
				bbs.exec("?sbbsimsg");
				draw_main();
				break;
			case 'S':
				this.LoadMenu("chatsettings");
				break;
		}
		cur=this.menu.current;
	}
	function show_chatsettings(key)
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
				this.menu.nodraw=true;
				break;
		}
	}
	function show_settingsmenu(key)
	{
		previous.push("settings");
		switch(key) 
		{
			case 'U':
				clear_screen();
				var oldshell=user.command_shell;
				bbs.user_config();
				/* Still using this shell? */
				if(user.command_shell != oldshell)
					exit(0);
				draw_main();
				break;
			case 'B':
				clear_screen();
				bbs.time_bank();
				draw_main();
				break;
		}
	}

	function top_bar(width)
	{
		return("\xda"+bars80.substr(0,width)+"\xbf");
	}
	function bottom_bar(width)
	{
		return("\xc0"+bars80.substr(0,width)+"\xd9");
	}
	function format_opt(str, width, expand)
	{
		var opt=str;
		if(expand) {
			var cleaned=opt;
			cleaned=cleaned.replace(/\|/g,'');
			opt+=spaces80.substr(0,width-cleaned.length-2);
			opt+=' |>';
		}
		return(opt);
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

		console.attributes=7;
		console.line_counter=0;
		console.clear();
		/* We are going to a line-mode thing... re-enable CTRL keys. */
		console.ctrlkey_passthru=orig_passthru;
		fullredraw=true;
	}
	function draw_main()
	{
		if(fullredraw) 
		{
			Redraw();
			fullredraw=false;
		}
		/* Disable CTRL keys that we "know" how to handle. */
		console.ctrlkey_passthru="+KOPTU";
	}

	this.Reload=function()
	{
		this.LoadMenu(this.currentmenu);
	}
	this.PreviousMenu=function()
	{
		if(previous.length)
		{
			this.LoadMenu(previous.pop());
		}
	}
	this.LoadMenu=function(name,value)
	{
		this.currentmenu=name;
		switch(name)
		{
			case "xtrnsecs":
				this.menu=new Xtrnsecs;
				this.process=show_xtrnsecs;
				bbs.node_action=NODE_XTRN;
				break
			case "xtrnsec":
				this.menu=new Xtrnsec(value);
				this.process=show_xtrnsec;
				bbs.node_action=NODE_XTRN;
				break;
			case "file":
				this.menu=new Filemenu;
				this.process=show_filemenu;
				bbs.node_action=NODE_XFER;
				break;
			case "message":
				this.menu=new Messagemenu;
				this.process=show_messagemenu;
				break;
			case "chat":
				this.menu=new Chatmenu;
				this.process=show_chatmenu;
				bbs.node_action=NODE_CHAT;
				break;
			case "email":
				this.menu=new Emailmenu;
				this.process=show_emailmenu;
				break;
			case "emailtarget":
				this.menu=new Emailtargetmenu;
				this.process=show_emailtargetmenu;
				break;
			case "fileinfo":
				this.menu=new Fileinfo;
				this.process=show_fileinfo;
				bbs.node_action=NODE_XFER;
				break;
			case "settings":
				this.menu=new Settingsmenu;
				this.process=show_settingsmenu;
				bbs.node_action=NODE_DFLT;
				break;
			case "filedir":
				this.menu=new Filedirmenu(value);
				this.process=show_filedirmenu;
				bbs.node_action=NODE_XFER;
				break;
			case "main":
				this.menu=new Mainbar;
				this.process=show_mainmenu;
				bbs.node_action=NODE_MAIN;
				break;
			case "info":
				this.menu=new Infomenu;
				this.process=show_infomenu;
				break;
			case "userlist":
				this.menu=new Userlists;
				this.process=show_userlistmenu;
				break;
			case "fileinfo":
				this.menu=new Fileinfo;
				this.process=show_fileinfo;
				break;
			case "chatsettings":
				this.menu=new Chatsettings;
				this.process=show_chatsettings;
				break;
			case "searchmsgtxt":
				this.menu=new Searchmsgtxt;
				this.process=show_searchmsgtxt;
				break;
			case "scantoyou":
				this.menu=new Scantoyou;
				this.process=show_scantoyou;
				break;
			case "newscan":
				this.menu=new Newscan;
				this.process=show_newscan;
				break;
			case "download":
				this.menu=new Download;
				this.process=show_downloadmenu;
				break;
			case "filesettings":
				this.menu=new Filesettings(value);
				this.process=show_filesettings;
				break;
			case "upload":
				this.menu=new Upload;
				this.process=show_uploadmenu;
				break;
		}
	}
}
function ControlKeys()
{
	this.handle=function(key)
	{
		var pause=false;
		switch(key) {
			case ctrl('R'):
				Redraw();	
				break;
			case ctrl('O'):	/* CTRL-O - Pause */
				break;
			case ctrl('U'):	/* CTRL-U User List */
			case ctrl('T'):	/* CTRL-T Time Info */
			case ctrl('K'):	/* CTRL-K Control Key Menu */
				pause=true;
			case ctrl('P'):	/* Ctrl-P Messages */
				console.clear();
				console.handle_ctrlkey(key);
				if(pause)
					console.pause();
				break;
		}
		Redraw();
	}
}
function Log(text)
{
	logger.Log(text);
}
function InfoBox()
{
	this.attr=7;
	this.messagetimeout=6000;		/* 100ths of a second */
	this.x;
	this.y;
	this.clock;
	this.columns;
	this.rows;
	this.window;
	this.box;
	this.messages=0;

	this.Menu=function()
	{
		while(1) 
		{
			Cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			switch(k)
			{
				case '\x12':	/* CTRL-R (Quick Redraw in SyncEdit) */
						Redraw();
						break;
				case ctrl('O'): /* CTRL-O - Pause */
				case ctrl('U'): /* CTRL-U User List */
				case ctrl('T'): /* CTRL-T Time Info */
				case ctrl('K'): /* CTRL-K Control Key Menu */
				case ctrl('P'): /* Ctrl-P Messages */
					controlkeys.handle(key);
					break;
				case '\x09':	/* CTRL-I TAB... ToDo expand to spaces */
					NextWindow("info");
					break;
				case "\x1b":
					return;
				default:
					break;
			}
		}
	}
	this.Cycle=function()
	{
		this.GetMessage();
	}
	this.Init=function(x,y)
	{
		this.x=x?x:1;
		this.y=y?y:1;
		this.columns=(console.screen_columns-20)-this.x;
		this.rows=5;
		this.window=new Graphic(this.columns,this.rows,this.attr,' ');
		this.box=new Window("INFO",this.x,this.y,this.columns,this.rows);
		this.box.Draw(this.messages);
		this.DrawInfo();
	}
	this.Redraw=function()
	{
		this.box.Draw(this.messages);
		if(this.messages==0) this.DrawInfo();
	}
	this.DrawInfo=function()
	{
		var addr="\1n\1c Address:\1h " + system.inet_addr;
		var date="\1n\1c    Date:\1h " + system.datestr();
		var loc="\1n\1cLocation:\1h " + system.location;
		var logons="\1n\1c  Logons:\1h " + system.stats.total_logons;
		console.gotoxy(this.x+1,this.y+1);
		console.putmsg("\1b\1h    T H e  -  B R o K E N  -  B U B B L e  -  B B S");
		console.gotoxy(this.x+1,this.y+2);
		console.putmsg(PrintPadded(addr,32) +	"\1n\1cGlobal Commands\1h:");
		console.gotoxy(this.x+1,this.y+3);
		console.putmsg(PrintPadded(loc,32) + 	"\1n\1c[\1hCtrl-R\1n\1c] Redraw Screen");
		console.gotoxy(this.x+1,this.y+4);
		console.putmsg(PrintPadded(date,32) +	"\1n\1c[\1hCtrl-P\1n\1c] MSG/Telegram/Chat");
		console.gotoxy(this.x+1,this.y+5);
		console.putmsg(PrintPadded(logons,32) +"\1n\1c[\1hCtrl-U\1n\1c] Users Online");
	}
	this.GetMessage=function()
	{
		/* Update node action */
		if(bbs.node_action != system.node_list[bbs.node_num-1].action)
		{
			system.node_list[bbs.node_num-1].action = bbs.node_action;
			userlist.Redraw();
		}

		/* Check for messages */
		if(system.node_list[bbs.node_num-1].misc & NODE_MSGW)
		{
			this.messages++;
			var telegram=system.get_telegram(user.number);
			this.window.putmsg(1,1,telegram,this.attr,true);
			this.window.draw(this.x+1,this.y+1);
		}
		if(system.node_list[bbs.node_num-1].misc & NODE_NMSG)
		{
			this.messages++;
			var nodemsg=system.get_node_message(bbs.node_num);
			this.window.putmsg(1,1,nodemsg,this.attr,true);
			this.window.draw(this.x+1,this.y+1);
		}
		/* Interrupted? */
		if(system.node_list[bbs.node_num-1].misc & NODE_INTR) 
		{
			this.messages++;
			this.window.putmsg(1,1,bbs.text(NodeLocked),this.attr,true);
			this.window.draw(this.x+1,this.y+1);
			hangup_now=true;
		}

		/* Fix up node status */
		if(system.node_list[bbs.node_num-1].status==NODE_WFC) {
			system.node_list[bbs.node_num-1].status=NODE_INUSE;
		}

		/* Check if user data has changed */
		if((system.node_list[bbs.node_num-1].misc & NODE_UDAT) && user.compare_ars("REST NOT G")) {
			user.cached=false;
			system.node_list[bbs.node_num-1].misc &= ~NODE_UDAT;
		}


		/* Sysop Chat? */
		if(system.node_list[bbs.node_num-1].misc & NODE_LCHAT) {
			// TODO: No way of calling bbs.priave_chat(true)
			// bbs.private_chat();
			bbs.nodesync();
			draw_main();
		}

		/* New day? */
	//	if(!(system.status & SS_NEWDAY))
	//		bbs.nodesync();
	}
}
function Init()
{
	console.clear();
	sysinfo.Init(1,1);
	clock.Init(console.screen_columns-18,sysinfo.y,LIGHTBLUE);
	userlist.Init(console.screen_columns-18,sysinfo.y+sysinfo.rows+2,17,console.screen_rows-(clock.y+clock.rows+3));
	chatroom.Init(2,sysinfo.y+sysinfo.rows+3,console.screen_columns-(userlist.columns+4),console.screen_rows-(sysinfo.y+sysinfo.rows+5));
}

var controlkeys=new ControlKeys();
var clock=new DigitalClock();
var menulist=new MenuList();
var sysinfo=new InfoBox();
var chatroom=new ChatRoom();
var userlist=new UserList();

Init();
Main();