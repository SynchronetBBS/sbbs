if(!js.global || js.global.Graphic==undefined) 
	load("graphic.js");

load("sbbsdefs.js");
load("funclib.js");
load("scrollbar.js");

function Window(xpos,ypos,w,h)
{
	var width=console.screen_columns;
	var height=console.screen_rows;
	var x=1;
	var y=1;
	var message_list=[];
	var window;
	var scrollbar;
	
	this.update=true;
	
	this.init=function(xpos,ypos,w,h)
	{
		if(xpos) x=xpos;
		if(ypos) y=ypos;
		if(w) width=w;
		if(h) height=h;
		
		if(width>=console.screen_columns) width=console.screen_columns-1;
		if(window && window.length > window.height) width-=1;
		window=new Graphic(width,height);
		window.atcodes=false;
		for(var m=0;m<message_list.length;m++) {
			window.putmsg(false,false,message_list[m],undefined,true);
		}
		//window.length=window.height-1;
	}
	this.handle_command=function(cmd)
	{
		switch(cmd) {
		case KEY_LEFT:
		case KEY_RIGHT:
			break;
		case KEY_DOWN:
		case KEY_UP:
		case KEY_HOME:
		case KEY_END:
			this.scroll(cmd);
			break;
		default:
			this.post(cmd,"\1n");
			break;
		}
		return true;
	}
	this.scroll=function(key) 
	{
		if(window.length>window.height) {
			var update=false;
			switch(key)
			{
			case '\x02':	/* CTRL-B KEY_HOME */
				if(window.home()) update=true;
				break;
			case '\x05':	/* CTRL-E KEY_END */
				if(window.end()) update=true;
				break;
			case KEY_DOWN:
				if(window.scroll(1)) update=true;
				break;
			case KEY_UP:
				if(window.scroll(-1)) update=true;
				break;
			}
			if(update) this.draw();
		}
	}
	this.post=function(text)
	{
		text+="\r\n";
		message_list.push(text);
		window.putmsg(false,false,text,undefined,true); 
		this.update=true;
	}
	this.list=function(array,color) //DISPLAYS A TEMPORARY MESSAGE IN THE CHAT WINDOW (NOT STORED)
	{
		for(var i=0;i<array.length;i++) this.post(array[i],color);
	}
	this.clear=function()
	{
		clearBlock(x,y,width,height);
		message_list=[];
	}
	this.draw=function()
	{
		this.update=false;
		if(window.length>window.height) {
			if(!scrollbar) {
				width-=1;
				scrollbar=new Scrollbar(x+width,y,height,"vertical","\1k\1h"); 
				window=new Graphic(width,height);
				window.atcodes=false;
				for(var m=0;m<message_list.length;m++) {
					window.putmsg(false,false,message_list[m],undefined,true);
				}
			}
			var index=window.index;
			var range=window.length-window.height;
			scrollbar.draw(index,range);
		}
		window.draw(x,y);
	}
	this.redraw=this.draw;
	
	this.init(xpos,ypos,w,h);
}
