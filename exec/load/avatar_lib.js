// Library for dealing with user Avatars (ex-ASCII/ANSI block art)

const defs = {
	width: 10,
	height: 6,
};

const QWK_ID_PATTERN = /^[A-Z][\w|-]{1,7}$/;
const FTN_3D_PATTERN = /^(\d+):(\d+)\/(\d+)$/;
const FTN_4D_PATTERN = /^(\d+):(\d+)\/(\d+)\.(\d+)$/;

const size = defs.width * defs.height * 2;	// 2 bytes per cell for char and attributes

var cache = {};

function cache_key(username, netaddr)
{
	var key = username;
	if(netaddr)
		key += ( '@' + netaddr );
	return key;
}

function cache_set(username, obj, netaddr)
{
	cache[cache_key(username, netaddr)] = obj;
}

function cache_get(username, netaddr)
{
	return cache[cache_key(username, netaddr)];
}

function local_library()
{
	return format("%savatars/", system.text_dir);
}

function fullpath(filename)
{
	if(file_getname(filename) != filename)
		return filename;
	return local_library() + filename;
}

function localuser_fname(usernum)
{
	return format("%suser/%04u.ini", system.data_dir, usernum);
}

function bbsindex_fname()
{
	return system.data_dir + "bbses.ini";
}

function bbsuser_fname(bbsname)
{
	var file = File(this.bbsindex_fname());
	if(!file.open("r"))
		return false;
	var netaddr = file.iniGetValue(bbsname, "netaddr", "");
	file.close();
	if(!netaddr || !netaddr.length)
		return false;
	return netuser_fname(netaddr);
}

function netuser_fname(netaddr)
{
	var fido = netaddr.match(FTN_3D_PATTERN);
	if(!fido)
		fido = netaddr.match(FTN_4D_PATTERN);
	if(fido)
		return format("%sfido/%04x%04x.avatars.ini", system.data_dir, fido[2], fido[3]);
	if(file_getname(netaddr).search(QWK_ID_PATTERN) == 0)
		return format("%sqnet/%s.avatars.ini", system.data_dir, file_getname(netaddr));
	return false;
}

