function 	Menu(title,x,y,color,hkey_color)		
{								//MENU CLASSES
	this.title=title;
	//this.disabled=[];
	//this.disabled_color="\1k\1h";
	this.items=[];
	var orig_x=x;
	var orig_y=y;

	this.showline=function()
	{
		var yyyy=orig_y;
		console.gotoxy(xxxx,yyyy); yyyy++;
		console.cleartoeol();
		drawLine("\1w\1h",79); console.crlf();
	}
	this.display=function()
	{
		var yyyy=orig_y;
	
		var clear=5;
		var cleared=0;
		for(var i in this.items) {
			if(this.items[i].enabled)
			{
				console.gotoxy(orig_x,yyyy); yyyy++;
				console.putmsg(this.items[i].text);
				console.cleartoeol();
				cleared++;
			}
		}
		for(var i=cleared;i<clear;i++)	{
			console.gotoxy(orig_x,yyyy); yyyy++;
			console.cleartoeol();

		}
	}
	this.disable=function(items)
	{
		for(var item in items) {
			this.items[items[item]].enabled=false;
		}
	}
	this.enable=function(items)
	{
		for(var item in items)	{
			this.items[items[item]].enabled=true;
		}
	}
	this.add=function(items)
	{
		for(var i=0;i<items.length;i++)	{
			var hotkey=this.getHotKey(items[i]);
			this.items[hotkey]=new MenuItem(items[i],hotkey,color,hkey_color);
			this.items[hotkey].init(color,hkey_color);
		}
	}
	this.displayTitle=function()
	{
		printf("\1h\1w" + this.title);
	}
	this.getHotKey=function(item)
	{
		var keyindex=item.indexOf("~")+1;
		return item.charAt(keyindex);
	}	
	this.displayHorizontal=function()
	{
		clearLine(1,48);
		console.gotoxy(orig_x,orig_y);
		for(var i in this.items) {
			if(this.items[i].enabled) console.putmsg(this.items[i].text + " ");
		}
	}
}
function 	MenuItem(item,hotkey,color,hkey_color)
{								//MENU ITEM OBJECT
	this.displayColor=color;
	this.keyColor=hkey_color;
	this.item=item;
	this.enabled=true;
	
	this.hotkey=hotkey;
	this.init=function(color,hkey_color)
	{
		this.text=this.item.replace(("~" + hotkey) , (color + "[" + hkey_color + hotkey + color + "]"));
	}
}