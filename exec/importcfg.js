// *****************************************************************
// Import a list of configuration items (e.g. msg areas, file areas)
// from a line-delimited JSON file (e.g. exported via exportcfg.js -json
// *****************************************************************

"use strict";

var cnflib = load({}, "cnflib.js");
var file_cnf = cnflib.read("file.cnf");

var cfgtype;
var cfgtypes = {
	'msg-grps':		'grp',
	'msg-subs':		'sub',
	'file-libs': 	'lib',
	'file-dirs':	'dir',
	'file-prots':	'prot',
	'file-extrs':   'fextr',
	'file-comps':   'fcomp',
	'file-viewers': 'fview',
	'file-testers': 'ftext',
	'file-dlevents':'dlevent',
	'text-secs':	'txtsec',
	'xtrn-secs': 	'xtrnsec',
	'xtrn-progs': 	'xtrn',
	'xtrn-events': 	'event',
	'xtrn-editors':	'xedit',
};

var cnf_fnames = {
	'msg-grps': 	'msgs.cnf',
	'msg-subs':		'msgs.cnf',
	'file-libs': 	'file.cnf',
	'file-dirs':	'file.cnf',
	'file-prots':	'file.cnf',
	'file-extrs':   'file.cnf',
	'file-comps':   'file.cnf',
	'file-viewers': 'file.cnf',
	'file-testers': 'file.cnf',
	'file-dlevents':'file.cnf',
	'text-secs':	'file.cnf',
	'xtrn-secs': 	'xtrn.cnf',
	'xtrn-progs': 	'xtrn.cnf',
	'xtrn-events': 	'xtrn.cnf',
	'xtrn-editors':	'xtrn.cnf',
};

var filename;

function usage(msg)
{
	if(msg) {
		writeln();
		alert(msg);
		writeln();
	}
	writeln("usage: importcfg.js [-overwrite] [-debug] <cfg-type> <filename>");
	writeln();
	writeln("cfg-types (choose one):");
	for(var c in cfgtypes)
		writeln("\t" + c);
	writeln();
	exit(0);
}

var options = {};
for(var i = 0; i < argc; i++) {
	var arg = argv[i];
	if(arg.charAt(0) == '-')
		options[arg.slice(1)] = true;
	else {
		if(cfgtype === undefined) {
			if(cfgtypes[arg] === undefined)
				usage("unsupported cfg-type: " + arg);
			cfgtype = arg;
		} else
			filename = arg;
	}
}

if(!cfgtype)
	usage("cfg-type not specified");

var file;
if(filename) {
	file = new File(filename);
	writeln("Opening " + filename);
	if(!file.open("r")) {
		alert("ERROR " + file.error + " opening " + file.name);
		exit(1);
	}
}
else {
	writeln("Reading from stdin");
	file = stdin;
}

var text = file.readAll(64*1024);
var list = [];
for(var i in text) {
	var obj;
	try {
		obj = JSON.parse(text[i]);
	} catch(e) {
		alert("line " + (Number(i) + 1));
		alert(e);
		exit(1);
	}
	list.push(obj);
}

/* Add list of imported objects into the .cnf file */
const cnf_obj = cfgtypes[cfgtype];
const cnf_fname = system.ctrl_dir + cnf_fnames[cfgtype];
var cnf = cnflib.read(cnf_fname);
if(!cnf) {
	alert("Failed to read " + cnf_fname);
	exit(-1);
}
writeln("Backing up " + cnf_fname);
file_backup(cnf_fname,15);

if (cnf_obj == 'xtrn') {
	var secs = {};
	for (var i in cnf['xtrnsec']) {
		secs[cnf['xtrnsec'][i]['code'].toUpperCase()] = i;
	}
}

for(var i in list) {
	var obj = list[i];
	var j;
	for(j = 0; j < cnf[cnf_obj].length; j++) {
		if(cnf[cnf_obj][j].code !== undefined) {
			if(cnf[cnf_obj][j].code.toUpperCase() == obj.code.toUpperCase())
				break;
		} else {
			if(cnf[cnf_obj][j].name.toUpperCase() == obj.name.toUpperCase())
				break;
		}
	}

	obj.code = obj.code.toUpperCase();
	switch (cnf_obj) {
		case 'xtrnsec':
			delete obj.number;
			delete obj.index;
			break;
		case 'xtrn':
			obj.sec_code = obj.sec_code.toUpperCase();
			delete obj.number;
			delete obj.index;
			delete obj.sec_index;
			delete obj.sec_number;
			if (typeof secs[obj.sec_code] === "undefined") {
				writeln("Could not find section " + obj.sec_code + " for program " + (obj.code || obj.name));
				continue;
			} else {
				obj.sec = secs[obj.sec_code];
			}
			break;
		default:
			break;
	}

	if(j < cnf[cnf_obj].length) {
		if(!options.overwrite) {
			writeln("Already exists: " + (obj.name || obj.code));
			continue;
		}
		writeln("Overwriting: " + (obj.name || obj.code));
		cnf[cnf_obj][j] = obj;
	} else {
		writeln("Adding: " + (obj.name || obj.code));
		cnf[cnf_obj].push(obj);
	}
}

if(options.debug)
	writeln(JSON.stringify(cnf[cnf_obj], null, 4));
else {
	writeln("Saving changes to " + cnf_fname);
	if(!cnflib.write(cnf_fname, undefined, cnf)) {
		alert("Failed to write " + cnf_fname);
		exit(-1);
	}
}
exit(0);
