function Menu(items,bg,x,y)		
{								//MENU CLASSES
	this.items=[];
	this.x=x;
	this.y=y;
	this.bg=bg;
	
	this.disable=function(item)
	{
		this.items[item].enabled=false;
	}
	this.enable=function(item)
	{
		this.items[item].enabled=true;
	}
	this.add=function(items)
	{
		for(i=0;i<items.length;i++)
		{
			hotkey=get_hotkey(items[i]);
			this.items[hotkey]=new Item(items[i],hotkey);
		}
	}
	this.clear=function()
	{
		console.gotoxy(this);
		console.cleartoeol(this.bg);
	}
	this.draw=function()
	{
		console.gotoxy(this);
		console.pushxy();
		console.cleartoeol(this.bg);
		console.popxy();
		for(i in this.items)
		{
			if(this.items[i].enabled==true) console.putmsg(this.items[i].text + " ",P_SAVEATR);
		}
		console.attributes=ANSI_NORMAL;
	}
	this.add(items);
}
function Item(item,hotkey)
{								//MENU ITEM OBJECT
	this.enabled=true;
	this.hotkey=hotkey;
	this.text=item.replace(("~" + hotkey) , hotkey);
}
function get_hotkey(item)
{
	keyindex=item.indexOf("~")+1;
	return item.charAt(keyindex);
}	
