/*
	JAVASCRIPT MULTI-USER CHAT ENGINE -- BY MCMLXXIX (Matt Johnson) 08/2008
	-----------------------------------------------
	FULLSCREEN DEFAULT OR WINDOWED MODE WITH SUPPLIED PARAMETERS
	//TODO: ADD SCROLLBACK HISTORY WINDOW FOR CHAT MESSAGES

	NOTE: 
	For a simple full screen chat, create a script like the following:
	
	load("chateng.js");
	var chat=new ChatEngine(root_directory,room_name);
	Chat(chat);
	
	This will call the following Chat() function 
	to handle the engine in its simplest form
	
	For more advanced chatting, see 
	ChatEngine this.Init() method
*/
function Chat(Engine,key)
{
	if(!Engine) Exit(0);
	if(key)
	{
		while(1)
		{
			if(!key) key=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			Engine.ProcessKey(key);
			switch(key)
			{
				case '\x02':	/* CTRL-B KEY_HOME */
				case '\x05':	/* CTRL-E KEY_END */
				case KEY_UP:
				case KEY_DOWN:
					Engine.ShowHistory(key);
					return true;
				case '\r':
					return true;
				case '\x1b':	
					return false;
				default:
					key="";
					break;
			}
		}
	}
	else
	{
		while(1)
		{
			key=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			Engine.ProcessKey(key);
			switch(key.toUpperCase())
			{
				case '\x1b':	
					return true;
				default:
					key="";
					break;
			}
		}
	}
}
//*************MAIN ENGINE*************
load("qengine.js");
load("scrollbar.js");

