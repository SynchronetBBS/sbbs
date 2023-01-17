// Helper load-library for configuration (ctrl/????.ini) files
// Replacement for cnflib.js use in SBBS v3.20+

const backup_level = 10;

function read_sections(f, prefix)
{
	return f.iniGetAllObjects("code", prefix, /* lowercase: */false, /* blanks: */true);
}

function read_main_ini(filename)
{
}

function read_msgs_ini(filename)
{
}

function read_file_ini(filename)
{
}

function find_xtrn_sec(obj, code)
{
	for(var i = 0; i < obj.sec.length; i++) {
		if(obj.sec[i].code == code)
			return i;
	}
	return -1;
}

function read_xtrn_ini(filename)
{
	var f = new File(filename);
	if(!f.open("r"))
		throw "Error " + f.error + " opening " + f.name;
	var obj = {};
	obj.event = read_sections(f, "event:");
	obj.editor = read_sections(f, "editor:");
	obj.sec = read_sections(f, "sec:");
	obj.prog = read_sections(f, "prog:");
	obj.native = read_sections(f, "native:");
	
	for(var i in obj.prog) {
		var item = obj.prog[i];
		var i = item.code.indexOf(':');
		if(i >= 0) {
			var seccode = item.code.substring(0, i);
			var secnum = find_xtrn_sec(obj, seccode);
			if(secnum < 0)
				throw "Invalid xtrn prog sec name: " + seccode;
			item.sec = secnum;
			item.code = item.code.substring(i + 1);
		}
	}
	return obj;
}

function write_sections(f, arr, prefix)
{
	if(!arr)
		return;
	f.iniRemoveSections(prefix);
	for(var i = 0; arr && i < arr.length; i++) {
		var item = arr[i];
		f.iniSetObject(prefix + item.code, item);
	}
}

function write_xtrn_ini(filename, obj)
{
	file_backup(filename, backup_level);
	var f = new File(filename);
	if(!f.open(f.exists ? 'r+' : 'w+'))
		throw "Error " + f.error + " opening/creating " + f.name;
	
	for(var i = 0; obj.prog && i < obj.prog.length; i++) {
		var item = obj.prog[i];
		if(obj.sec[item.sec] === undefined)
			throw "Invalid section number: " + item.sec;
		item.code = obj.sec[item.sec].code + ":" + item.code;
	}
	write_sections(f, obj.event, "event:");
	write_sections(f, obj.editor, "editor:");
	write_sections(f, obj.sec, "sec:");
	write_sections(f, obj.prog, "prog:");
	write_sections(f, obj.native, "native:");
	return true;
}

function read_chat_ini(filename)
{
}

function read(filename, struct)
{
	if(struct)
		throw "struct argument not supported";
	switch(file_getname(filename)) {
		case "main.ini":
			return read_main_ini(filename);
		case "msgs.ini":
			return read_msgs_ini(filename);
		case "file.ini":
			return read_file_ini(filename);
		case "xtrn.ini":
			return read_xtrn_ini(filename);
		case "chat.ini":
			return read_chat_ini(filename);
	}
	throw "unsupported filename: " + filename;
}

function write(filename, struct, obj)
{
	if(struct)
		throw "struct argument not supported";
	switch(file_getname(filename)) {
		case "main.ini":
			return write_main_ini(filename, obj);
		case "msgs.ini":
			return write_msgs_ini(filename, obj);
		case "file.ini":
			return write_file_ini(filename, obj);
		case "xtrn.ini":
			return write_xtrn_ini(filename, obj);
		case "chat.ini":
			return write_chat_ini(filename, obj);
	}
	throw "unsupported filename: " + filename;
	
}

this;
