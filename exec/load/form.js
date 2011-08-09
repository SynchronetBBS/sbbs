/* Id: $ */

/**
 *	Javascript Object Editor 
 *	for Synchronet v3.15a+
 *	by Matt Johnson
 *
 *	returns a copy of any passed object 
 *	with modified properties
 *
 *	USAGE:
 *
 *	var obj = {
 *		x = 1,
 *		y = 2
 *	};
 *	var newobj = formEdit(obj);
 */

if(js.global.SYS_CLOSED == undefined)
	load("sbbsdefs.js");
if(js.global.getColor == undefined)
	load("funclib.js");
if(js.global.Layout == undefined)
	load("layout.js");
	
function formEdit(obj)
{
	function Form(obj) 
	{
		this.len = 0;
		this.max = 25;
		this.index = 0;
		this.items = [];
		
		for(var k in obj) {
			if(console.strlen(k) > this.len)
				this.len = console.strlen(k);
			this.items.push({
				key:k,
				value:obj[k],
				type:typeof obj[k]
			});
		}
		
		this.box = new Layout_View(
			printPadded("KEY",this.len+2) + splitPadded("VALUE","ESC",this.max),
			(console.screen_columns / 2) - ((this.len + 25)/2) -1,
			(console.screen_rows / 2) - (this.items.length / 2) -2,
			this.len + this.max + 4,
			this.items.length + 3
		);
		
		this.box.show_tabs = false;
		
		this.edit = function() {
			var item = this.items[this.index];
			
			if(item.type == "object") {
				item.value = formEdit(item.value);
				this.draw();
			}
			else {
				var coords = this.coords(this.index);
				console.attributes = BG_LIGHTGRAY + BLACK;
				console.gotoxy(coords.x + this.len + 2, coords.y);
				clearLine(this.max);
				console.gotoxy(coords.x + this.len + 2, coords.y);
				
				var newvalue = console.getstr(item.value.toString(),this.max,K_EDIT|K_AUTODEL);
				switch(item.type) {
					case "number":
						item.value = Number(newvalue);
						break;
					case "function":
						item.value = eval(newvalue);
						break;
					case "boolean":
						item.value = Bool(newvalue);
						break;
					case "string":
						item.value = newvalue;
						break;
				}
				this.drawItem(this.index);
			}
		}
		
		this.draw = function() {
			this.box.drawView();
			
			for(var i = 0; i < this.items.length; i++) {
				this.drawItem(i,this.index == i);
			}
		}
		
		this.object getter = function() {
			var obj = {};
			for(var i = 0; i < this.items.length; i++) {
				var item = this.items[i];
				obj[item.key] = item.value;
			}
			return obj;
		}
		
		this.coords = function(i) {
			return {
				x:this.box.x + 1,
				y:this.box.y + 2 + i,
			}
		}
		
		this.drawItem = function(i,curr) {
			var coords = this.coords(i);
			console.gotoxy(coords);
			if(curr) 
				console.putmsg(getColor(layout_settings.lbg) + getColor(layout_settings.lfg) + 
					printPadded(this.items[i].key,this.len + 2) + "\1w" + 
					printPadded(this.items[i].value,this.max)
				);
			else
				console.putmsg(getColor(layout_settings.vbg) + getColor(layout_settings.vfg) + 
					printPadded(this.items[i].key,this.len + 2) + "\1h" +
					printPadded(this.items[i].value,this.max)
				);
		}
	}

	var form = new Form(obj);
	var update = false;
	form.draw();
	
	while(1) {
		
		if(update) 
			form.drawItem(form.index,true);
		else
			update = true;
		
		var cmd = console.inkey(K_NOECHO,1);
		switch(cmd) {
			case KEY_DOWN:
				form.drawItem(form.index);
				form.index++;
				if(form.index >= form.items.length)
					form.index = 0;
				break;
			case KEY_UP:
				form.drawItem(form.index);
				form.index--;
				if(form.index < 0)
					form.index = form.items.length-1;
				break;
			case '\r':
			case '\n':
				form.edit();
				break;
			case '\b':
			case '\x11':
			case '\x1b':
				return form.object;
			default:
				update = false;
				break;
		}
	}
	
	/* just in case?? */
	return form.object;
}

