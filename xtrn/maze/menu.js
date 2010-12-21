function Menu(items,x,y,w,hl,txt)		
{								//MENU CLASSES
	this.items=[];
	this.x=x;
	this.y=y;
	this.width=w;
	this.hl=hl;
	this.txt=txt;
	
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
		for(i=0;i<items.length;i++) {
			hotkey=get_hotkey(items[i]);
			this.items[hotkey]=new Item(items[i],hotkey,hl,txt);
		}
	}
	this.clear=function()
	{
		console.gotoxy(this);
		console.write(format("%*s",this.width,""));
	}
	this.draw=function()
	{
		var str="";
		for each(var i in this.items) {
			if(i.enabled==true) str+=i.text + " ";
		}
		console.gotoxy(this);
		console.attributes=ANSI_NORMAL;
		console.putmsg(format("%-s",str,this.width));
	}
	this.add(items);
}
function Item(item,hotkey,hl,txt)
{								//MENU ITEM OBJECT
	this.enabled=true;
	this.hotkey=hotkey;
	this.text=item.replace(("~" + hotkey) , hl + hotkey + txt);
}
function get_hotkey(item)
{
	keyindex=item.indexOf("~")+1;
	return item.charAt(keyindex);
}	
