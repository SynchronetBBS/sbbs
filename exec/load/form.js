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
		this.full_redraw = false;
		this.keylen = 3;
		this.max_value = 25;
		this.max_height;
		this.index = 0;
		this.line = 1;
		this.items = [];
		
		this.init = function() {
			/* load object values into form */
			for(var k in obj) {
				if(console.strlen(k) > this.keylen)
					this.keylen = console.strlen(k);
				this.items.push({
					key:k,
					value:obj[k],
					type:typeof obj[k]
				});
			}
			
			/* initialize editing window */
			var posx = (console.screen_columns / 2) - ((this.keylen + 25)/2) -1;
			var posy = (console.screen_rows / 2) - (this.items.length / 2) -2;
			var height = this.items.length + 3;
			var width = this.keylen + this.max_value + 4;
			
			if(posx < 1)
				posx = 1;
			if(posy < 1)
				posy = 1;
			if(height > (console.screen_rows))
				height = console.screen_rows;
			if(width > (console.screen_columns-2)) {
				this.keylen -= (width-console.screen_columns-2);
				width = console.screen_columns-2;
			}
		
			this.box = new Layout_View(
				printPadded("KEY",this.keylen+2) + splitPadded("VALUE","ESC",this.max_value),
				posx,
				posy,
				width,
				height
			);
			this.max_height = this.box.height - 3;
			this.box.show_tabs = false;
			this.box.drawView();
		}
		
		this.edit = function() {
			var item = this.items[this.index];
			
			if(item.type == "object") {
				item.value = formEdit(item.value);
				this.draw();
			}
			else {
				var coords = this.coords(this.index);
				console.attributes = BG_LIGHTGRAY + BLACK;
				console.gotoxy(coords.x + this.keylen + 2, coords.y);
				clearLine(this.max_value);
				console.gotoxy(coords.x + this.keylen + 2, coords.y);
				
				var newvalue = console.getstr(item.value.toString(),this.max_value,K_EDIT|K_AUTODEL);
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
			/* if a full redraw is requested, draw everything */
			if(this.full_redraw) {
				this.full_redraw = false;
				this.box.drawView();
			}
			
			/* find starting point */
			var i = (this.index+1) - this.max_height;
			var c = 0;
			if(i < 0)
				i = 0;
			while(i < this.items.length && c < this.max_height) {
				this.drawItem(i,this.index == i);
				i++;
				c++;
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
			var posx = this.box.x + 1;
			var posy = this.box.y + 2;
			
			if(this.index >= this.max_height) 
				posy += this.max_height - (this.index - i) - 1;
			else 
				posy += i;
			return {
				x:posx,
				y:posy,
			}
		}
		
		this.drawItem = function(i,curr) {
			var coords = this.coords(i);
			console.gotoxy(coords);
			if(curr) 
				console.putmsg(getColor(layout_settings.lbg) + getColor(layout_settings.lfg) + 
					printPadded(this.items[i].key,this.keylen + 2) + "\1w" + 
					printPadded(this.items[i].value,this.max_value)
				);
			else
				console.putmsg(getColor(layout_settings.vbg) + getColor(layout_settings.vfg) + 
					printPadded(this.items[i].key,this.keylen + 2) + "\1h" +
					printPadded(this.items[i].value,this.max_value)
				);
		}
		
		this.init();
	}

	var form = new Form(obj);
	var update = false;
	var redraw = false;
	form.draw();
	
	while(!js.terminated) {
		
		if(redraw) 
			form.draw();
			
		else if(update) 
			form.drawItem(form.index,true);
			
		redraw = false;
		update = true;
		var cmd = console.inkey(K_NOECHO,1);

		switch(cmd) {
		case KEY_DOWN:
			form.drawItem(form.index);
			form.index++;
			if(form.line < form.max_height)
				form.line++;
			if(form.index >= form.max_height)
				redraw = true;
			if(form.index >= form.items.length) {
				form.index = 0;
				form.line = 1;
			}
			break;
		case KEY_UP:
			form.drawItem(form.index);
			form.index--;
			if(form.index < (form.line-1))
				form.line--;
			if(form.index < 0) {
				form.index = form.items.length-1;
				form.line = form.items.length;
			}
			if(form.index >= form.max_height - 1) {
				form.line = form.max_height;
				redraw = true;
			}
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

