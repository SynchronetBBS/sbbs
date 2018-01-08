// $Id$

// Library for dealing with user Avatars (ex-ASCII/ANSI block art)

const defs = {
	width: 10,
	height: 6,
};

function local_fname(usernum)
{
	return format("%suser/%04u.ini", system.data_dir, usernum);
}

function qnet_fname(netaddr)
{
	return format("%sqnet/%s.avatars.ini", system.data_dir, file_getname(netaddr));
}

function write_local(usernum, obj)
{
	var file = new File(this.local_fname(usernum));
	if(!file.open(file.exists ? 'r+':'w+')) {
		return false;
	}
	var result = file.iniSetObject("avatar", obj);
	file.close();
	return result;
}

function write_qnet(username, netaddr, obj)
{
	var file = new File(this.qnet_fname(netaddr));
	if(!file.open(file.exists ? 'r+':'w+')) {
		return false;
	}
	var result = file.iniSetObject(username, obj);
	file.close();
	return result;

}

function read_local(usernum)
{
	var file = new File(this.local_fname(usernum));
	if(!file.open("r")) {
		return false;
	}
	var obj = file.iniGetObject("avatar");
	file.close();
	return obj;
}

function read_qnet(username, netaddr)
{
	var file = new File(this.qnet_fname(netaddr));
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
		return read_local(usernum);
	else
		return read_qnet(username, netaddr);
}

function update_local(usernum, data)
{
	var obj = read_local(usernum);
	if(obj == false)
		obj = { created: new Date() };
	obj.data = data;
	obj.updated = new Date();
	return write_local(usernum, obj);
}

function import_file(usernum, filename, offset)
{
	load('graphic.js');
	var graphic = new Graphic(this.defs.width, this.defs.height);
	if(!graphic.load(filename, offset))
		return false;
	return update_local(usernum, base64_encode(graphic.BIN));
}

function find_name(objs, name)
{
	for(var i=0; i < objs.length; i++)
		if(objs[i].name.toLowerCase() == name.toLowerCase())
			return i;
	return -1;
}

function import_list(netaddr, list)
{
	var objs = [];
	var file = new File(this.qnet_fname(netaddr));
	if(file.open("r")) {
		objs = file.iniGetAllObjects();
		file.close();
	}
	for(var i in list) {
		for(var u in list[i]) {
			var index = find_name(objs, list[i][u]);
			if(index >= 0) {
				if(objs[index].data != i) {
					objs[index].data = i;
					objs[index].updated = new Date();
				}
			} else
				objs.push({ name: list[i][u], data: i, created: new Date() });
		}
	}
	if(!file.open("w"))
		return false;
	var result = file.iniSetAllObjects(objs);
	file.close();
	return result;
}

function draw(usernum, username, netaddr, above, right)
{
	var avatar = this.read(usernum, username, netaddr);
	if(avatar) {
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
		};
	}
}

this;
