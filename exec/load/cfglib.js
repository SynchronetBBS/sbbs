// Helper load-library for configuration (ctrl/????.ini) files
// Replacement for cnflib.js use in SBBS v3.20+

var backup_level = 10;

function read_sections(f, prefix)
{
	return f.iniGetAllObjects("code", prefix, /* lowercase: */false, /* blanks: */true);
}

// Section values as strings, exactly as they appear in the file: typed values
// would not survive being written back (e.g. extension=001 would become 1)
function read_string_sections(f, prefix)
{
	var list = [];
	var sections = f.iniGetSections(prefix);
	for(var i = 0; i < sections.length; i++) {
		var obj = { code: sections[i].substr(prefix.length) };
		var keys = f.iniGetKeys(sections[i]);
		for(var k = 0; k < keys.length; k++)
			obj[keys[k]] = f.iniGetValue(sections[i], keys[k], "");
		list.push(obj);
	}
	return list;
}

// The file type handler sections of file.ini, which share one format
var file_handler_types = ["extractor", "compressor", "viewer", "tester"];

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
	for(var i = 0; i < file_handler_types.length; i++)
		obj[file_handler_types[i]] = read_string_sections(f, file_handler_types[i] + ":");
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

// Writes only the file type handler sections present in obj (e.g. obj.viewer),
// numbered in array order: the order in which they are tried. The root section,
// protocols, libraries and directories are left as they are. Sections already
// numbered in file order (as SCFG writes them) are updated in place, and only
// where their contents changed.
function write_file_ini(filename, obj)
{
	file_backup(filename, backup_level);
	var f = new File(filename);
	if(!f.open(f.exists ? 'r+' : 'w+'))
		throw new Error("Error " + f.error + " opening/creating " + f.name);
	f.ini_key_prefix = "\t";
	f.ini_section_separator = "";
	for(var t = 0; t < file_handler_types.length; t++) {
		var type = file_handler_types[t];
		var list = obj[type];
		if(!list)
			continue;
		var sections = f.iniGetSections(type + ":");
		var in_order = true;
		for(var i = 0; i < sections.length; i++) {
			if(sections[i].toLowerCase() != type + ":" + i)
				in_order = false;
		}
		if(!in_order) {
			f.iniRemoveSections(type + ":");
			sections = [];
		}
		for(var i = list.length; i < sections.length; i++)
			f.iniRemoveSection(sections[i]);
		for(var i = 0; i < list.length; i++) {
			var item = {};
			for(var k in list[i]) {
				var value = list[i][k];
				if(k == "code" || value === undefined)
					continue;
				if(k == "ex_mode" && typeof value == "number")
					value = value ? "0x" + value.toString(16) : "0";
				item[k] = String(value);
			}
			var section = type + ":" + i;
			if(i < sections.length) {
				// Make the section hold exactly item's keys and values
				var changed = false;
				var keys = f.iniGetKeys(sections[i]);
				for(var k = 0; k < keys.length; k++) {
					if(item[keys[k]] === undefined) {
						f.iniRemoveKey(sections[i], keys[k]);
						changed = true;
					}
				}
				for(var k in item) {
					if(keys.indexOf(k) < 0
						|| f.iniGetValue(sections[i], k, "") !== item[k])
						changed = true;
				}
				if(!changed)
					continue;
			}
			f.iniSetObject(section, item);
		}
	}
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
			return write_file_ini(filename, obj);
		case "xtrn.ini":
			return write_xtrn_ini(filename, obj);
		case "chat.ini":
			throw new Error("writing chat.ini not yet supported");
	}
	throw new Error("unsupported filename: " + filename);
}

this;
