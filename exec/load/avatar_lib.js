// $Id$

// Library for dealing with user Avatars (ex-ASCII/ANSI block art)

const defs = {
	width: 10,
	height: 6,
};

const size = defs.width * defs.height * 2;	// 2 bytes per cell for char and attributes

function local_library()
{
	return format("%savatars/", system.text_dir);
}

function localuser_fname(usernum)
{
	return format("%suser/%04u.ini", system.data_dir, usernum);
}

function netuser_fname(netaddr)
{
	return format("%sqnet/%s.avatars.ini", system.data_dir, file_getname(netaddr));
}

function is_valid(buf)
{
	if(!buf || !buf.length || buf.length != this.size)
		return false;
	var invalid = buf.split('').filter(function (e,i) { 
		if((i&1) == 0) { // char
			switch(e) {
				case '\r':
				case '\n':
				case '\x07':	// Beep/BEL ('\a')
				case '\b':
				case '\t':
				case '\f':
				case '\x1b':	// ESC ('\e')
				case '\xff':	// Telnet IAC
					return true;
			}
			return false;
		}
		// attr
		return (ascii(e)&BLINK);
		});
	return invalid.length == 0;
}

function write_localuser(usernum, obj)
{
	var file = new File(this.localuser_fname(usernum));
	if(!file.open(file.exists ? 'r+':'w+')) {
		return false;
	}
	var result = file.iniSetObject("avatar", obj);
	file.close();
	return result;
}

function write_netuser(username, netaddr, obj)
{
	var file = new File(this.netuser_fname(netaddr));
	if(!file.open(file.exists ? 'r+':'w+')) {
		return false;
	}
	var result = file.iniSetObject(username, obj);
	file.close();
	return result;
}

function read_localuser(usernum)
{
	var file = new File(this.localuser_fname(usernum));
	if(!file.open("r")) {
		return false;
	}
	var obj = file.iniGetObject("avatar");
	file.close();
	return obj;
}

function read_netuser(username, netaddr)
{
	var file = new File(this.netuser_fname(netaddr));
	if(!file.open("r")) {
		return false;
	}
	var obj = file.iniGetObject(username);
	file.close();
	return obj;
}

function read(usernum, username, netaddr)
{
	if(parseInt(usernum) >= 1)
		return read_localuser(usernum);
	else
		return read_netuser(username, netaddr);
}

function update_localuser(usernum, data)
{
	var obj = read_localuser(usernum);
	if(obj == false)
		obj = { created: new Date() };
	obj.data = data;
	obj.updated = new Date();
	return write_localuser(usernum, obj);
}

function import_file(usernum, filename, offset)
{
	load('graphic.js');
	var graphic = new Graphic(this.defs.width, this.defs.height);
	if(!graphic.load(filename, offset))
		return false;
	if(!is_valid(graphic.BIN))
		return false;
	return update_localuser(usernum, base64_encode(graphic.BIN));
}

// Uses Graphic.draw() at an absolute screen coordinate
function draw(usernum, username, netaddr, above, right)
{
	var avatar = this.read(usernum, username, netaddr);
	if(!avatar)
		return false;
	load('graphic.js');
	var graphic = new Graphic(this.defs.width, this.defs.height);
	try {
		graphic.BIN = base64_decode([avatar.data]);
		var lncntr = console.line_counter;
		var pos = console.getxy();
		var x = pos.x;
		var y = pos.y;
		if(above)
			y -= this.defs.height;
		if(right)
			x = console.screen_columns - (this.defs.width + 1);
		graphic.attr_mask = ~graphic.defs.BLINK;	// Disable blink attribute (consider iCE colors?)
		graphic.draw(x, y, this.defs.width, this.defs.height);
		console.gotoxy(pos);
		console.line_counter = lncntr;
	} catch(e) {
		return false;
	};
	return true;
}

// Uses console.write() where-ever the cursor happens to be
function show(usernum, username, netaddr)
{
	var avatar = this.read(usernum, username, netaddr);
	if(!avatar)
		return false;
	load('graphic.js');
	var graphic = new Graphic(this.defs.width, this.defs.height);
	graphic.attr_mask = ~graphic.defs.BLINK;	// Disable blink attribute (consider iCE colors?)
	try {
		graphic.BIN = base64_decode([avatar.data]);
		console.write(graphic.ANSI);
	} catch(e) {
		return false;
	};
	return true;
}


this;
