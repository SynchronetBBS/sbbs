// Helper load-library for configuration (ctrl/????.ini) files
// Replacement for cnflib.js use in SBBS v3.20+

const backup_level = 10;

function read_sections(f, prefix)
{
	return f.iniGetAllObjects("code", prefix, /* lowercase: */false, /* blanks: */true);
}

function read_main_ini(filename)
{
	var f = new File(filename);
	if(!f.open("r"))
		throw new Error("Error " + f.error + " opening " + f.name);
	var obj = f.iniGetObject();
	obj.newuser = f.iniGetObject("newuser");
	obj.expired = f.iniGetObject("expired");
	obj.mqtt = f.iniGetObject("MQTT");
	obj.module = f.iniGetObject("module");
	obj.shell = read_sections(f, "shell:");
	f.close();
	return obj;
}

function read_file_ini(filename)
{
	var f = new File(filename);
	if(!f.open("r"))
		throw new Error("Error " + f.error + " opening " + f.name);
	var obj = f.iniGetObject();
	obj.extractor = read_sections(f, "extractor:");
	obj.compressor = read_sections(f, "compressor:");
	obj.viewer = read_sections(f, "viewer:");
	obj.tester = read_sections(f, "tester:");
	obj.protocol = read_sections(f, "protocol:");
	f.close();
	return obj;
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
		throw new Error("Error " + f.error + " opening " + f.name);
	var obj = {};
	obj.event = read_sections(f, "event:");
	obj.editor = read_sections(f, "editor:");
	obj.sec = read_sections(f, "sec:");
	obj.prog = read_sections(f, "prog:");
	obj.native = read_sections(f, "native:");
	obj.hotkey = read_sections(f, "hotkey:");
	
	for(var i in obj.prog) {
		var item = obj.prog[i];
		var i = item.code.indexOf(':');
		if(i >= 0) {
			var seccode = item.code.substring(0, i);
			var secnum = find_xtrn_sec(obj, seccode);
			if(secnum < 0)
				throw new Error("Invalid xtrn prog sec name: " + seccode);
			item.sec = secnum;
			item.code = item.code.substring(i + 1);
		}
	}
	f.close();
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
		throw new Error("Error " + f.error + " opening/creating " + f.name);
	
	for(var i = 0; obj.prog && i < obj.prog.length; i++) {
		var item = obj.prog[i];
		if(obj.sec[item.sec] === undefined)
			throw new Error("Invalid section number: " + item.sec);
		item.code = obj.sec[item.sec].code + ":" + item.code;
	}
	write_sections(f, obj.editor, "editor:");
	write_sections(f, obj.sec, "sec:");
	write_sections(f, obj.prog, "prog:");
	write_sections(f, obj.event, "event:");
	write_sections(f, obj.native, "native:");
	write_sections(f, obj.hotkey, "hotkey:");
	f.close();
	return true;
}

function read(filename, struct)
{
	if(struct)
		throw new Error("struct argument not supported");
	switch(file_getname(filename)) {
		case "main.ini":
			return read_main_ini(filename);
		case "msgs.ini":
			throw new Error("reading msgs.ini not yet supported");
		case "file.ini":
			return read_file_ini(filename);
		case "xtrn.ini":
			return read_xtrn_ini(filename);
		case "chat.ini":
			throw new Error("reading chat.ini not yet supported");
	}
	throw new Error("unsupported filename: " + filename);
}

function write(filename, struct, obj)
{
	if(struct)
		throw new Error("struct argument not supported");
	switch(file_getname(filename)) {
		case "main.ini":
			throw new Error("writing main.ini not yet supported");
		case "msgs.ini":
			throw new Error("writing msgs.ini not yet supported");
		case "file.ini":
			throw new Error("writing file.ini not yet supported");
		case "xtrn.ini":
			return write_xtrn_ini(filename, obj);
		case "chat.ini":
			throw new Error("writing chat.ini not yet supported");
	}
	throw new Error("unsupported filename: " + filename);
}

this;
