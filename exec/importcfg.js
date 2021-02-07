// *****************************************************************
// Import a list of configuration items (e.g. msg areas, file areas)
// from a line-delimited JSON file (e.g. exported via exportcfg.js -json
// *****************************************************************

"use strict";

var cnflib = load({}, "cnflib.js");
var file_cnf = cnflib.read("file.cnf");

var cfgtype;
var cfgtypes = {
	'msg-grps': 	msg_area.grp,
	'msg-subs':		msg_area.sub,
	'file-libs': 	file_area.lib,
	'file-dirs':	file_area.dir,
	'file-prots':	file_cnf.prot,
	'file-extrs':   file_cnf.fextr,
	'file-comps':   file_cnf.fcomp,
	'file-viewers': file_cnf.fview,
	'file-testers': file_cnf.ftest,
	'file-dlevents':file_cnf.dlevent,
	'text-secs':	file_cnf.txtsec,
	'xtrn-secs': 	xtrn_area.sec,
	'xtrn-progs': 	xtrn_area.prog,
	'xtrn-events': 	xtrn_area.event,
	'xtrn-editors':	xtrn_area.editor,
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

var cnf_objs = {
	'msg-grps': 	'grp',
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
const cnf_obj = cnf_objs[cfgtype];
const cnf_fname = system.ctrl_dir + cnf_fnames[cfgtype];
var cnf = cnflib.read(cnf_fname);
if(!cnf) {
	alert("Failed to read " + cnf_fname);
	exit(-1);
}

for(var i in list) {
	var obj = list[i];
	var j;
	for(j = 0; j < cnf[cnf_obj].length; j++) {
		if(cnf[cnf_obj][j].code !== undefined) {
			if(cnf[cnf_obj][j].code == obj.code)
				break;
		} else {
			if(cnf[cnf_obj][j].name == obj.name)
				break;
		}
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
	writln("Saving changes to " + cnf_fname);
	if(!cnflib.write(cnf_fname, undefined, cnf)) {
		alert("Failed to write " + cnf_fname);
		exit(-1);
	}
}
exit(0);