function is_valid(buf)
{
	load("cga_defs.js");
	if(!buf || !buf.length || buf.length != this.size)
		return false;
	var invalid = buf.split('').filter(function (e,i) { 
		if((i&1) == 0) { // char
			switch(e) {
				case '\x00':
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
	cache_set(usernum, obj);
	return result;
}

function enable_localuser(usernum, enabled)
{
	var file = new File(this.localuser_fname(usernum));
	if(!file.open(file.exists ? 'r+':'w+')) {
		return false;
	}
	var result;
	var obj = file.iniGetObject("avatar");
	if(!obj || obj.disabled == !enabled)
		result = false;
	else {
		obj.disabled = !enabled;
		obj.updated = new Date();
		file.ini_key_prefix = '\t';
		file.ini_section_separator = '\n';
		file.ini_value_separator = ' = ';
		result = file.iniSetObject("avatar", obj);
		cache_set(usernum, obj);
	}
	file.close();
	return result;
}

function remove_localuser(usernum)
{
	var file = new File(this.localuser_fname(usernum));
	if(!file.open(file.exists ? 'r+':'w+')) {
		return false;
	}
	var result = file.iniRemoveSection("avatar");
	file.close();
	cache_set(usernum, undefined);
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
	cache_set(username, obj, netaddr);
	return result;
}

function read_localuser(usernum)
{
	if(!usernum)
		return false;
	var file = new File(this.localuser_fname(usernum));
	if(!file.open("r")) {
		return false;
	}
	var obj = file.iniGetObject("avatar");
	file.close();
	return obj;
}

// netaddr may be a network address OR a BBS name
function read_netuser(username, netaddr)
{
	var filename = this.netuser_fname(netaddr);
	if(!filename && netaddr.indexOf('@') == -1)
		filename = this.bbsuser_fname(netaddr);
	if(!filename)
		return false;
	var file = new File(filename);
	if(!file.open("r")) {
		return false;
	}
	var obj = file.iniGetObject(username);
	file.close();
	return obj;
}

function read(usernumber, username, netaddr, bbsid)
{
	var usernum = parseInt(usernumber, 10);
	if(!usernum && !username)
		return false;
	var obj = cache_get(usernum >= 1 ? usernum : username, netaddr);
	if(obj !== undefined)	// null and false are also valid cached avatar values
		return obj;
	if(usernum >= 1)
		obj = read_localuser(usernum);
	else if(!netaddr)
		obj = read_localuser(system.matchuser(username));
	else {
		obj = read_netuser(username, netaddr);
		if(!obj && bbsid)
			obj = read_netuser(username, bbsid);
		if(!obj) {
			var namehash = "md5:" + md5_calc(username);
			obj = read_netuser(namehash, netaddr);
			if(!obj && bbsid)
				obj = read_netuser(namehash, bbsid);
		}
	}
	cache_set(usernum >= 1 ? usernum : username, obj, netaddr);
	return obj;
}

function update_localuser(usernum, data)
{
	var obj = read_localuser(usernum);
	if(!obj)
		obj = { created: new Date() };
	obj.data = data;
	obj.updated = new Date();
	return write_localuser(usernum, obj);
}

function import_file(usernum, filename, offset)
{
	sauce_lib = load({}, 'sauce_lib.js');
	var sauce = sauce_lib.read(fullpath(filename));
	if(sauce) {
		var num_avatars = sauce.filesize / this.size;
		if(num_avatars < 1) {
			alert("Number of avatars: " + num_avatars);
			return false;
		}
		if(offset >= num_avatars)
			return false;
		else if(offset == undefined)
			offset = random(num_avatars);
	}
	load('graphic.js');
	var graphic = new Graphic(this.defs.width, this.defs.height);
	try {
		if(!graphic.load(fullpath(filename), offset))
			return false;
	} catch(e) {
		alert(e);
		return false;
	}
	if(!is_valid(graphic.BIN))
		return false;
	var data = base64_encode(graphic.BIN);
	if(!usernum)
		return data;
	return update_localuser(usernum, data);
}

function is_enabled(obj)
{
	return obj
		&& typeof obj == 'object' 
		&& typeof obj.data == 'string'
		&& obj.data.length
		&& !obj.disabled;
}

// Uses Graphic.draw() at an absolute screen coordinate
function draw(usernum, username, netaddr, above, right, top, cols)
{
	var avatar = this.read(usernum, username, netaddr, usernum);
	if(!is_enabled(avatar))
		return false;
	return draw_bin(avatar.data, above, right, top, cols);
}

function draw_bin(data, above, right, top, cols)
{
	if(!cols || (cols > console.screen_columns))
		cols = console.screen_columns;
	load('graphic.js');
	var graphic = new Graphic(this.defs.width, this.defs.height);
	try {
		graphic.BIN = base64_decode(data);
		var lncntr = console.line_counter;
		var pos = console.getxy();
		var x = pos.x;
		var y = pos.y;
		if(top)
			y = 1;
		else if(above)
			y -= this.defs.height;
		if(right)
			x = cols - (this.defs.width + 1);
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
	var avatar = this.read(usernum, username, netaddr, usernum);
	if(!is_enabled(avatar))
		return false;
	return show_bin(avatar.data);
}

function show_bin(data)
{
	load('graphic.js');
	var graphic = new Graphic(this.defs.width, this.defs.height);
	graphic.attr_mask = ~graphic.defs.BLINK;	// Disable blink attribute (consider iCE colors?)
	try {
		graphic.BIN = base64_decode(data);
		console.print(graphic.MSG);
	} catch(e) {
		return false;
	};
	return true;
}


this;
