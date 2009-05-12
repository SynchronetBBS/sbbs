/*
	JAVASCRIPT MULTI-USER CHAT ENGINE -- BY MCMLXXIX (Matt Johnson) 08/2008
	-----------------------------------------------
	FULLSCREEN DEFAULT OR WINDOWED MODE WITH SUPPLIED PARAMETERS
	//TODO: ADD SCROLLBACK HISTORY WINDOW FOR CHAT MESSAGES

	NOTE: 
	For a simple full screen chat, create a script like the following:
	
	load("chateng.js");
	var chat=new ChatEngine(root_directory,room_name,log_object);
	Chat(chat);
	
	This will call the following Chat() function 
	to handle the engine in its simplest form
	
	For more advanced chatting, see 
	ChatEngine this.Init() method
*/
function Chat(key,engine)
{
	if(!engine) exit(0);
	if(key)
	{
		engine.ProcessKey(key);
		switch(key)
		{
			case '\x1b':	
				return false;
			default:
				return true;
		}
	}
	else
	{
		while(1)
		{
			key=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(key) engine.ProcessKey(key);
			switch(key.toUpperCase())
			{
				case '\x1b':	
					return false;
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
var queue;

function ChatEngine(root,name,logger,stream)
{
	this.root=			(root?root:"/sbbs/");
	this.name=			(name?name:"chat");
	this.chatlog=		(logger?logger:false);
	queue=				(stream?stream:new DataQueue(this.root,this.name,this.chatlog));
	this.room;
	this.fullscreen;
	this.columns;
	this.rows;
	this.x;
	this.y;
	this.boxed;
	this.input_line;
	this.buffer;
	this.scrollbar;
	this.local_color=	"\1n\1g";
	this.remote_color=	"\1n\1c";
	this.notice_color=	"\1n\1r";
	this.messages=		[];
	this.history_index=	0;
	this.history_file;
	
	
	// USEFUL METHODS 
	this.Init=function(room,input_line,columns,rows,posx,posy,lined,boxed,scrollbar_color,userlist) //NOTE: DESTROYS BUFFER AND MESSAGE LIST
	{
												//DEFAULT SETTINGS
		this.columns=columns?columns:			79;			//chat window width
		this.rows=rows?rows:					24;			//chat window height
		this.x=posx?posx:						1;			//top left corner x
		this.y=posy?posy:						1;			//top left corner y
		this.lined=lined?lined:					false;		//frame chat window? (boolean)
		this.boxed=boxed?boxed:					false;		//frame chat window? (boolean)
		this.room=room?room:					"default";	//room name (for lobby style interface)
		this.userlist=userlist?userlist:		false;		//separate user list window (boolean)
		this.input_line=input_line?input_line:	false; 	//EX: inputline={'x':1,'y':1,'columns':40};
		this.buffer="";
		this.messages=[];
		if(this.input_line)
		{
			var x=this.x;
			var y=this.y+this.rows+1;
			this.input_line={'x':x,'y':y};
			this.fullscreen=false;
			this.scrollbar=new Scrollbar(this.x+this.columns,this.y,this.rows,"vertical",scrollbar_color?scrollbar_color:"\1k"); 
		}
		else this.fullscreen=true;
		this.history_file=new File(this.root + this.room + ".his");		
		queue.Init(this.room,"");
		this.LoadHistory();
		this.DrawLines();
		this.DrawBox();
		this.Display();
		this.Log("Chat Room Initialized: " + this.room);
	}
	this.Resize=function(x,y,columns,rows) //NOTE: DOES NOT DESTROY BUFFER OR MESSAGE LIST
	{
		if(this.lined) ClearLine(this.columns+2,this.x-1,this.y-1);
		this.ClearChatWindow();
		if(x) this.x=x;
		if(y) this.y=y;
		if(columns) this.columns=columns;
		if(rows) this.rows=rows;
		if(this.input_line)
		{
			this.input_line.x=this.x;
			this.input_line.y=this.y+this.rows+1;
		}
		this.ClearChatWindow();
		this.Redraw();
		this.scrollbar=new Scrollbar(this.x+this.columns,this.y,this.rows,"vertical",this.scrollbar.color); 
	}
	this.Cycle=function()
	{
		this.Receive();
		this.GetNotices();
	}
	this.FindUser=function(id)
	{
		return queue.FindUser(id);
	}
	this.ClearChatWindow=function() //CLEARS THE ENTIRE CHAT WINDOW
	{
		ClearBlock(this.x,this.y,this.columns,this.rows);
	}
	this.DisplayInfo=function(array) //DISPLAYS A TEMPORARY MESSAGE IN THE CHAT WINDOW (NOT STORED)
	{
		this.ClearChatWindow();
		var newarray=[];
		for(line in array)
		{
			var newlines=this.Concat(array[line]);
			for(newline in newlines)
			{
				newarray.push(newlines[newline]);
			}
		}
		for(item=0;item<newarray.length;item++)
		{
			console.gotoxy(this.x,this.y+item);
			console.putmsg(newarray[item],P_SAVEATR);
		}
	}
	this.Alert=function(msg) //DISPLAYS A MESSAGE ON THE INPUT LINE (OR CURRENT LINE IN FULLSCREEN MODE)
	{
		if(this.input_line)
		{
			this.ClearInputLine();
			console.gotoxy(this.input_line.x,this.input_line.y);
		}
		console.putmsg(msg);
	}
	this.AddNotice=function(msg)
	{
		queue.notices.push(msg);
	}
	this.ClearInputLine=function()
	{
		write(console.ansi(ANSI_NORMAL));
		if(this.input_line)	ClearLine(this.columns,this.input_line.x,this.input_line.y);
	}
	this.Redraw=function()
	{
		ClearBlock(this.x,this.y,this.columns,this.rows);
		this.DrawLines();
		this.DrawBox();
		this.Display();
		this.ClearInputLine();
		this.Buffer();
	}
	this.Send=function(data)
	{
		queue.SendData(data,this.name);
		if(data.message) 
		{
			this.Display(data.message,this.local_color,user.alias);
			this.StoreHistory(data.message);
		}
	}
	
	
	
	//INTERNAL METHODS
	this.ShowHistory=function(key) //ACTIVATE MESSAGE HISTORY SCROLLBACK
	{
		var output=[];
		for(txt in this.messages)
		{
			var array=this.Concat(this.messages[txt]);
			for(item in array)
			{
				output.push(array[item]);
			}
		}
		if(output.length<=this.rows) return;
		switch(key)
		{
			case '\x02':	/* CTRL-B KEY_HOME */
				this.history_index=output.length;
				break;
			case '\x05':	/* CTRL-E KEY_END */
				this.history_index=0;
				break;
			case KEY_DOWN:
				if(this.history_index>0) this.history_index--;
				break;
			case KEY_UP:
				if(this.history_index+this.rows<output.length) this.history_index++;
				break;
		}
		var index=output.length-this.history_index-this.rows;
		this.scrollbar.draw(index,output.length-this.rows);
		var line=0;
		while(index<output.length && line<this.rows)
		{
			console.gotoxy(this.x,this.y+line);
			console.putmsg(output[index],P_SAVEATR);
			var length=console.strlen(strip_ctrl(output[index]));
			if(length<this.columns) ClearLine(this.columns-length);
			index++;
			line++;
		}
	}
	this.StoreHistory=function(text) //WRITE MESSAGE HISTORY TO FILE 
	{
		this.history_file.open('a',true); 	
		this.history_file.writeln(user.alias + ": " + text);
		this.history_file.close();
	}
	this.ProcessKey=function(key) //PROCESS ALL INCOMING KEYSTROKES
	{
		this.Cycle();
		switch(key.toUpperCase())
		{
		//borrowed Deuce's feseditor.js
		case '\x00':	/* CTRL-@ (NULL) */
		case '\x03':	/* CTRL-C (Center Line) */
		case '\x04':	/* CTRL-D (Quick Find in SyncEdit)*/
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
		case '\x02':	/* CTRL-B KEY_HOME */
		case '\x05':	/* CTRL-E KEY_END */
		case KEY_UP:
		case KEY_DOWN:
			this.ShowHistory(key);
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
	this.DrawLines=function()
	{
		if(!this.lined) return;
		DrawLine(this.input_line.x-1,this.input_line.y-1,this.columns+2);
		if(this.y>1) DrawLine(this.x-1,this.y-1,this.columns+2);
		if(this.input_line.y<24) DrawLine(this.input_line.x-1,this.input_line.y+1,this.columns+2);
	}
	this.DrawBox=function()
	{
		if(!this.boxed) return;
		console.gotoxy(this.x-1,this.y-1);
		console.putmsg("\1n\1h\xDA");
		DrawLine(false,false,this.columns-6,"\1n\1h");
		console.putmsg("\1n\1h\xB4\1nCHAT\1h\xC3\xBF");
		for(line = 0; line<this.rows; line++)
		{
			console.gotoxy(this.x-1,this.y+line);
			printf("\1n\1h\xB3%*s\xB3",this.columns,"");
		}
		console.gotoxy(this.input_line.x-1,this.input_line.y-1);
		console.putmsg("\1n\1h\xC3");
		DrawLine(false,false,this.columns-7,"\1n\1h");
		console.putmsg("\1n\1h\xB4\1nINPUT\1h\xC3\xB4");
		console.gotoxy(this.input_line.x-1,this.input_line.y);
		printf("\1n\1h\xB3%*s\xB3",this.columns,"");
		console.gotoxy(this.input_line.x-1,this.input_line.y+1);
		console.putmsg("\1n\1h\xC0");
		DrawLine(false,false,this.columns,"\1n\1h");
		var spot=console.getxy();
		if(spot.y==24 && spot.x==80);
		else console.putmsg("\1n\1h\xD9");
	}
	this.LoadHistory=function() //LOAD CHAT ROOM HISTORY FROM FILE
	{
		if(!file_exists(this.history_file.name)) 
		{
			return false;
		}
		this.history_file.open('r',true); 
		var messages=this.history_file.readAll();
		for(msg in messages)
		{
			if(messages[msg].indexOf(":"))
			{
				var array=messages[msg].split(":");
				var name=array[0];
				color=(name==user.alias?this.local_color:this.remote_color);
				name=color + name + "\1h:";
				var text=color + array[1];
				this.messages.push(name + text);
			}
			else 
			{
				color=this.notice_color;
				this.messages.push(this.notice_color + messages[msg]);
			}
			
		}
		this.history_file.close();
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
	this.Buffer=function(key) //ADD A KEY TO THE USER INPUT BUFFER
	{
		if(!this.fullscreen)
		{
			var offset=key?1:0;
			if(this.buffer.length+offset>this.columns)
			{
				var overrun=(this.buffer.length+offset-this.columns);
				var truncated=this.buffer.substr(overrun);
				console.gotoxy(this.input_line.x,this.input_line.y);
				console.putmsg(truncated,P_SAVEATR);
			}
			else if(!key)
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
	this.GetNotices=function() //RECEIVE ALL GENERAL NOTICES FROM QUEUE AND DISPLAY THEM
	{
		for(notice in queue.notices)
		{
			this.Display(queue.notices[notice],this.notice_color);
		}
		queue.notices=[];
	}
	this.Log=function(text)
	{
		if(this.chatlog) this.chatlog.Log(text);
	}
	this.Display=function(text,color,user_)
	{
		var col=color?color:"\1n\1r";
		//FOR FULLSCREEN MODE WITH NO INPUT LINE, DISPLAY MESSAGE IN FULL AND RETURN
		if(this.fullscreen)	
		{
			if(!text) return;
			if(user_) console.putmsg(col + user_ + ": " + text + "\r\n",P_SAVEATR);
			else console.putmsg(col + text + "\r\n",P_SAVEATR);
			if(this.buffer.length) console.putmsg(this.local_color+this.buffer,P_SAVEATR);
		}
		else
		{
			//FOR ALL OTHER MODES, STORE MESSAGES IN AN ARRAY AND HANDLE WRAPPING 
			var output=[];
			if(text)
			{
				if(user_) 
				{
					this.messages.push(col + user_+ "\1h: \1n" + col + text);
				}
				else this.messages.push(col + text);
			}
			for(msg in this.messages)
			{
				var array=this.Concat(this.messages[msg]);
				for(item in array)
				{
					output.push(array[item]);
				}
			}
			if(output.length>this.rows && this.scrollbar) 
			{
				this.scrollbar.draw(1,1); //SHOW SCROLLBAR AT BOTTOM LIMIT
				this.history_index=0;
				while(output.length>this.rows) output.shift();
			}
			console.gotoxy(this.x,this.y);
			for(line in output)
			{
				//TODO: FIX SINGLE LINE MODE... NOT FUNCTIONAL
				if(this.rows==1) console.gotoxy(this.x,this.y);
				
				else console.gotoxy(this.x,this.y+parseInt(line));
				console.putmsg(output[line],P_SAVEATR);
				var length=console.strlen(strip_ctrl(output[line]));
				if(length<this.columns) ClearLine(this.columns-length);
			}
		}
	}
	this.Concat=function(text) //WRAP AND CONCATENATE NEW MESSAGE DATA
	{
		var newarray=[];
		if(console.strlen(text)<=this.columns) newarray.push(text);
		else
		{
			var array=word_wrap(text,this.columns,text.length,false);
			array=array.split(/[\r\n$]+/);
			for(item in array)
				if(array[item]!="") newarray.push(RemoveSpaces(array[item]));
		}
		return newarray;
	}
	this.Receive=function()
	{
		var data=queue.ReceiveData(this.name);
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
}
function UserList(x,y,c,r)
{
	this.x=x;
	this.y=y;
	this.columns=c;
	this.rows=r;
	this.userfile=queue.user_file;
	this.userlist;
	this.hidden=false;
	
	this.Hide=function()
	{
		this.hidden=true;
	}
	this.Unhide=function()
	{
		this.hidden=false;
		this.Redraw();
	}
	this.Init=function()
	{
		if(this.hidden) return;
		this.DrawBox();
		this.UpdateList();
		this.DrawList();
	}
	this.Resize=function(columns,rows,x,y)
	{
		if(columns) this.columns+=columns;
		if(rows) this.rows+=rows;
		if(x) this.x=x;
		if(y) this.y=y;
		this.Redraw();
	}
	this.Redraw=function()
	{
		if(this.hidden) return;
		this.DrawBox();
		this.DrawList();
	}
	this.DrawList=function()
	{
		var index=0;
		for(u in this.userlist)
		{
			var name=this.userlist[u].name;
			var room=this.userlist[u].room;
			console.gotoxy(this.x+1,this.y+(index*2)+2);
			console.putmsg("\1g\1h" + PrintPadded(name,this.columns));
			console.gotoxy(this.x+1,this.y+(index*2)+3);
			console.putmsg("\1k\1h\xC0\xC4\1n\1g" + room + "");
			index++;
		}
	}
	this.UpdateList=function()
	{
		if(!queue.UpdateUsers()) return;
		this.userlist=[];
		this.userfile.open('r',true);
		var rooms=this.userfile.iniGetSections();
		for(r in rooms)
		{
			var room=rooms[r];
			var users=this.userfile.iniGetKeys(room);
			for(u in users)
			{
				var name=users[u].substr(users[u].indexOf(".")+1);
				var status=this.userfile.iniGetValue(room,name);
				this.userlist.push({"name":name,"room":room,"status":status});
			}
		}
		this.userfile.close();
		this.DrawList();
	}
	this.DrawBox=function()
	{
		console.gotoxy(this.x,this.y);
		console.putmsg("\1n\1h\xDA\xB4\1nUSERS\1h\xC3");
		DrawLine(false,false,this.columns-7,"\1n\1h");
		console.putmsg("\1n\1h\xBF");
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



