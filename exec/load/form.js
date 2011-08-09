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
if(js.global.Scrollbar == undefined)
	load("scrollbar.js");
	
function formEdit(obj,parent)
{
	function Form(obj,parent) 
	{
		var full_redraw = false;
		this.parent = parent;
		this.typelen = 4;
		this.keylen = 3;
		this.max_value = 40;
		this.max_lines;
		this.index = 0;
		this.line = 1;
		this.items = [];
		
		this.full_redraw getter = function() {
			return full_redraw;
		}
		
		this.full_redraw setter = function(bool) {
			full_redraw = bool;
		}
		
		this.init = function() {
			/* determine object type */
			if(typeof obj != "object")
				throw("invalid data type: " + typeof obj);
			else if(obj.length != undefined)
				this.type = "array";
			else
				this.type = "object";
			
			/* load object values into form */
			for(var k in obj) {
				if(console.strlen(k) > this.keylen)
					this.keylen = console.strlen(k);
				if(console.strlen(typeof obj[k]) > this.typelen)
					this.typelen = console.strlen(typeof obj[k]);
				this.items.push({
					key:k,
					value:obj[k],
					type:typeof obj[k]
				});
			}
			
			/* initialize editing window */
			var height = this.items.length + 3;
			var width = this.typelen + this.keylen + this.max_value + 4;
			var posx = (console.screen_columns / 2) - (width / 2);
			var posy = (console.screen_rows / 2) - (height / 2);
			
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
				printPadded("KEY",this.typelen+1) + 
				printPadded("KEY",this.keylen+1) + 
				splitPadded("VALUE","ESC",this.max_value),
				posx,
				posy,
				width,
				height
			);
			
			this.max_lines = this.box.height - 3;
			this.box.show_tabs = false;
			this.scrollbar = new Scrollbar(posx + width -1, posy + 2, this.max_lines, "vertical");
			this.box.drawView();
		}
		
		this.edit = function() {
			var item = this.items[this.index];
			
			if(item.type == "object") {
				item.value = formEdit(item.value,this);
				this.full_redraw = true;
				this.draw(true);
			}
			else if(item.type != "function") {
				var coords = this.coords(this.index);
				console.attributes = BG_LIGHTGRAY + BLACK;
				console.gotoxy(coords.x + this.typelen + this.keylen + 2, coords.y);
				clearLine(this.max_value);
				console.gotoxy(coords.x + this.typelen + this.keylen + 2, coords.y);
				
				var newvalue = console.getstr(item.value.toString().replace(/\r\n/g,'\\r\\n'),this.max_value,K_EDIT|K_AUTODEL);
				switch(item.type) {
					case "number":
						item.value = Number(newvalue);
						break;
					case "boolean":
						item.value = Boolean(eval(newvalue));
						break;
					case "string":
						item.value = newvalue.replace(/\\\\/g,"\\");
						break;
				}
			}
		}
		
		this.draw = function(all) {
			/* if a full redraw is requested, draw everything */
			if(this.full_redraw) {
				if(this.parent)
					this.parent.draw(all);
					
				this.full_redraw = false;
				this.box.drawView();
			}
			
			/* declare starting index for scrollbar usage */
			var first_line = 0;
			
			if(all) {
				/* find starting point */
				i = (this.index+1) - this.max_lines;
				var c = 0;
				if(i < 0)
					i = 0;
				first_line = i;

				while(i < this.items.length && c < this.max_lines) {
					this.drawItem(i,this.index == i);
					i++;
					c++;
				}
			}
			
			else 
				this.drawItem(this.index,true);
			
			if(this.items.length > this.max_lines) 
				this.scrollbar.draw(first_line,this.items.length);
		}
		
		this.object getter = function() {
			if(this.type == "array")
				obj = [];
			else
				obj = {};
			for(var i = 0; i < this.items.length; i++) {
				var item = this.items[i];
				obj[item.key] = item.value;
			}
			return obj;
		}
		
		this.coords = function(i) {
			var posx = this.box.x + 1;
			var posy = this.box.y + 2;
			
			if(this.index >= this.max_lines) 
				posy += this.max_lines - (this.index - i) - 1;
			else 
				posy += i;
			return {
				x:posx,
				y:posy,
			}
		}
		
		this.drawItem = function(i,curr) {
			var coords = this.coords(i);
			var item = this.items[i];
			var value = item.value;
			var type = item.type;
			var can_edit = true;
			
			if(type == "object") {
				if(item.value.length != undefined) {
					type = "array";
					value = "[Array]";
				}
				else
					value = "[Object]";
					
			}
			else if(type == "function") {
				can_edit = false;
				type = "function";
				value = "{...}";
			}
			else if(type == "string") {
				value = value.replace(/\r\n/g,'\\r\\n');
			}
			else {
				value = value.toString();
			}
			
			console.gotoxy(coords);
			if(curr) {
				console.attributes = layout_settings.lbg + (can_edit?layout_settings.lfg:RED);
				console.write(
					printPadded(type,this.typelen + 1) + 
					printPadded(item.key,this.keylen + 1) + 
					printPadded(value,this.max_value)
				);
			}
			else {
				console.attributes = layout_settings.vbg + (can_edit?layout_settings.vfg:RED);
				console.write(
					printPadded(type,this.typelen + 1) + 
					printPadded(item.key,this.keylen + 1) + 
					printPadded(value,this.max_value)
				);
			}
		}
		
		this.init();
	}

	var form = new Form(obj,parent);
	var update = false;
	var redraw = false;
	form.draw(true);
	
	while(!js.terminated) {
		
		if(update) 
			form.draw(redraw);
			
		redraw = false;
		update = true;
		var cmd = console.inkey(K_NOECHO,1);

		switch(cmd) {
		case KEY_HOME:
			form.drawItem(form.index);
			if(form.index >= form.max_lines)
				redraw = true;
			form.index = 0;
			form.line = 1;
			break;
		case KEY_END:
			form.drawItem(form.index);
			form.index = form.items.length-1;
			form.line = form.items.length;
			if(form.index >= form.max_lines - 1) {
				form.line = form.max_lines;
				redraw = true;
			}
			break;
		case KEY_DOWN:
			form.drawItem(form.index);
			form.index++;
			if(form.line < form.max_lines)
				form.line++;
			if(form.index >= form.max_lines)
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
			if(form.index >= form.max_lines - 1) {
				form.line = form.max_lines;
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
	
}

