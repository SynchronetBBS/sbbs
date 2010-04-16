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
	ChatEngine this.init() method
*/
function Chat(key,engine)
{
	if(!engine) 
	{
		engine=new ChatEngine();
		engine.init();
	}
	if(key)
	{
		engine.processKey(key);
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
			engine.receive();
			key=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(key) {
				engine.processKey(key);
				switch(key.toUpperCase())
				{
					case '\x1b':	
						engine.exit();
						return false;
					default:
						key="";
						break;
				}
			}
		}
	}
}
//*************MAIN ENGINE*************
load("commclient.js");
load("scrollbar.js");
load("graphic.js");
load("str_cmds.js");
load("nodedefs.js");
bbs.sys_status |= SS_PAUSEOFF;	
oldpass=console.ctrl_key_passthru;

function ChatEngine(root)
{
	const flag_normal=			"#";
	const flag_global=			"!";
	const flag_private=		"%";
	const flag_alert=			2;
	const flag_notice=			1;
	const flag_message=		0;
	
	const local_color=			"\1n\1g";
	const remote_color=		"\1n\1c";
	const alert_color=			"\1r\1h";
	const notice_color=		"\1n\1y";
	const input_color=			"\1n";
	const private_color=		"\1y\1h";
	const global_color=		"\1n\1m";
	
	//TODO: the only time this will be used is for storing chat history
	//maybe give ALL chat history files their own home, independent of the parent script
	var root_dir=(root?root:js.exec_dir);
	var stream=new ServiceConnection("chat");
	var scope=flag_normal;
	var window=false;
	var message=new Object();
	var fullscreen=true;
	var scrollbar=false;
	var background="\0010";
	var ignore=[];
	
	this.buffer="";
	this.room="main";	
	this.columns=console.screen_columns;
	this.rows=console.screen_rows;
	this.x=1;
	this.y=1;
	this.input_line;
	
	// USEFUL METHODS 
	this.init=function(room,c,r,x,y,bg,ix,iy,iw) //NOTE: DESTROYS BUFFER AND MESSAGE LIST
	{
		if(x) this.x=x;				//top left corner x
		if(y) this.y=y;				//top left corner y
		if(room) this.room=room;		//room name (for lobby style interface)
		if(bg) background=bg;
		this.buffer="";

		if(c && r) {
			fullscreen=false;
			this.columns=c;
			this.rows=r;
			if(this.columns>=console.screen_columns) this.columns=console.screen_columns-1;
			scrollbar=new Scrollbar(this.x+this.columns,this.y,this.rows,"vertical","\1k\1h"); 
		}
 		if(ix && iy) {
			this.input_line=new InputLine(ix,iy,iw?iw:this.columns,bg);
			fullscreen=false;
		} else if(ix || iy) {
			log("invalid input line parameters",LOG_WARNING);
			return false;
		}
		if(!fullscreen) {
			console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
			bbs.sys_status|=SS_MOFF;
			window=new Graphic(this.columns,this.rows,undefined,undefined,true);
		}
		this.entryMessage();
		return true;
	}
	this.exit=function()
	{
		this.exitMessage();
		stream.close();
		console.ctrlkey_passthru=oldpass;
		bbs.sys_status&=~SS_MOFF;
		bbs.sys_status&=~SS_PAUSEOFF;
		console.attributes=ANSI_NORMAL;
	}
	this.exitMessage=function()
	{
		if(user.compare_ars("QUIET")) return false;
		var message=user.alias + " disconnected";
		this.send(message,flag_notice);
	}
	this.entryMessage=function()
	{
		if(user.compare_ars("QUIET")) return false;
		var message=user.alias + " connected";
		this.send(message,flag_notice);
	}
	this.resize=function(x,y,c,r,ix,iy,iw) //NOTE: DOES NOT DESTROY BUFFER OR MESSAGE LIST
	{
		this.clearChatWindow();
		if(x) this.x=x;
		if(y) this.y=y;
		if(c) this.columns=columns;
		if(r) this.rows=rows;
		if(ix && iy) {
			if(!this.input_line) {
				this.input_line=new InputLine(ix,iy,iw?iw:this.columns,background);
			} else {
				this.input_line.x=ix;
				this.input_line.y=iy;
				this.input_line.width=iw?iw:this.columns;
			}
		} else {
			if(this.input_line)	{
				this.input_line.x=this.x;
				this.input_line.y=this.y+this.rows+1;
				this.input_line.width=this.columns;
			}
		}
		if(this.input_line) {
			scrollbar=new Scrollbar(this.x+this.columns-1,this.y,this.rows,"vertical",DARKGRAY); 
		}
		this.redraw();
	}
	this.ignoreUser=function(alias)
	{
		if(ignore[alias]==true) ignore[alias]=false;
		else ignore[alias]==true;
	}
	this.findUser=function(id)
	{
		//return stream.findUser(id);
	}
	this.clearChatWindow=function() //CLEARS THE ENTIRE CHAT WINDOW
	{
		clearBlock(this.x,this.y,this.columns,this.rows);
	}
	this.list=function(array,color) //DISPLAYS A TEMPORARY MESSAGE IN THE CHAT WINDOW (NOT STORED)
	{
		for(var i=0;i<array.length;i++) this.display(array[i],color);
	}
	this.notice=function(msg)
	{
		this.display(msg,notice_color);
	}
	this.alert=function(msg)
	{
		this.display(msg,alert_color);
	}
	this.redraw=function()
	{
		this.clearChatWindow();
		this.display();
		this.printBuffer();
	}
	this.send=function(txt,level,scope,source,target)
	{
		if(!level) level=flag_message;
		if(!scope) scope=flag_normal;
		var packet=new Message(txt,level,scope,source,target);
		if(!stream.send(packet)) {
			this.alert("message not sent.");
		}
	}
	this.receive=function()
	{
		var packet=stream.receive();
		if(packet && packet.message) {
			this.processData(packet);
		}
	}
	
	//INTERNAL METHODS
	this.processKey=function(key) //PROCESS ALL INCOMING KEYSTROKES
	{
		switch(key.toUpperCase())
		{
		//borrowed Deuce's feseditor.js
		case '\x00':	/* CTRL-@ (NULL) */
		case '\x03':	/* CTRL-C (Center Line) */
		case '\x04':	/* CTRL-D (Quick find in SyncEdit)*/
		case '\x0b':	/* CTRL-K */
		case '\x0c':	/* CTRL-L (Insert Line) */
		case '\x0e':	/* CTRL-N */
		case '\x0f':	/* CTRL-O (Quick Save/exit in SyncEdit) */
		case '\x10':	/* CTRL-P */
		case '\x11':	/* CTRL-Q (XOff) (Quick Abort in SyncEdit) */
		case '\x12':	/* CTRL-R (Quick redraw in SyncEdit) */
		case '\x13':	/* CTRL-S (Xon)  */
		case '\x14':	/* CTRL-T (Justify Line in SyncEdit) */
		case '\x15':	/* CTRL-U (Quick Quote in SyncEdit) */
		case '\x16':	/* CTRL-V (toggle insert mode) */
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
		case '\x02':	/* CTRL-B KEY_HOME */
		case '\x05':	/* CTRL-E KEY_END */
			this.showHistory(key);
			break;
		case '\b':
			this.backSpace();
			break;
		case '\r':
		case '\n':
			this.submitMessage();
			break;
		case '\x09':	/* CTRL-I TAB */
			if(message.scope==flag_global) message.scope=flag_normal;
			else message.scope=flag_global;
			this.printBuffer();
			break;
		case ';':
			this.bufferKey(key);
			if(this.buffer.length==1 && fullscreen && user.compare_ars("SYSOP")) {
				this.getStringCommand()
				this.resetInput();
				break;
			}
			break;
		case '@':
			if(!user.compare_ars("SYSOP") && !(bbs.sys_status&SS_TMPSYSOP)) break;
		default:
			this.bufferKey(key);
			break;
		}
		return true;
	}
	this.getStringCommand=function()
	{
		var cmd=console.getstr();
		if(cmd.length>0) {
			str_cmds(cmd);
			return true;
		} else return false;
	}
	this.submitMessage=function()
	{
		if(strlen(this.buffer)>0) {
			this.send(this.buffer,message.level,message.scope,message.source,message.target);
			switch(message.scope) 
			{
			case flag_global:
				this.display(this.buffer,global_color,user.alias);	
				break;
			case flag_private:
				this.display(this.buffer,private_color,user.alias,message.target);
				break;
			case flag_normal:
			default:
				this.display(this.buffer,local_color,user.alias);
				break;
			}
		}
		this.resetInput();
	}
	this.backSpace=function()
	{
		if(this.buffer.length>0) {
			if(fullscreen) {
				console.left();
				console.cleartoeol();
				this.buffer=this.buffer.substr(0,this.buffer.length-1);
			} else if(this.buffer.length<=this.input_line.width) {
				this.getInputPosition();
				console.left();
				console.putmsg(" ",P_SAVEATR);
				this.buffer=this.buffer.substr(0,this.buffer.length-1);
			} else {
				this.buffer=this.buffer.substr(0,this.buffer.length-1);
				this.printBuffer();
			}
			return true;
		} else {
			return false;
		}
	}
	this.getInputPosition=function()
	{
		console.gotoxy(this.input_line.x+this.buffer.length,this.input_line.y);
	}
	this.printBuffer=function()
	{
		if(!this.buffer.length) return false;
		var color="";
		switch(message.scope) {
		case flag_global:
			color=background + global_color;
			break;
		case flag_private:
			color=background + private_color;
			break;
		case flag_normal:
		default:	
			color=background + input_color;
			break;
		}
		if(fullscreen) {
			console.putmsg(color,P_SAVEATR);
			console.write("\r" + this.buffer);
			console.cleartoeol();
		} else {
			console.gotoxy(this.input_line);
			console.putmsg(color,P_SAVEATR);
			if(this.buffer.length>this.input_line.width) {
				var overrun=(this.buffer.length-this.columns);
				var truncated=this.buffer.substr(overrun);
				var disp=truncated;
				if(disp.indexOf('@')>=0) disp=disp.replace(/@/g,"?");
				console.write(disp);
			} else {
				console.write(this.buffer);
			}
		}
	}
	this.bufferKey=function(key) //ADD A KEY TO THE USER INPUT BUFFER
	{
		if(!key) return false;
		var color="";
		switch(message.scope) {
		case flag_global:
			color=background + global_color;
			break;
		case flag_private:
			color=background + private_color;
			break;
		case flag_normal:
		default:	
			color=background + input_color;
			break;
		}
		if(!fullscreen) {
			if(this.buffer.length>=this.input_line.width) {
				this.buffer+=key;
				this.printBuffer();
				return;
			} else {
				this.getInputPosition();
			}
		} 
		this.buffer+=key;
		console.putmsg(color,P_SAVEATR);
		console.write(key);
	}
	this.showHistory=function(key) //ACTIVATE MESSAGE HISTORY SCROLLBACK
	{
		if(!fullscreen && window.length>window.height) {
			switch(key)
			{
				case '\x02':	/* CTRL-B KEY_HOME */
					window.home();
					break;
				case '\x05':	/* CTRL-E KEY_END */
					window.end();
					break;
				case KEY_DOWN:
					window.scroll(1);
					break;
				case KEY_UP:
					window.scroll(-1);
					break;
			}
			window.draw(this.x,this.y);
			scrollbar.draw(window.index,window.length,window.height);
		}
	}
	this.display=function(text,color,source,target)
	{
		var msg;
		if(source) {
			msg="\r" + color + source + "\1h: " + color + text + "\r\n";
		} else {
			msg="\r" + color + text + "\r\n";
		}
		if(fullscreen) {
			console.putmsg(msg,P_SAVEATR); 
			this.printBuffer();
		} else {
			window.putmsg(false,false,msg,undefined,true); 
			window.draw(this.x,this.y);
			if(window.length>window.height) scrollbar.draw(window.index,window.length,window.height);

		}
	}
	this.processData=function(data)
	{
		if(ignore[data.source]==true) return false;
		switch(data.scope)
		{
		case flag_private:
			if(data.target==user.alias) this.display(data.message,private_color + "\1h",data.source);
			break;
		case flag_global:
			this.display(data.message,global_color + "\1h",data.source);
			break;
		case flag_normal:
		default:
			if(!data.source) {
				switch(data.level) 
				{
				case flag_alert:
					this.display(data.message,alert_color);
					break;
				case flag_notice:
				default:
					this.display(data.message,notice_color);
					break;
				}
			} else	if(data.source==user.alias) {
				this.display(data.message,local_color,data.source);
			} else this.display(data.message,remote_color,source);
			break;
		}
	}
	this.resetInput=function()
	{
		if(!fullscreen) this.input_line.clear();
		else {
			console.left(this.buffer.length);
			console.cleartoeol();
		}
		this.buffer="";
		message=new Message();
	}
	
}
function Message(txt,level,scope,source,target)
{
	this.txt=txt;
	this.level=level;
	this.scope=scope;
	this.source=source;
	this.target=target;
}
function InputLine(x,y,width,bg)
{
	this.x=x;
	this.y=y;
	this.width=width;
	this.bg=bg;
	this.position;
	this.window;
	this.buffer="";
	
	this.clear=function(bg) 
	{
		clearLine(this.width,this.x,this.y,bg?bg:this.bg);
		console.gotoxy(this);
	}
	this.init=function(x,y,w,bg,window) 
	{
		this.x=x;
		this.y=y;
		this.width=w;
		this.bg=bg;
		if(window) {
			this.window=new Window(x,y,w,3);
			this.window.init("\1nINPUT");
			this.x+=1;
			this.y+=1;
			this.width-=2;
		}
	}
	this.draw=function()
	{
		if(this.window) {
			this.window.draw();
		}
		console.gotoxy(this.x,this.y);
		console.write(printPadded(this.color + this.buffer,this.width));
	}
}