function ChatEngine(root,name,log_)
{
	this.root=		(root?root:Quit(100));
	this.name=		(name?name:"chat");
	this.chatlog=	(log_?log_:new Logger(this.root,this.name));
	this.room;

	//DEFAULT VALUES FOR FULL SCREEN CHAT MODE WITH NO DEDICATED INPUT LINE
	this.fullscreen=true;
	this.columns=	79;
	this.rows=		24;
	this.x=			1;
	this.y=			1;
	
	this.local_color=	"\1n\1g";
	this.remote_color=	"\1n\1c";

	this.boxed;
	this.input_line;
	this.buffer;
	this.messages;
	this.scrollbar;
	this.history=[];
	this.history_index=0;
	this.hFile=new File(this.root + this.name + ".his");
	this.queue=new DataQueue(this.root,this.name,this.chatlog);
	
	this.Init=function(room,input_line,columns,rows,posx,posy,boxed)
	{
		this.fullscreen=(columns?false:true);
		if(input_line)	this.input_line=input_line;
		if(columns) 	this.columns=columns;
		if(rows)		this.rows=rows;
		if(posx)		this.x=posx;
		if(posy)		this.y=posy;
		if(boxed)		this.boxed=boxed;
		if(room)		this.room=room;
		
		this.buffer="";
		this.messages=[];
		this.history=[];
	
		this.queue.Init(this.room,"");
		
		var fx=this.x+this.columns;
		this.scrollbar=new Scrollbar(fx,this.y,fx,this.y+this.rows-1,"\1y");
		this.LoadHistory();
		this.DrawLines();
		this.log("Chat Initialized: Room: " + this.room);
	}
	this.DrawLines=function()
	{
		if(!this.boxed) return;
		DrawLine(this.input_line.x-1,this.input_line.y-1,this.columns+2);
		if(this.y>1) DrawLine(this.x-1,this.y-1,this.columns+2);
		if(this.input_line.y<24) DrawLine(this.input_line.x-1,this.input_line.y+1,this.columns+2);
	}
	this.Cycle=function()
	{
		this.Receive();
		this.GetNotices();
	}
	this.ProcessKey=function(key)
	{
		this.Cycle();
		switch(key.toUpperCase())
		{
		//borrowed Deuce's feseditor.js
		case '\x00':	/* CTRL-@ (NULL) */
		case '\x02':	/* CTRL-B KEY_HOME */
		case '\x03':	/* CTRL-C (Center Line) */
		case '\x04':	/* CTRL-D (Quick Find in SyncEdit)*/
		case '\x05':	/* CTRL-E KEY_END */
		case '\x09':	/* CTRL-I TAB... ToDo expand to spaces */
		case '\x0b':	/* CTRL-K */
		case '\x0c':	/* CTRL-L (Insert Line) */
		case '\x0e':	/* CTRL-N */
		case '\x0f':	/* CTRL-O (Quick Save/exit in SyncEdit) */
		case '\x10':	/* CTRL-P */
		case '\x11':	/* CTRL-Q (XOff) (Quick Abort in SyncEdit) */
		case '\x12':	/* CTRL-R (Quick Redraw in SyncEdit) */
		case '\x13':	/* CTRL-S (Xon)  */
		case '\x14':	/* CTRL-T (Justify Line in SyncEdit) */
		case '\x15':	/* CTRL-U (Quick Quote in SyncEdit) */
		case '\x16':	/* CTRL-V (Toggle insert mode) */
		case '\x17':	/* CTRL-W (Delete Word) */
		case '\x18':	/* CTRL-X (PgDn in SyncEdit) */
		case '\x19':	/* CTRL-Y (Delete Line in SyncEdit) */
		case '\x1a':	/* CTRL-Z (EOF) (PgUp in SyncEdit)  */
		case '\x1c':	/* CTRL-\ (RegExp) */
		case '\x1f':	/* CTRL-_ Safe quick-abort*/
		case '\x7f':	/* DELETE */
		case '\x1b':	/* ESC (This should parse extra ANSI sequences) */
		case KEY_LEFT:
		case KEY_RIGHT:
			break;
		case KEY_UP:
		case KEY_DOWN:
			break;
		case '\b':
			this.BackSpace();
			break;
		case '\r':
			if(!this.fullscreen) this.ClearInputLine();
			else 
			{
				console.left(this.buffer.length)
				console.cleartoeol();
			}
			if(!console.strlen(RemoveSpaces(this.buffer))) 
			{
				this.buffer="";
				break;
			}
			var message={"message":this.buffer,"name":user.alias};
			this.buffer="";
			this.Send(message);
			break;
		default:
			if(key) this.Buffer(key);
			break;
		}
		return true;
	}
	this.StoreHistory=function()
	{
		this.log("Storing message history");
		for(msg in this.messages)
		{
			this.history.push(this.messages[msg]);
		}
		this.hFile.open(file_exists(this.hFile.name) ? 'r+':'w+'); 	
		for(his in this.history)
		{
			this.hFile.iniSetValue(this.room,his,this.history[his]);
		}
		this.hFile.close();
	}
	this.LoadHistory=function()
	{
		if(!file_exists(this.hFile.name)) return false;
		this.log("Loading message history");
		this.hFile.open('r'); 
		var history=hFile.iniGetKeys(this.room);
		for(his in history)
		{
			this.history.push(this.hFile.iniGetValue(history[his]));
		}
		this.hFile.close();
	}
	this.ShowHistory=function(key)
	{
		if(!this.history.length) return false;
		
		switch(key)
		{
			case '\x02':	/* CTRL-B KEY_HOME */
				this.history_index=this.history.length;
				break;
			case '\x05':	/* CTRL-E KEY_END */
				this.history_index=0;
				break;
			case KEY_DOWN:
				if(this.history_index>0) this.history_index--;
				break;
			case KEY_UP:
				if(this.history_index<this.history.length) this.history_index++;
				break;
		}
		var index=this.history.length-this.history_index;
		this.scrollbar.draw(index,this.history.length);
		var line=0;
		while(index<this.history.length && line<this.rows)
		{
			console.gotoxy(this.x,this.y+line);
			console.putmsg(this.history[index],P_SAVEATR);
			ClearLine(this.columns-console.strlen(strip_ctrl(this.history[index])));
			index++;
			line++;
		}
		var msg=0;
		while(line<this.rows)
		{
			console.gotoxy(this.x,this.y+line);
			console.putmsg(this.messages[msg],P_SAVEATR);
			ClearLine(this.columns-console.strlen(strip_ctrl(this.messages[msg])));
			line++;
			msg++;
		}
	}
	this.Alert=function(msg)
	{
		if(this.input_line)
		{
			this.ClearInputLine();
			console.gotoxy(this.input_line.x,this.input_line.y);
		}
		console.putmsg(msg);
	}
	this.ClearInputLine=function()
	{
		write(console.ansi(ANSI_NORMAL));
		if(this.input_line)	ClearLine(this.input_line.columns,this.input_line.x,this.input_line.y);
	}
	this.BackSpace=function()
	{
		if(this.buffer.length>0) 
		{
			write(console.ansi(ANSI_NORMAL));
			this.buffer=this.buffer.substring(0,this.buffer.length-1);
			if(this.fullscreen) 
			{
				console.left();
				console.cleartoeol();
			}
			this.Buffer();
		}
	}
	this.Buffer=function(key)
	{
		if(!this.fullscreen)
		{
			var offset=key?1:0;
			if(this.buffer.length+offset>this.input_line.columns)
			{
				var overrun=(this.buffer.length+offset-this.input_line.columns);
				var truncated=this.buffer.substr(overrun);
				console.gotoxy(this.input_line.x,this.input_line.y);
				console.putmsg(truncated,P_SAVEATR);
			}
			else if(this.buffer.length==this.input_line.columns && !key)
			{
				console.gotoxy(this.input_line.x,this.input_line.y);
				console.putmsg(this.buffer,P_SAVEATR);
			} 
			else 
			{
				offset=this.buffer.length;
				console.gotoxy(this.input_line.x+offset,this.input_line.y);
			}
		}
		if(key) 
		{
			console.putmsg(key,P_SAVEATR);
			this.buffer+=key;
		}
		if(this.buffer.length<this.columns)	ClearLine(this.columns-this.buffer.length);
	}
	this.Redraw=function()
	{
		this.Display();
		this.DrawLines();
		this.ClearInputLine();
		this.Buffer();
	}
	this.GetNotices=function()
	{
		for(notice in this.queue.notices)
		{
			this.Display(this.queue.notices[notice]);
		}
		this.queue.notices=[];
	}
	this.Display=function(text,color,user_)
	{
		write(console.ansi(ANSI_NORMAL));
		color=color?color:"\1r\1h";
		if(this.history.length) 
		{
			this.scrollbar.draw(1,1); //SHOW SCROLLBAR AT BOTTOM LIMIT
			this.history_index=0;
		}
		//FOR FULLSCREEN MODE WITH NO INPUT LINE, DISPLAY MESSAGE IN FULL AND RETURN
		if(this.fullscreen)	
		{
			if(!text) return;
			if(user_) console.putmsg(color + user_ + ": " + text + "\r\n",P_SAVEATR);
			else console.putmsg(color + text + "\r\n",P_SAVEATR);
			if(this.buffer.length) console.putmsg(this.local_color+this.buffer);
		}
		else
		{
			//FOR ALL OTHER MODES, STORE MESSAGES IN AN ARRAY AND HANDLE WRAPPING 
			if(text)
			{
				if(user_) this.Concat(color + user_+ "\1h: \1n" + color + text);
				else this.Concat(color + text);
			}
			console.gotoxy(this.x,this.y);
			for(msg in this.messages)
			{
				//TODO: FIX SINGLE LINE MODE... NOT FUNCTIONAL
				if(this.rows==1) console.gotoxy(this.x,this.y);
				
				else console.gotoxy(this.x,this.y+parseInt(msg));
				console.putmsg(this.messages[msg],P_SAVEATR);
				ClearLine(this.columns-console.strlen(strip_ctrl(this.messages[msg])));
			}
		}
	}
	this.Concat=function(text)
	{
		var array=word_wrap(text,this.columns,text.length,false);
		array=array.split(/[\r\n$]+/);
		for(item in array)
			if(array[item]!="") this.messages.push(RemoveSpaces(array[item]));
		while(this.messages.length>this.rows)
			this.history.push(this.messages.shift());
	}
	this.Receive=function()
	{
		var data=this.queue.ReceiveData(this.name);
		for(item in data)
		{
			if(data[item].info)
			{
				/*
					TODO: 	ADD PROCESSING RULES FOR NON-MESSAGE DATA
							I.E. PLAYERS ENTERING GAMES, GAME INVITES, PRIVATE CHAT INVITES
				*/
			}
			if(data[item].message) 
				this.Display(data[item].message,this.remote_color,data[item].name);
		}
	}
	this.Send=function(data)
	{
		this.queue.SendData(data,this.name);
		if(data.message) this.Display(data.message,this.local_color,user.alias);
	}
	this.log=function(text)
	{
		this.chatlog.Log(text);
	}
	function Quit(ERR)
	{
		if(ERR)
		{
			switch(ERR)
			{
				case 100:
					this.log("Error: No root directory specified");
					break;
				case 200:
					this.log("Error: No chat window parameters specified");
					break;
				default:
					this.log("Error: Unknown");
					break;
			}
		}
		exit(0);
	}
}

