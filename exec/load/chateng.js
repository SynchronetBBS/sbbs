/*
	JAVASCRIPT MULTI-USER CHAT ENGINE -- BY MCMLXXIX (Matt Johnson) 08/2008
	***UPDATED: 11/2009
	-----------------------------------------------
	FULLSCREEN DEFAULT OR WINDOWED MODE WITH SUPPLIED PARAMETERS

	NOTE: 
	For a simple full screen chat, create a script like the following:
	
	load("chateng.js");
	Chat();
	
	This will call the following Chat() function 
	to handle the engine in its simplest form
	
	For more advanced chatting, see 
	ChatEngine this.Init() method
*/
function Chat(key,engine)
{
	if(!engine) 
	{
		engine=new ChatEngine();
		engine.Init();
	}
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
			if(engine.stream) 
			{
				var packet=engine.stream.receive();
				if(packet<0) exit();
				else if(!packet==0 && (packet.room==engine.room || packet.scope==global_scope)) 
				{
					engine.ProcessData(packet.data);
				}
			}
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
load("commclient.js");
load("scrollbar.js");
load("nodedefs.js");

function ChatEngine(root,stream)
{
	const local_color=		"\1n\1g";
	const remote_color=	"\1n\1c";
	const alert_color=		"\1n\1r\1h";
	const notice_color=	"\1n\1k\1h";
	const input_color=		"\1n\1y";
	const private_color=	"\1n\1y";
	const global_color=	"\1n\1m";
	
	var root_dir=			(root?root:system.exec_dir);
	var scope=				normal_scope;
	var messages=			[];

	this.stream=			stream?stream:new GameConnection("chat");
	this.buffer=			"";
	this.room;	
	this.fullscreen;
	this.columns;
	this.rows;
	this.x;
	this.y;
	this.boxed;
	this.lined;
	this.input_line;
	this.scrollbar;
	
	
	
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
	
		this.buffer="";
		messages=[];
		
 		if(input_line)
		{
			this.input_line={'x':this.x,'y':this.y+this.rows+1};
			this.fullscreen=false;
			this.scrollbar=new Scrollbar(this.x+this.columns,this.y,this.rows,"vertical",scrollbar_color?scrollbar_color:"\1k"); 
		}
		else this.fullscreen=true;

		this.DrawLines();
		this.DrawBox();
		this.GetNotices();
		this.EntryMessage();
		
		if(!this.stream) 
		{
			log("unable to connect to service...");
			return false;
		}
		return true;
	}
	this.EntryMessage=function()
	{
		var message=user.alias + " has entered the room";
		var data=
		{
			"scope":scope,
			"system":system.qwk_id,
			"message":message
		};
		var packet=
		{
			"scope":scope,
			"room":this.room,
			"type":"chat",
			"data":data
		};
		this.stream.send(packet);
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
		this.Redraw();
		this.scrollbar=new Scrollbar(this.x+this.columns,this.y,this.rows,"vertical",this.scrollbar.color); 
	}
	this.FindUser=function(id)
	{
		//return this.stream.FindUser(id);
	}
	this.ClearChatWindow=function() //CLEARS THE ENTIRE CHAT WINDOW
	{
		ClearBlock(this.x,this.y,this.columns,this.rows);
	}
	this.DisplayInfo=function(array) //DISPLAYS A TEMPORARY MESSAGE IN THE CHAT WINDOW (NOT STORED)
	{
		this.ClearChatWindow();
		var newarray=[];
		for(var l=0;l<array.length;l++)
		{
			var newlines=this.Concat(array[l]);
			for(var n=0;n<newlines.length;n++)
			{
				newarray.push(newlines[n]);
			}
		}
		for(item=0;item<newarray.length;item++)
		{
			console.gotoxy(this.x,this.y+item);
			console.putmsg(newarray[item],P_SAVEATR);
		}
	}
	this.Notice=function(msg)
	{
		this.Display(notice_color + msg);
	}
	this.GetNotices=function()
	{
		var msgs=stream.getnotices();
		if(!msgs) return false;
		stream.notices=[];
		for(var m=0;m<msgs.length;m++)
		{
			this.Notice(msgs[m]);
		}
	}
	this.Alert=function(msg) //DISPLAYS A MESSAGE ON THE INPUT LINE (OR CURRENT LINE IN FULLSCREEN MODE)
	{
		if(this.input_line)
		{
			this.ClearInputLine();
			console.gotoxy(this.input_line.x,this.input_line.y);
		}
		console.putmsg(alert_color + msg);
	}
	this.ClearInputLine=function()
	{
		write(console.ansi(ANSI_NORMAL));
		if(this.input_line)	ClearLine(this.columns,this.input_line.x,this.input_line.y);
	}
	this.Redraw=function()
	{
		this.ClearChatWindow();
		this.DrawLines();
		this.DrawBox();
		this.Display();
		this.ClearInputLine();
		this.Buffer();
	}
	this.Send=function(message)
	{
		var packet=this.PackageData(message);
		if(!this.stream.send(packet))
		{
			this.Notice("message could not be sent..");
		}
		scope=normal_scope;
		//this.StoreHistory(data.message);
	}
	
	//INTERNAL METHODS
	this.ProcessKey=function(key) //PROCESS ALL INCOMING KEYSTROKES
	{
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
		case '\x02':	/* CTRL-B KEY_HOME */
		case '\x05':	/* CTRL-E KEY_END */
		case KEY_UP:
		case KEY_DOWN:
			break;
		case '\b':
			this.BackSpace();
			break;
		case '\r':
		case '\n':
			if(console.strlen(RemoveSpaces(this.buffer))) 
			{
				this.Send(this.buffer);
			}
			this.ResetInput();
			break;
		case '!':
			if(console.strlen(RemoveSpaces(this.buffer)) || scope==global_scope) 
			{
				this.Buffer(key);
			}
			else
			{
				scope=global_scope;
			}
			break;
		case '@':
			if(!user.compare_ars("SYSOP") && !(bbs.sys_status&SS_TMPSYSOP)) 
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
		DrawLine(false,false,this.columns-(8+this.room.length),"\1n\1h");
		console.putmsg("\1n\1h\xB4\1nCHAT\1h: \1n" + this.room + "\1h\xC3\xBF");
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
		if(!(spot.y==console.screen_rows && spot.x==console.screen_columns)) console.putmsg("\1n\1h\xD9");
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
				var disp=truncated;
				if(disp.indexOf('@')>=0) disp=disp.replace(/@/g,"?");
				console.gotoxy(this.input_line.x,this.input_line.y);
				console.putmsg(input_color + disp,P_SAVEATR);
			}
			else if(!key)
			{
				console.gotoxy(this.input_line.x,this.input_line.y);
				var disp=this.buffer;
				if(disp.indexOf('@')>=0) disp=disp.replace(/@/g,"?");
				console.putmsg(input_color + disp,P_SAVEATR);
			}
			else 
			{
				offset=this.buffer.length;
				console.gotoxy(this.input_line.x+offset,this.input_line.y);
			}
		}
		if(key) 
		{
			this.buffer+=key;
			if(key=="@") key="?";
			console.putmsg(input_color + key,P_SAVEATR);
		}
		if(!this.fullscreen && this.buffer.length<this.columns)	ClearLine(this.columns-this.buffer.length);
	}
	this.Display=function(text,color,username)
	{
		var col=color?color:"";
		//FOR FULLSCREEN MODE WITH NO INPUT LINE, DISPLAY MESSAGE IN FULL AND RETURN
		if(this.fullscreen)	
		{
			if(!text) return;
			if(username) console.putmsg("\r" + col + username + ": " + text + "\r\n",P_SAVEATR);
			else console.putmsg("\r" + col + text + "\r\n",P_SAVEATR);
			if(this.buffer.length) console.putmsg(local_color+this.buffer,P_SAVEATR);
		}
		else
		{
			//FOR ALL OTHER MODES, STORE MESSAGES IN AN ARRAY AND HANDLE WRAPPING 
			var output=[];
			if(text)
			{
				if(username) 
				{
					messages.push(col + username+ "\1h: " + col + text);
				}
				else messages.push(col + text);
			}
			for(msg in messages)
			{
				var array=this.Concat(messages[msg]);
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
				
				else console.gotoxy(this.x,this.y+parseInt(line,10));
				var display=output[line];
				if(user.compare_ars("SYSOP"))
				{
					if(display.indexOf('@')>=0) display=display.replace(/@/g,"?");
				}
				console.putmsg(display,P_SAVEATR);
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
			{
				if(array[item]!=="") newarray.push(RemoveSpaces(array[item]));
			}
		}
		return newarray;
	}
	this.ProcessData=function(data)
	{
		var intensity="";
		if(data.name==user.alias) intensity="\1h";
		switch(data.scope)
		{
			case normal_scope:
				if(data.name) this.Display(data.message,remote_color + intensity,data.name);
				else this.Display(data.message,alert_color);
				break;
			case priv_scope:
				this.Display(data.message,private_color + intensity,data.name);
				break;
			case global_scope:
				this.Display(data.message,global_color + intensity,data.name);
				break;
			default:
				log("message scope unknown");
				break;
		}
	}
	this.PackageData=function(message)
	{
		var data=
		{
			"scope":scope,
			"system":system.qwk_id,
			"name":user.alias,
			"message":message
		};
		var packet=
		{
			"scope":scope,
			"room":this.room,
			"type":"chat",
			"data":data
		};
		this.ProcessData(data);
		return packet;
	}
	this.ResetInput=function()
	{
		if(!this.fullscreen) this.ClearInputLine();
		else 
		{
			console.left(this.buffer.length);
			console.cleartoeol();
		}
		this.buffer="";
		scope=normal_scope;
	}
	
}



