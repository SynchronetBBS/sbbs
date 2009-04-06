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
		DrawLine("\1w\1h",79); console.crlf();
	}
	this.display=function()
	{
		var yyyy=orig_y;
	
		var clear=5;
		var cleared=0;
		for(i in this.items)
		{
			if(this.items[i].enabled)
			{
				console.gotoxy(orig_x,yyyy); yyyy++;
				console.putmsg(this.items[i].text);
				console.cleartoeol();
				cleared++;
			}
		}
		for(i=cleared;i<clear;i++)
		{
			console.gotoxy(orig_x,yyyy); yyyy++;
			console.cleartoeol();

		}
	}
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
	this.add=function(items)
	{
		for(i=0;i<items.length;i++)
		{
			hotkey=this.getHotKey(items[i]);
			this.items[hotkey.toUpperCase()]=new MenuItem(items[i],hotkey,color,hkey_color);
			this.items[hotkey.toUpperCase()].Init(color,hkey_color);
		}
	}
	this.displayTitle=function()
	{
		printf("\1h\1w" + this.title);
	}
	this.getHotKey=function(item)
	{
		keyindex=item.indexOf("~")+1;
		return item.charAt(keyindex);
	}	
	this.displayHorizontal=function()
	{
		ClearLine(1,48);
		console.gotoxy(orig_x,orig_y);
		for(i in this.items)
		{
			if(this.items[i].enabled) console.putmsg(this.items[i].text);
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
	this.Init=function(color,hkey_color)
	{
		this.text=this.item.replace(("~" + hotkey) , (hkey_color + hotkey + color));
	}
}