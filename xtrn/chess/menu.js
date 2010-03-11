function 	Menu(color,hkey_color)		
{								
	this.items=[];
	this.color=color;
	this.hkey_color=hkey_color;

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
}
function 	MenuItem(item,color,hotkey,hkey_color)
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