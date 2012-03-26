function Menu(items,frame,hk_color,text_color) {								
	this.items=[];
	this.frame=frame;
	this.hk_color=hk_color;
	this.text_color=text_color;
	
	this.disable=function(item)
	{
		this.items[item].enabled=false;
		this.draw();
	}
	this.enable=function(item)
	{
		this.items[item].enabled=true;
		this.draw();
	}
	this.add=function(items)
	{
		for(i=0;i<items.length;i++) {
			hotkey=get_hotkey(items[i]);
			this.items[hotkey.toUpperCase()]=new Item(items[i],hotkey,hk_color,text_color);
		}
	}
	this.draw=function()
	{
		var str="";
		for each(var i in this.items)
			if(i.enabled==true) str+=i.text + " ";
		this.frame.clear();
		this.frame.putmsg(str);
	}
	
	function Item(item,hotkey,hk_color,text_color)
	{								
		this.enabled=true;
		this.hotkey=hotkey;
		this.text=item.replace(("~" + hotkey) , hk_color + hotkey + text_color);
	}
	function get_hotkey(item)
	{
		keyindex=item.indexOf("~")+1;
		return item.charAt(keyindex);
	}	
	this.add(items);
}
