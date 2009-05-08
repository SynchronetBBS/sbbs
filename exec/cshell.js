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

var root;
try { barfitty.barf(barf); } catch(e) { root = e.fileName; }
root = root.replace(/[^\/\\]*$/,"");
var logger=new Logger(root,"chatshell");
bbs.sys_status |= SS_PAUSEOFF;	
console.clear();
var fullredraw=false;

function Main()
{
	var clearinput=true;
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
				case KEY_LEFT:
					mainmenu.Main();
					break;
				case KEY_RIGHT:
					break;
				case ';':
					chatroom.Alert("Command (? For Help): ");
					if(!console.aborted) {
						var str=console.getstr("",40,K_EDIT);
						MainMenu.clear_screen();
						if(str=='?') {
							if(!user.compare_ars("SYSOP"))
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
								exit(0);
						}
					}
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
	Main();
}
function Cycle()
{
	chatroom.Cycle();
	userlist.UpdateList();
	clock.Update();
}
function Exit()
{
	bbs.sys_status&=~SS_PAUSEOFF;
	exit(0);
}
function Redraw()
{
	userlist.Redraw();
	chatroom.Redraw();
	clock.Redraw();
	sysinfo.Redraw();
}

function ChatRoom()
{
	this.x=2;
	this.y=9;
	this.rows=13;
	this.columns=59;
	this.menu;
	this.chat=new ChatEngine(root,"chatshell",logger);
	this.InitChat=function()
	{
		var rows=this.rows;
		var columns=this.columns;
		var posx=this.x;
		var posy=this.y;
		this.chat.Init("Main Menu",true,columns,rows,posx,posy,false,true,"\1k\1h",true);
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
	this.Resize=function(width,height,x,y)
	{
		var rows=this.rows;
		var columns=this.columns;
		var posx=this.x;
		var posy=this.y;
		if(width)
		{
			width+=2;
			columns-=width;
			if(x)
			{
				posx=x;
			}
			else posx+=width;
		}
		if(height)
		{
			height+=2;
			rows-=height;
			if(y)
			{
				posy=y;
			}
			else posy+=height;
		}
		this.chat.Resize(posx,posy,columns,rows);
		if(fullredraw) 
		{
			Redraw();
			fullredraw=false;
		}
		else this.Redraw();
	}
	this.InitMenu=function()
	{
		this.menu=new Menu(	this.chat.input_line.x,this.chat.input_line.y,"\1n","\1c\1h");
		var menu_items=[		"~Logoff Fast"					, 
								"~Help"							,
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
				case "D":
					Redraw();
					break;
				case "L":
					bbs.hangup();
					break;
				default:
					break;
			}
	}
	this.Help=function()
	{
		//TODO: write help file
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
	
	this.InitChat();
	this.InitMenu();
}
function MainMenu()
{
	const LBShell_Attr=0x37;
	const MessageWindow_Attr=7;
	const MessageTimeout=6000;		/* 100ths of a second */
	const posx=1;
	const posy=8;
	
	var use_bg=false;
	var MessageWindow=new Graphic(80,console.screen_rows,MessageWindow_Attr,' ');
	var bars80="\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4";
	var spaces80="                                                                               ";
	var msg_rows=0;
	var msg_timeouts=new Array();
	var menus_displayed=new Array();
	var lastmessage_time=0;
	var lastmessage_type=0;
	var orig_passthru=console.ctrlkey_passthru;
	var hangup_now=false;
	var done=0;
	
	this.Main=function()
	{
		var next_key='';
		while(1) 
		{
			var done=0;
			var key=next_key;
			var extra_select=false;
			next_key='';
			bbs.node_action=NODE_MAIN;
			if(key=='')
			{
				chatroom.Resize(mainbar.width);
				key=mainbar.getval()
			}
			else
				mainbar.draw();
			extra_select=false;
			if(key==KEY_RIGHT) 
			{
				chatroom.Resize();
				return;
			}
			if(key=='\x1b')
			{
				chatroom.Resize();
				return;
			}
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
					if(!show_filemenu()) 
					{
						chatroom.Resize();
						return;;
					}
					break;
				case 'S':
					if(!show_settingsmenu())
					{
						chatroom.Resize();
						return;;
					}
					break;
				case 'E':
					if(!show_emailmenu())
					{
						chatroom.Resize();
						return;;
					}
					break;
				case 'M':
					if(!show_messagemenu())
					{
						chatroom.Resize();
						return;;
					}
					break;
				case 'C':
					if(!show_chatmenu())
					{
						chatroom.Resize();
						return;;
					}
					break;
				case 'O':
					var curr_xtrnsec=0;
					var x_sec;
					var x_prog;
					done=false;
					var xtrnsec=new Xtrnsecs;
					chatroom.Resize(xtrnsec.width);
					menus_displayed.push(xtrnsec);
					while(!done) 
					{
						x_sec=xtrnsec.getval();
						if(x_sec=='\b' || x_sec=='\x7f' || x_sec=='\x1b')
							break;
						if(x_sec==KEY_RIGHT)
						{
							chatroom.Resize();
							return;
						}
						if(x_sec==KEY_LEFT)
						{
							break;
						}
						if(x_sec==ctrl('O')
								|| x_sec==ctrl('U')
								|| x_sec==ctrl('T')
								|| x_sec==ctrl('K')
								|| x_sec==ctrl('P')) {
							handle_a_ctrlkey(x_sec);
							continue;
						}
						curr_xtrnsec=parseInt(x_sec);
						var this_xtrnsec=new Xtrnsec(curr_xtrnsec);
						chatroom.Resize(this_xtrnsec.width);
						menus_displayed.push(this_xtrnsec);
						while(1) {
							x_prog=this_xtrnsec.getval();
							if(x_prog==KEY_LEFT || x_prog=='\b' || x_prog=='\x7f' || x_prog=='\x1b') 
							{
								done=1;
								break;
							}
							if(x_prog==KEY_RIGHT) 
							{
								chatroom.Resize();
								return;
							}
							if(x_prog==ctrl('O')
									|| x_prog==ctrl('U')
									|| x_prog==ctrl('T')
									|| x_prog==ctrl('K')
									|| x_prog==ctrl('P')) {
								handle_a_ctrlkey(x_prog);
								continue;
							}
							clear_screen();
							bbs.exec_xtrn(xtrn_area.sec_list[curr_xtrnsec].prog_list[parseInt(x_prog)].number);
							draw_main();
							xtrnsec.draw();
						}
						menus_displayed.pop();
					}
					menus_displayed.pop();
					break;
				case 'V':
					var infomenu=new Infomenu;
					chatroom.Resize(infomenu.width);
					menus_displayed.push(infomenu);
					infoloop: while(1) {
						key=infomenu.getval();
						switch(key) {
							case ctrl('O'): /* CTRL-O - Pause */
							case ctrl('U'): /* CTRL-U User List */
							case ctrl('T'): /* CTRL-T Time Info */
							case ctrl('K'): /* CTRL-K Control Key Menu */
							case ctrl('P'): /* Ctrl-P Messages */
								handle_a_ctrlkey(key);
								break;
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
							case KEY_LEFT:
								if(infomenu.items[infomenu.current].retval!='U') {
									done=1;
									break infoloop;
								}
								// Fall-through
							case 'U':
								var userlists=new Userlists;
								chatroom.Resize(userlists.width);
								menus_displayed.push(userlists);
								userlistloop: 
								while(1) 
								{
									key=userlists.getval();
									switch(key) {
										case ctrl('O'): /* CTRL-O - Pause */
										case ctrl('U'): /* CTRL-U User List */
										case ctrl('T'): /* CTRL-T Time Info */
										case ctrl('K'): /* CTRL-K Control Key Menu */
										case ctrl('P'): /* Ctrl-P Messages */
											handle_a_ctrlkey(key);
											break;
										case KEY_LEFT:
											break infoloop;
										case KEY_RIGHT:
										case '\b':
										case '\x7f':
										case '\x1b':
											chatroom.Resize();
											return;
										case 'L':
											clear_screen();
											bbs.list_logons();
											console.pause();
											draw_main();
											infomenu.draw();
											break;
										case 'S':
											clear_screen();
											bbs.list_users(UL_SUB);
											console.pause();
											draw_main();
											infomenu.draw();
											break;
										case 'A':
											clear_screen();
											bbs.list_users(UL_ALL);
											console.pause();
											draw_main();
											infomenu.draw();
											break;
									}
								}
								menus_displayed.pop();
								break;
							case 'T':
								clear_screen();
								bbs.text_sec();
								draw_main();
								break infoloop;
							case KEY_RIGHT:
								chatroom.Resize();
								return;
							case '\b':
							case '\x7f':
							case '\x1b':
								break infoloop;
						}
					}
					menus_displayed.pop();
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
					if(!extra_select)
						exit(1);
			}
		}
	
	}

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
		this.timeout=100;
		this.callback=message_callback;
	}
	Mainbar.prototype=new Lightbar;
	function Filemenu()
	{
		this.items=new Array();
		// Width of longest line with no dynamic variables
		var width=0;
		if(width < 12+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length)
			width=12+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length;
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
		this.timeout=100;
		this.callback=message_callback;
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
		this.timeout=100;
		this.callback=message_callback;
	}
	Filedirmenu.prototype=new Lightbar;
	function Fileinfo()
	{
		this.width=32;
		this.items=new Array();
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
		this.add("File |Transfer Policy","T",32);
		this.add("Information on |Directory","D",32);
		this.add("|Users With Access to Dir","U",32);
		this.add("|Your File Transfer Statistics","Y",32);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",12);
		}
		this.add("|< Previous Menu","",12);
		this.add(format_opt("Return to Chat",32,true),"",32);
		this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
		this.timeout=100;
		this.callback=message_callback;
	}
	Fileinfo.prototype=new Lightbar;
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
		this.timeout=100;
		this.callback=message_callback;
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
		this.timeout=100;
		this.callback=message_callback;
	}
	Emailmenu.prototype=new Lightbar;
	function Messagemenu()
	{
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
		this.timeout=100;
		this.callback=message_callback;
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
		this.timeout=100;
		this.callback=message_callback;
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
		this.timeout=100;
		this.callback=message_callback;
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
		this.timeout=100;
		this.callback=message_callback;
	}
	Xtrnsec.prototype=new Lightbar;
	function Infomenu()
	{
		this.width=25;
		this.items=new Array();
		this.xpos=posx;
		this.ypos=posy;
		this.lpadding="\xb3";
		this.rpadding="\xb3";
		this.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
		this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
		this.add("System |Information","I",25);
		this.add("Synchronet |Version Info","V",25);
		this.add("Info on |Sub-Board","S",25);
		this.add("|Your Statistics","Y",25);
		this.add("|User Lists","U",25);
		this.add("|Text Files","T",25);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",25);
		}
		this.add("|< Previous Menu","",25);
		this.add(format_opt("Return to Chat",width,true),"",width);
		this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
		this.timeout=100;
		this.callback=message_callback;
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
		this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
		this.add("|Logons Today","L",12);
		this.add("|Sub-Board","S",12);
		this.add("|All","A",12);
		while(this.items.length<console.screen_rows-(this.ypos+2))
		{
			this.add("","",12);
		}
		this.add("|< Previous Menu","",12);
		this.add(format_opt("Return to Chat",width,true),"",width);
		this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
		this.timeout=100;
		this.callback=message_callback;
	}
	Userlists.prototype=new Lightbar;

	function handle_a_ctrlkey(key)
	{
		var i;
		var pause=false;
		switch(key) {
			case ctrl('O'):	/* CTRL-O - Pause */
				break;
			case ctrl('L'):	/* CTRL-L - Global War Menu */
				clear_screen();
				bbs.menu("gwarmenu");
				break;
			case ctrl('U'):	/* CTRL-U User List */
			case ctrl('T'):	/* CTRL-T Time Info */
			case ctrl('K'):	/* CTRL-K Control Key Menu */
				pause=true;
			case ctrl('P'):	/* Ctrl-P Messages */
				clear_screen();
				console.handle_ctrlkey(key);
				if(pause)
					console.pause();
				draw_main();
				for(i=0; i<menus_displayed.length; i++)
					menus_displayed[i].draw();
				break;
		}
	}
	function get_message()
	{
		var rows=0;

		/* Update node action */
		if(bbs.node_action != system.node_list[bbs.node_num-1].action)
			system.node_list[bbs.node_num-1].action = bbs.node_action;

		/* Check for messages */
		if(system.node_list[bbs.node_num-1].misc & NODE_MSGW)
			rows+=MessageWindow.putmsg(1,MessageWindow.height,system.get_telegram(user.number),MessageWindow_Attr,true);
		if(system.node_list[bbs.node_num-1].misc & NODE_NMSG)
			rows+=MessageWindow.putmsg(1,MessageWindow.height,system.get_node_message(bbs.node_num),MessageWindow_Attr,true);

		/* Fix up node status */
		if(system.node_list[bbs.node_num-1].status==NODE_WFC) {
			log("NODE STATUS FIXUP");
			system.node_list[bbs.node_num-1].status=NODE_INUSE;
		}

		/* Check if user data has changed */
		if((system.node_list[bbs.node_num-1].misc & NODE_UDAT) && user.compare_ars("REST NOT G")) {
			user.cached=false;
			system.node_list[bbs.node_num-1].misc &= ~NODE_UDAT;
		}

		/* Interrupted? */
		if(system.node_list[bbs.node_num-1].misc & NODE_INTR) {
			rows+=MessageWindow.putmsg(1,MessageWindow.height,bbs.text(NodeLocked),MessageWindow_Attr,true);
			log("Interrupted");
			hangup_now=true;
		}

		/* Sysop Chat? */
		if(system.node_list[bbs.node_num-1].misc & NODE_LCHAT) {
			// TODO: No way of calling bbs.priave_chat(true)
			// bbs.private_chat();
			bbs.nodesync();
			draw_main();
			for(i=0; i<menus_displayed.length; i++)
				menus_displayed[i].draw();
		}

		/* New day? */
	//	if(!(system.status & SS_NEWDAY))
	//		bbs.nodesync();

		return(rows);
	}
	function message_callback()
	{
		var rows;
		var display=false;
		var i;
		var old_rows=msg_rows;

		rows=get_message();
		if(rows>0) {
			display=true;
			/* 
			 * ToDo: This currently assumes that the
			 * current menu is the only one protruding into the message window
			 * This would not be correct with (for example) 20 external areas
			 * and 20 externals in one area
			 */
			 
			/* Create new timeout object. */
			if(rows > console.screen_rows-1)
				rows=console.screen_rows-1;
			var timeout=new Object;
			timeout.ticks=0;
			timeout.rows=rows;
			msg_timeouts.push(timeout);
			msg_rows+=rows;
			if(msg_rows > console.screen_rows-1) {
				/* Find and remove older messages that have scrolled off the top (or at least partially). */
				for(i=0; i<msg_timeouts.length; i++) {
					timeout=msg_timeouts.shift();
					msg_rows -= timeout.rows;
					if(msg_rows < console.screen_rows-1)
						break;
				}
			}
			console.beep();
		}
		/* Increment the ticks and expire as necessary */
		for(i=0; i<msg_timeouts.length; i++) {
			msg_timeouts[i].ticks++;
			if(msg_timeouts[i].ticks>=MessageTimeout) {
				msg_rows -= msg_timeouts[i].rows;
				i--;
				msg_timeouts.shift();
				display=true;
			}
		}
		if(hangup_now)
			bbs.hangup();
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
	function show_filemenu()
	{
		var cur=1;
		var nd=false;
		bbs.node_action=NODE_XFER;
		while(1) {
			var filemenu=new Filemenu();
			chatroom.Resize(filemenu.width);
			var ret;
			var i;
			var j;
			filemenu.nodraw=nd;
			filemenu.current=cur;

			menus_displayed.push(filemenu);
			ret=filemenu.getval();
			file: 
			switch(ret) 
			{
				case ctrl('O'): /* CTRL-O - Pause */
				case ctrl('U'): /* CTRL-U User List */
				case ctrl('T'): /* CTRL-T Time Info */
				case ctrl('K'): /* CTRL-K Control Key Menu */
				case ctrl('P'): /* Ctrl-P Messages */
					handle_a_ctrlkey(ret);
					break;
				case KEY_LEFT:
					menus_displayed.pop();
					return true;
				case '\b':
				case '\x7f':
				case '\x1b':
					menus_displayed.pop();
					return true;
				case KEY_RIGHT:
					menus_displayed.pop();
					return false;
				case 'C':
					clear_screen();
					changedir: do {
						if(!file_area.lib_list.length)
							break changedir;
						while(1) {
							var orig_lib=bbs.curlib;
							i=0;
							j=0;
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
					var typemenu=new Filedirmenu(filemenu.xpos+filemenu.items[0].text.length, filemenu.current+1, true);
					chatroom.Resize(typemenu.width);
					menus_displayed.push(typemenu);
					while(1) {
						ret=typemenu.getval();
						switch(ret) {
							case ctrl('O'): /* CTRL-O - Pause */
							case ctrl('U'): /* CTRL-U User List */
							case ctrl('T'): /* CTRL-T Time Info */
							case ctrl('K'): /* CTRL-K Control Key Menu */
							case ctrl('P'): /* Ctrl-P Messages */
								handle_a_ctrlkey(ret);
								break;
							case 'A':
								clear_screen();
								console.putmsg("\r\nchNew File Scan (All)\r\n");
								bbs.scan_dirs(FL_ULTIME,true);
								console.pause();
								draw_main();
								filemenu.draw();
								break;
							case 'L':
								/* Scan this lib only */
								clear_screen();
								console.putmsg("\r\nchNew File Scan (Lib)\r\n");
								for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++)
									bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[i].number,FL_ULTIME);
								console.pause();
								draw_main();
								filemenu.draw();
								break;
							case 'D':
								/* Scan this dir only */
								clear_screen();
								console.putmsg("\r\nchNew File Scan (Dir)\r\n");
								bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number,FL_ULTIME);
								console.pause();
								draw_main();
								filemenu.draw();
								break;
							case 'N':
								// ToDo: Don't clear screen here, just do one line
								clear_screen();
								bbs.new_file_time=bbs.get_newscantime(bbs.new_file_time);
								draw_main();
								filemenu.draw();
								break;
							case KEY_RIGHT:
								menus_displayed.pop();
								menus_displayed.pop();
								return false;
							default:	// Anything else will escape.
								filemenu.nodraw=true;
								menus_displayed.pop();
								break file;
						}
					}
					break;
				case 'F':
					var typemenu=new Filedirmenu(filemenu.xpos+filemenu.items[0].text.length, filemenu.current+1, false);
					chatroom.Resize(typemenu.width);
					menus_displayed.push(typemenu);
					while(1) {
						ret=typemenu.getval();
						switch(ret) {
							case ctrl('O'): /* CTRL-O - Pause */
							case ctrl('U'): /* CTRL-U User List */
							case ctrl('T'): /* CTRL-T Time Info */
							case ctrl('K'): /* CTRL-K Control Key Menu */
							case ctrl('P'): /* Ctrl-P Messages */
								handle_a_ctrlkey(ret);
								break;
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
								filemenu.draw();
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
								filemenu.draw();
								break;
							case 'D':
								/* Scan this dir only */
								clear_screen();
								console.putmsg("\r\nchSearch for Filename(s) (Dir)\r\n");
								var spec=bbs.get_filespec();
								bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number,spec,0);
								console.pause();
								draw_main();
								filemenu.draw();
								break;
							case KEY_RIGHT:
								menus_displayed.pop();
								menus_displayed.pop();
								return false;
							default:	// Anything else will escape.
								filemenu.nodraw=true;
								menus_displayed.pop();
								break file;
						}
					}
					break;
				case 'T':
					var typemenu=new Filedirmenu(filemenu.xpos+filemenu.items[0].text.length, filemenu.current+1, false);
					chatroom.Resize(typemenu.width);
					menus_displayed.push(typemenu);
					while(1) {
						ret=typemenu.getval();
						switch(ret) {
							case ctrl('O'): /* CTRL-O - Pause */
							case ctrl('U'): /* CTRL-U User List */
							case ctrl('T'): /* CTRL-T Time Info */
							case ctrl('K'): /* CTRL-K Control Key Menu */
							case ctrl('P'): /* Ctrl-P Messages */
								handle_a_ctrlkey(ret);
								break;
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
								filemenu.draw();
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
								filemenu.draw();
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
								filemenu.draw();
								break;
							case KEY_RIGHT:
								menus_displayed.pop();
								menus_displayed.pop();
								return false;
							default:	// Anything else will escape.
								filemenu.nodraw=true;
								menus_displayed.pop();
								break file;
						}
					}
					break;
				case 'D':
					var typemenu=new Lightbar;
					chatroom.Resize(17);
					typemenu.xpos=posx;
					typemenu.ypos=posy;
					typemenu.lpadding="\xb3";
					typemenu.rpadding="\xb3";
					typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
					typemenu.add(top_bar(17),undefined,undefined,"","");
					typemenu.add('|Batch','B',17,undefined,undefined,bbs.batch_dnload_total<=0);
					typemenu.add('By |Name/File spec','N',17);
					typemenu.add('From |User','U',17);
					while(typemenu.items.length<console.screen_rows-(typemenu.ypos+2))
					{
						typemenu.add("","",17);
					}
					typemenu.add("|< Previous Menu","",17);
					typemenu.add(format_opt("Return to Chat",17,true),"",17);
					typemenu.add(bottom_bar(17),undefined,undefined,"","");
					typemenu.timeout=100;
					typemenu.callback=message_callback;
					menus_displayed.push(typemenu);
					while(1) {
						ret=typemenu.getval();
						switch(ret) {
							case ctrl('O'): /* CTRL-O - Pause */
							case ctrl('U'): /* CTRL-U User List */
							case ctrl('T'): /* CTRL-T Time Info */
							case ctrl('K'): /* CTRL-K Control Key Menu */
							case ctrl('P'): /* Ctrl-P Messages */
								handle_a_ctrlkey(ret);
								break;
							case 'B':
								clear_screen();
								bbs.batch_download();
								/* Redraw just in case */
								draw_main();
								filemenu.draw();
								break;
							case 'N':
								clear_screen();
								var spec=bbs.get_filespec();
								bbs.list_file_info(bbs.curdir,spec,FI_DOWNLOAD);
								draw_main();
								filemenu.draw();
								break;
							case 'U':
								clear_screen();
								bbs.list_file_info(bbs.curdir,spec,FI_USERXFER);
								draw_main();
								filemenu.draw();
								break;
							case KEY_RIGHT:
								menus_displayed.pop();
								menus_displayed.pop();
								return false;
							default:
								filemenu.nodraw=true;
								menus_displayed.pop();
								break file;
						}
					}
					break;
				case 'U':
					var typemenu=new Lightbar;
					var width=19;
					chatroom.Resize(width);
					typemenu.xpos=posx;
					typemenu.ypos=posy;
					typemenu.lpadding="\xb3";
					typemenu.rpadding="\xb3";
					typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
					if(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload || file_area.upload_dir==undefined) {
						if(width<9+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length)
							width=9+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length;
					}
					typemenu.add(top_bar(width),undefined,undefined,"","");
					if(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload || file_area.upload_dir==undefined) {
						typemenu.add('To |Dir ('+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name+')','C',width,undefined,undefined,!file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload);
					}
					else {
						typemenu.add('To Upload |Dir','P',width);
					}
					typemenu.add('To |Sysop Only','S',width,undefined,undefined,file_area.sysop_dir==undefined);
					typemenu.add('To Specific |User(s)','U',width,undefined,undefined,file_area.user_dir==undefined);
					while(typemenu.items.length<console.screen_rows-(typemenu.ypos+2))
					{
						typemenu.add("","",width);
					}
					typemenu.add("|< Previous Menu","",width);
					typemenu.add(format_opt("Return to Chat",width,true),"",width);
					typemenu.add(bottom_bar(width),undefined,undefined,"","");
					typemenu.timeout=100;
					typemenu.callback=message_callback;
					menus_displayed.push(typemenu);
					while(1) {
						ret=typemenu.getval();
						switch(ret) {
							case ctrl('O'): /* CTRL-O - Pause */
							case ctrl('U'): /* CTRL-U User List */
							case ctrl('T'): /* CTRL-T Time Info */
							case ctrl('K'): /* CTRL-K Control Key Menu */
							case ctrl('P'): /* Ctrl-P Messages */
								handle_a_ctrlkey(ret);
								break;
							case 'C':	// Current dir
								clear_screen();
								bbs.upload_file(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number);
								draw_main();
								filemenu.draw();
								break;
							case 'P':	// Upload dir
								clear_screen();
								bbs.upload_file(file_area.upload_dir);
								draw_main();
								filemenu.draw();
								break;
							case 'S':	// Sysop dir
								clear_screen();
								bbs.upload_file(file_area.sysop_dir);
								draw_main();
								filemenu.draw();
								break;
							case 'U':	// To user
								clear_screen();
								bbs.upload_file(file_area.user_dir);
								draw_main();
								filemenu.draw();
							case KEY_RIGHT:
								menus_displayed.pop();
								menus_displayed.pop();
								return false;
							default:
								filemenu.nodraw=true;
								menus_displayed.pop();
								break file;
						}
					}
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
					var typemenu=new Lightbar;
					var width=32;
					chatroom.Resize(width);
					typemenu.xpos=posx;
					typemenu.ypos=posy;
					typemenu.lpadding="\xb3";
					typemenu.rpadding="\xb3";
					typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
					typemenu.add(top_bar(width),undefined,undefined,"","");
					typemenu.add('File |Contents','C',width);
					typemenu.add('File |Information','I',width);
					typemenu.add('File Transfer |Policy','P',width);
					typemenu.add('|Directory Info','D',width);
					typemenu.add('|Users with Access to Dir','U',width);
					typemenu.add('Your File Transfer |Statistics','S',width);
					while(typemenu.items.length<console.screen_rows-(typemenu.ypos+2))
					{
						typemenu.add("","",width);
					}
					typemenu.add("|< Previous Menu","",width);
					typemenu.add(format_opt("Return to Chat",width,true),"",width);
					typemenu.add(bottom_bar(width),undefined,undefined,"","");
					typemenu.timeout=100;
					typemenu.callback=message_callback;
					menus_displayed.push(typemenu);
					while(1) {
						ret=typemenu.getval();
						switch(ret) {
							case ctrl('O'): /* CTRL-O - Pause */
							case ctrl('U'): /* CTRL-U User List */
							case ctrl('T'): /* CTRL-T Time Info */
							case ctrl('K'): /* CTRL-K Control Key Menu */
							case ctrl('P'): /* Ctrl-P Messages */
								handle_a_ctrlkey(ret);
								break;
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
											break file;
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
								filemenu.draw();
								break;
							case 'I':
								clear_screen();
								console.putmsg("\r\nchView File Information\r\n");
								str=bbs.get_filespec();
								if(str!=null) {
									if(!bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FI_INFO)) {
										console.putmsg(bbs.text(SearchingAllDirs));
										for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
											if(i==bbs.curdir)
												continue;
											if(bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FI_INFO))
												break;
										}
										if(i<file_area.lib_list[bbs.curlib].dir_list.length)
											break file;
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
								filemenu.draw();
								break;
							case 'P':
								clear_screen();
								bbs.xfer_policy();
								console.pause();
								draw_main();
								filemenu.draw();
								break;
							case 'D':
								clear_screen();
								bbs.dir_info();
								console.pause();
								draw_main();
								filemenu.draw();
								break;
							case 'U':
								clear_screen();
								bbs.list_users(UL_DIR);
								console.pause();
								draw_main();
								filemenu.draw();
								break;
							case 'S':
								break;
							case KEY_RIGHT:
								menus_displayed.pop();
								menus_displayed.pop();
								return false;
							default:
								menus_displayed.pop();
								filemenu.nodraw=true;
								break file;
						}
					}
					break;
				case 'S':
					var cur=1;
					while(1) {
						var typemenu=new Lightbar;
						var width=28;
						chatroom.Resize(width);
						if(user.settings&USER_EXTDESC)
							width++;
						typemenu.xpos=posx;
						typemenu.ypos=posy;
						typemenu.lpadding="\xb3";
						typemenu.rpadding="\xb3";
						typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
						typemenu.add(top_bar(width),undefined,undefined,"","");
						typemenu.add('Set Batch Flagging '+(user.settings&USER_BATCHFLAG?'Off':'On'),'B',width);
						typemenu.add('Set Extended Descriptions '+(user.settings&USER_EXTDESC?'Off':'On'),'S',width);
						while(typemenu.items.length<console.screen_rows-(typemenu.ypos+2))
						{
							typemenu.add("","",width);
						}
						typemenu.add("|< Previous Menu","",width);
						typemenu.add(format_opt("Return to Chat",width,true),"",width);
						typemenu.add(bottom_bar(width),undefined,undefined,"","");
						typemenu.current=cur;
						typemenu.timeout=100;
						typemenu.callback=message_callback;
						menus_displayed.push(typemenu);
						ret=typemenu.getval();
						switch(ret) {
							case ctrl('O'): /* CTRL-O - Pause */
							case ctrl('U'): /* CTRL-U User List */
							case ctrl('T'): /* CTRL-T Time Info */
							case ctrl('K'): /* CTRL-K Control Key Menu */
							case ctrl('P'): /* Ctrl-P Messages */
								handle_a_ctrlkey(ret);
								break;
							case 'B':
								user.settings ^= USER_BATCHFLAG;
								break;
							case 'S':
								/* Need to clear for shorter menu */
								user.settings ^= USER_EXTDESC;
								break;
							case KEY_RIGHT:
								menus_displayed.pop();
								menus_displayed.pop();
								return false;
							default:
								filemenu.nodraw=true;
								menus_displayed.pop();
								break file;
						}
						cur=typemenu.current;
						menus_displayed.pop();
					}
					break;
			}
			cur=filemenu.current;
			nd=filemenu.nodraw;
			menus_displayed.pop();
		}
	}
	function show_messagemenu()
	{
		var cur=1;
		var nd=false;

		while(!done) {
			var i;
			var j;
			var ret;
			var messagemenu=new Messagemenu();
			chatroom.Resize(messagemenu.width);
			messagemenu.current=cur;
			messagemenu.nodraw=nd;

			menus_displayed.push(messagemenu);
			ret=messagemenu.getval();
			if(ret==KEY_RIGHT) {
				if(messagemenu.items[messagemenu.current].text.substr(-2,2)==' >')
					ret=messagemenu.items[messagemenu.current].retval;
			}
			message: switch(ret) {
				case ctrl('O'): /* CTRL-O - Pause */
				case ctrl('U'): /* CTRL-U User List */
				case ctrl('T'): /* CTRL-T Time Info */
				case ctrl('K'): /* CTRL-K Control Key Menu */
				case ctrl('P'): /* Ctrl-P Messages */
					handle_a_ctrlkey(ret);
					break;
				case KEY_LEFT:
					menus_displayed.pop();
					return true;
				case '\b':
				case '\x7f':
				case '\x1b':
					menus_displayed.pop();
					return true;
				case KEY_RIGHT:
					menus_displayed.pop();
					return false;
				case 'C':
					clear_screen();
					if(!msg_area.grp_list.length)
						break;
					msgjump: while(1) {
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
						if(file_exists(system.text_dir+"menu/subs"+(bbs.curgrp+1)))
							bbs.menu("subs"+(bbs.curgrp+1));
						else {
							console.line_counter=0;
							console.clear();
							console.putmsg(format(bbs.text(SubLstHdr), msg_area.grp_list[j].description),P_SAVEATR);
							for(i=0; i<msg_area.grp_list[j].sub_list.length; i++) {
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
						}
						console.mnemonics(format(bbs.text(JoinWhichSub),bbs.cursub+1));
						i=console.getnum(msg_area.grp_list[j].sub_list.length);
						if(i==-1) {
							if(msg_area.grp_list.length==1) {
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
					var typemenu=new Lightbar;
					var width=29;
					chatroom.Resize(29);
					if(width<8+msg_area.grp_list[bbs.curgrp].name.length)
						width=8+msg_area.grp_list[bbs.curgrp].name.length;
					if(width<6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length)
						width=6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length;
					typemenu.xpos=posx;
					typemenu.ypos=posy;
					typemenu.lpadding="\xb3";
					typemenu.rpadding="\xb3";
					typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
					typemenu.add(top_bar(width),undefined,undefined,"","");
					typemenu.add('|All Message Areas','A',width);
					typemenu.add("|Group ("+msg_area.grp_list[bbs.curgrp].name+")",'G',width);
					typemenu.add('|Sub ('+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name+')','S',width);
					typemenu.add('Change New Scan |Configuration','C',width);
					typemenu.add('Change New Scan |Pointers','P',width);
					typemenu.add('|Reset New Scan Pointers','R',width);
					while(typemenu.items.length<console.screen_rows-(typemenu.ypos+2))
					{
						typemenu.add("","",width);
					}
					typemenu.add("|< Previous Menu","",width);
					typemenu.add(format_opt("Return to Chat",width,true),"",width);
					typemenu.add(bottom_bar(width),undefined,undefined,"","");
					typemenu.timeout=100;
					typemenu.callback=message_callback;
					menus_displayed.push(typemenu);
					while(1) {
						ret=typemenu.getval();
						switch(ret) {
							case ctrl('O'): /* CTRL-O - Pause */
							case ctrl('U'): /* CTRL-U User List */
							case ctrl('T'): /* CTRL-T Time Info */
							case ctrl('K'): /* CTRL-K Control Key Menu */
							case ctrl('P'): /* Ctrl-P Messages */
								handle_a_ctrlkey(ret);
								break;
							case 'A':
								clear_screen();
								console.putmsg("\r\n\x01c\x01hNew Message Scan\r\n");
								for(j=0; j<msg_area.grp_list.length; j++) {
									for(i=0; i<msg_area.grp_list[j].sub_list.length; i++)
										bbs.scan_posts(msg_area.grp_list[j].sub_list[i].number, SCAN_NEW);
								}
								draw_main();
								messagemenu.draw();
								break;
							case 'G':
								clear_screen();
								console.putmsg("\r\n\x01c\x01hNew Message Scan\r\n");
								for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++)
									bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_NEW);
								draw_main();
								messagemenu.draw();
								break;
							case 'S':
								clear_screen();
								console.putmsg("\r\n\x01c\x01hNew Message Scan\r\n");
								bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_NEW);
								draw_main();
								messagemenu.draw();
								break;
							case 'C':
								clear_screen();
								bbs.cfg_msg_scan(SCAN_CFG_NEW);
								draw_main();
								messagemenu.draw();
								break;
							case 'P':
								clear_screen();
								bbs.cfg_msg_ptrs(SCAN_CFG_NEW);
								draw_main();
								messagemenu.draw();
								break;
							case 'R':
								bbs.reinit_msg_ptrs()
								break;
							case KEY_RIGHT:
								menus_displayed.pop();
								menus_displayed.pop();
								return false;
							default:
								messagemenu.nodraw=true;
								menus_displayed.pop();
								break message;
						}
					}
					break;
				case 'Y':
					var typemenu=new Lightbar;
					var width=30;
					chatroom.Resize(width);
					if(width<8+msg_area.grp_list[bbs.curgrp].name.length)
						width=8+msg_area.grp_list[bbs.curgrp].name.length;
					if(width<6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length)
						width=6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length;
					typemenu.xpos=posx;
					typemenu.ypos=posy;
					typemenu.lpadding="\xb3";
					typemenu.rpadding="\xb3";
					typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
					typemenu.add(top_bar(width),undefined,undefined,"","");
					typemenu.add('|All Message Areas','A',width);
					typemenu.add("|Group ("+msg_area.grp_list[bbs.curgrp].name+")",'G',width);
					typemenu.add('|Sub ('+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name+')','S',width);
					typemenu.add('Change Your Scan |Configuration','C',width);
					while(typemenu.items.length<console.screen_rows-(typemenu.ypos+2))
					{
						typemenu.add("","",width);
					}
					typemenu.add("|< Previous Menu","",width);
					typemenu.add(format_opt("Return to Chat",width,true),"",width);
					typemenu.add(bottom_bar(width),undefined,undefined,"","");
					typemenu.timeout=100;
					typemenu.callback=message_callback;
					menus_displayed.push(typemenu);
					while(1) {
						ret=typemenu.getval();
						switch(ret) {
							case ctrl('O'): /* CTRL-O - Pause */
							case ctrl('U'): /* CTRL-U User List */
							case ctrl('T'): /* CTRL-T Time Info */
							case ctrl('K'): /* CTRL-K Control Key Menu */
							case ctrl('P'): /* Ctrl-P Messages */
								handle_a_ctrlkey(ret);
								break;
							case 'A':
								clear_screen();
								console.putmsg("\r\n\x01c\x01hYour Message Scan\r\n");
								for(j=0; j<msg_area.grp_list.length; j++) {
									for(i=0; i<msg_area.grp_list[j].sub_list.length; i++)
										bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_TOYOU);
								}
								draw_main();
								messagemenu.draw();
								break;
							case 'G':
								clear_screen();
								console.putmsg("\r\n\x01c\x01hYour Message Scan\r\n");
								for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++)
									bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_TOYOU);
								draw_main();
								messagemenu.draw();
								break;
							case 'S':
								clear_screen();
								console.putmsg("\r\n\x01c\x01hYour Message Scan\r\n");
								bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_TOYOU);
								draw_main();
								messagemenu.draw();
								break;
							case 'C':
								clear_screen();
								bbs.cfg_msg_scan(SCAN_CFG_TOYOU);
								draw_main();
								messagemenu.draw();
								break;
							case KEY_RIGHT:
								menus_displayed.pop();
								menus_displayed.pop();
								return false;
							default:
								messagemenu.nodraw=true;
								menus_displayed.pop();
								break message;
						}
					}
					break;
				case 'T':
					var typemenu=new Lightbar;
					var width=17;
					chatroom.Resize(width);
					if(width<8+msg_area.grp_list[bbs.curgrp].name.length)
						width=8+msg_area.grp_list[bbs.curgrp].name.length;
					if(width<6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length)
						width=6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length;
					typemenu.xpos=posx;
					typemenu.ypos=posy;
					typemenu.lpadding="\xb3";
					typemenu.rpadding="\xb3";
					typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
					typemenu.add(top_bar(width),undefined,undefined,"","");
					typemenu.add('|All Message Areas','A',width);
					typemenu.add("|Group ("+msg_area.grp_list[bbs.curgrp].name+")",'G',width);
					typemenu.add('|Sub ('+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name+')','S',width);
					while(typemenu.items.length<console.screen_rows-(typemenu.ypos+2))
					{
						typemenu.add("","",width);
					}
					typemenu.add("|< Previous Menu","",width);
					typemenu.add(format_opt("Return to Chat",width,true),"",width);
					typemenu.add(bottom_bar(width),undefined,undefined,"","");
					typemenu.timeout=100;
					typemenu.callback=message_callback;
					menus_displayed.push(typemenu);
					while(1) {
						ret=typemenu.getval();
						switch(ret) {
							case ctrl('O'): /* CTRL-O - Pause */
							case ctrl('U'): /* CTRL-U User List */
							case ctrl('T'): /* CTRL-T Time Info */
							case ctrl('K'): /* CTRL-K Control Key Menu */
							case ctrl('P'): /* Ctrl-P Messages */
								handle_a_ctrlkey(ret);
								break;
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
								messagemenu.draw();
								break;
							case 'G':
								clear_screen();
								console.putmsg("\r\n\x01c\x01hMessage Search\r\n");
								str=console.getstr("",40,K_LINE|K_UPPER);
								for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++)
									bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_FIND, str);
								draw_main();
								messagemenu.draw();
								break;
							case 'S':
								clear_screen();
								console.putmsg("\r\n\x01c\x01hMessage Search\r\n");
								str=console.getstr("",40,K_LINE|K_UPPER);
								bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_FIND, str);
								draw_main();
								messagemenu.draw();
								break;
							case KEY_RIGHT:
								menus_displayed.pop();
								menus_displayed.pop();
								return false;
							default:
								messagemenu.nodraw=true;
								menus_displayed.pop();
								break message;
						}
					}
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
			menus_displayed.pop();
		}
	}
	function show_emailmenu()
	{
		var cur=1;
		/* There's nothing dynamic, so we can fiddle this here */
		var emailmenu=new Emailmenu();
		chatroom.Resize(emailmenu.width);
		/* For consistency */
		emailmenu.current=cur;
		menus_displayed.push(emailmenu);

		while(1) {
			var i;
			var j;
			var ret;

			/* Nothing dynamic, so we don't need to save/restore nodraw */

			ret=emailmenu.getval();
			email: 
			switch(ret) 
			{
				case ctrl('O'): /* CTRL-O - Pause */
				case ctrl('U'): /* CTRL-U User List */
				case ctrl('T'): /* CTRL-T Time Info */
				case ctrl('K'): /* CTRL-K Control Key Menu */
				case ctrl('P'): /* Ctrl-P Messages */
					handle_a_ctrlkey(ret);
					break;
				case KEY_LEFT:
				case '\b':
				case '\x7f':
				case '\x1b':
					menus_displayed.pop();
					return true;
				case KEY_RIGHT:
					menus_displayed.pop();
					return false;
				case 'S':
					var typemenu=new Lightbar;
					var width=30;
					chatroom.Resize(width);
					typemenu.xpos=posx;
					typemenu.ypos=posy;
					typemenu.lpadding="\xb3";
					typemenu.rpadding="\xb3";
					typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
					typemenu.add(top_bar(width),undefined,undefined,"","");
					typemenu.add('To |Sysop','S',width,undefined,undefined,user.compare_ars("REST S"));
					typemenu.add('To |Local User','L',width,undefined,undefined,user.compare_ars("REST E"));
					typemenu.add('To Local User with |Attachment','A',width,undefined,undefined,user.compare_ars("REST E"));
					typemenu.add('To |Remote User','R',width,undefined,undefined,user.compare_ars("REST E OR REST M"));
					typemenu.add('To Remote User with A|ttachment','T',width,undefined,undefined,user.compare_ars("REST E OR REST M"));
					while(typemenu.items.length<console.screen_rows-(typemenu.ypos+2))
					{
						typemenu.add("","",width);
					}
					typemenu.add("|< Previous Menu","",width);
					typemenu.add(format_opt("Return to Chat",width,true),"",width);
					typemenu.add(bottom_bar(width),undefined,undefined,"","");
					typemenu.timeout=100;
					typemenu.callback=message_callback;
					menus_displayed.push(typemenu);
					while(1) 
					{
						ret=typemenu.getval();
						switch(ret) {
							case ctrl('O'): /* CTRL-O - Pause */
							case ctrl('U'): /* CTRL-U User List */
							case ctrl('T'): /* CTRL-T Time Info */
							case ctrl('K'): /* CTRL-K Control Key Menu */
							case ctrl('P'): /* Ctrl-P Messages */
								handle_a_ctrlkey(ret);
								break;
							case 'S':
								clear_screen();
								bbs.email(1,WM_EMAIL,bbs.text(ReFeedback));
								draw_main();
								emailmenu.draw();
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
								emailmenu.draw();
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
								emailmenu.draw();
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
								emailmenu.draw();
								break;
							case 'T':
								clear_screen();
								console.putmsg("\x01_\r\n\x01b\x01hE-mail (User name or number): \x01w");
								str=console.getstr("",40,K_UPRLWR);
								if(str!=null && str!="")
									bbs.netmail(str,WM_FILE);
								draw_main();
								emailmenu.draw();
								break;
							case KEY_RIGHT:
								menus_displayed.pop();
								menus_displayed.pop();
								return false;
							default:
								emailmenu.nodraw=true;
								menus_displayed.pop();
								break email;
						}
					}
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
			cur=emailmenu.current;
		}
	}
	function show_chatmenu()
	{
		var cur=1;
		/* There's nothing dynamic, so we can fiddle this here */
		var chatmenu=new Chatmenu();
		chatroom.Resize(chatmenu.width);
		/* For consistency */
		chatmenu.current=cur;
		menus_displayed.push(chatmenu);

		while(1) {
			var i;
			var j;
			var ret;

			/* Nothing dynamic, so we don't need to save/restore nodraw */

			ret=chatmenu.getval();
			chat: switch(ret) {
				case ctrl('O'): /* CTRL-O - Pause */
				case ctrl('U'): /* CTRL-U User List */
				case ctrl('T'): /* CTRL-T Time Info */
				case ctrl('K'): /* CTRL-K Control Key Menu */
				case ctrl('P'): /* Ctrl-P Messages */
					handle_a_ctrlkey(ret);
					break;
				case KEY_LEFT:
				case '\b':
				case '\x7f':
				case '\x1b':
					menus_displayed.pop();
					return true;
				case KEY_RIGHT:
					menus_displayed.pop();
					return false;
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
					while(1) {
						var typemenu=new Lightbar;
						var width=24;
						chatroom.Resize(width);
						if(user.chat_settings&CHAT_SPLITP)
							width++;
						typemenu.xpos=posx;
						typemenu.ypos=posy;
						typemenu.lpadding="\xb3";
						typemenu.rpadding="\xb3";
						typemenu.hotkeys=KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
						typemenu.add(top_bar(width),undefined,undefined,"","");
						typemenu.add("Set |Split Screen Chat "+(user.chat_settings&CHAT_SPLITP?"Off":"On"),'S',width);
						typemenu.add("Set A|vailability "+(user.chat_settings&CHAT_NOPAGE?"On":"Off"),'V',width);
						typemenu.add("Set Activity |Alerts "+(user.chat_settings&CHAT_NOACT?"On":"Off"),'A',width);
						while(typemenu.items.length<console.screen_rows-(typemenu.ypos+2))
						{
							typemenu.add("","",width);
						}
						typemenu.add("|< Previous Menu","",width);
						typemenu.add(format_opt("Return to Chat",width,true),"",width);
						typemenu.add(bottom_bar(width),undefined,undefined,"","");
						typemenu.timeout=100;
						typemenu.callback=message_callback;
						menus_displayed.push(typemenu);
						ret=typemenu.getval();
						switch(ret) {
							case ctrl('O'): /* CTRL-O - Pause */
							case ctrl('U'): /* CTRL-U User List */
							case ctrl('T'): /* CTRL-T Time Info */
							case ctrl('K'): /* CTRL-K Control Key Menu */
							case ctrl('P'): /* Ctrl-P Messages */
								handle_a_ctrlkey(ret);
								break;
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
							case KEY_RIGHT:
								menus_displayed.pop();
								menus_displayed.pop();
								return false;
							default:
								chatmenu.nodraw=true;
								menus_displayed.pop();
								break chat;
						}
						menus_displayed.pop();
					}
					break;
			}
			cur=chatmenu.current;
		}
	}
	function show_settingsmenu()
	{
		var settingsmenu=new Settingsmenu();
		chatroom.Resize(settingsmenu.width);
		var ret;

		menus_displayed.push(settingsmenu);
		while(1) 
		{
			ret=settingsmenu.getval();
			switch(ret) 
			{
				case ctrl('O'): /* CTRL-O - Pause */
				case ctrl('U'): /* CTRL-U User List */
				case ctrl('T'): /* CTRL-T Time Info */
				case ctrl('K'): /* CTRL-K Control Key Menu */
				case ctrl('P'): /* Ctrl-P Messages */
					handle_a_ctrlkey(ret);
					break;
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
				case KEY_RIGHT:
					menus_displayed.pop();
					return false;
				case KEY_LEFT:
				case '\b':
				case '\x7f':
				case '\x1b':
					menus_displayed.pop();
					return true;
			}
		}
	}
	
	var mainbar=new Mainbar();
}
function Log(text)
{
	logger.Log(text);
}
function InfoBox()
{
	this.x=chatroom.x-1;
	this.y=1;
	this.columns=chatroom.columns;
	this.rows=5;

	var sys=" \1n\1cSystem:\1h " + system.name;
	var sysop=" \1n\1c SysOp:\1h " + system.operator;
	var date=" \1n\1c  Date:\1h " + system.datestr();
	var loc=" \1n\1cLocation:\1h " + system.location;
	var addr=" \1n\1c Address:\1h " + system.inet_addr;
	var platform=" \1n\1cPlatform:\1h " + system.platform;
	
	this.Init=function()
	{
		this.DrawBox();
		this.DrawInfo();
	}
	this.Redraw=function()
	{
		this.DrawBox();
		this.DrawInfo();
	}
	this.DrawInfo=function()
	{
		console.gotoxy(this.x+1,this.y+2);
		console.putmsg(PrintPadded(sys,37) + addr);
		console.gotoxy(this.x+1,this.y+3);
		console.putmsg(PrintPadded(sysop,37) + loc);
		console.gotoxy(this.x+1,this.y+4);
		console.putmsg(PrintPadded(date,37) + platform);
	}
	this.DrawBox=function()
	{
		console.gotoxy(this.x,this.y);
		console.putmsg("\1n\1h\xDA");
		DrawLine(false,false,this.columns-6,"\1n\1h");
		console.putmsg("\1n\1h\xB4\1nINFO\1h\xC3\1n\1h\xBF");
		for(line = 1; line<=this.rows; line++)
		{
			console.gotoxy(this.x,this.y+line);
			printf("\1n\1h\xB3%*s\xB3",this.columns,"");
		}
		console.gotoxy(this.x,this.y + this.rows+1);
		console.putmsg("\1n\1h\xC0");
		DrawLine(false,false,this.columns,"\1n\1h");
		var spot=console.getxy();
		if(spot.y==24 && spot.x==80);
		else console.putmsg("\1n\1h\xD9");
	}
	this.Init();
}

clock=new DigitalClock(62,1,LIGHTBLUE);
mainmenu=new MainMenu();
chatroom=new ChatRoom();
sysinfo=new InfoBox();
userlist=new UserList(62,8,17,15);

Main();