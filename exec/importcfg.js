// Imports a limited set of types exported by exportcfg.js
// Example: jsexec exportcfg.js xtrn-secs > secs.txt
//          jsexec importcfg.js xtrn-secs secs.txt

"use strict";

var cnflib = load({}, "cnflib.js");

var cfgtype;
var cfgtypes = {
	'xtrn-secs': 	xtrn_area.sec,
	'xtrn-progs': 	xtrn_area.prog,
	'xtrn-events': 	xtrn_area.event,
	'xtrn-editors':	xtrn_area.editor,
};

function usage(msg)
{
	if(msg) {
		writeln();
		alert(msg);
		writeln();
	}
	writeln("usage: import.js <cfg-type> <import file>");
	writeln("\tonly imports standard tab-delimited file from exportcfg");
	writeln();
	writeln("cfg-types (choose one):");
	for(var c in cfgtypes)
		writeln("\t" + c);
	writeln();
	exit(0);
}

if (argc < 2) {
	usage();
}

if ((typeof argv[0] === "undefined") || (typeof argv[1] === "undefined")) {
	usage();
}

var xtrn_cnf = cnflib.read(system.ctrl_dir + "xtrn.cnf");
if (!xtrn_cnf) {
	throw("Failed to read " + system.ctrl_dir + "xtrn.cnf");
}

if (!file_exists(argv[1])) {
	throw("File not found: " + argv[1]);
}

var file = new File(argv[1]);
if(!file.open('r',false))
	throw("error opening file: " + file.name);

var lines = file.readAll();
file.close();

switch(argv[0]) {
	case 'xtrn-secs':
		for (var i in lines) {
			// index uniquenum code name ars canaccess
			var data = lines[i].split('\t');
			
			var section_updated = false;
			for (var j in xtrn_cnf['xtrnsec']) {
				if (xtrn_cnf['xtrnsec'][j]['code'].toUpperCase() === data[2].toUpperCase()) {
					writeln("Updating: " + data[2] + ": " + data[3]);
					xtrn_cnf['xtrnsec'][j]['name'] = data[3];
					xtrn_cnf['xtrnsec'][j]['ars'] = data[4];
					section_updated = true;
				}
			}
			if (!section_updated) {
				writeln("Adding: " + data[2] + ": " + data[3]);
				xtrn_cnf['xtrnsec'].push({
					code: data[2].toUpperCase(),
					name: data[3],
					ars: data[4]
				});
			}
		}
		if (!cnflib.write(system.ctrl_dir + "xtrn.cnf", undefined, xtrn_cnf)) {
			throw("Failed to write " + system.ctrl_dir + "xtrn.cnf");
		} else {
			writeln("External sections saved.");
		}
		break;
		
	case 'xtrn-progs':
		var secs = {};
		for (var j in xtrn_cnf['xtrnsec']) {
			secs[xtrn_cnf['xtrnsec'][j]['code'].toUpperCase()] = j;
		}
		
		for (var i in lines) {
			// 0 index 1 number 2 sec_index 3 sec_number 4 sec_code 5 code
			// 6 name 7 cmd 8 clean_cmd 9 startup_dir 10 ars 11 execution_ars
			// 12 settings 13 type 14 event 15 textra 16 max_time 17 cost
			// 18 can_access 19 can_run
			var data = lines[i].split('\t');
			
			var prog_updated = false;
			
			if (typeof secs[data[4].toUpperCase()] === "undefined") {
				writeln("Cannot import because no section found: " + data[5] + "(" + data[6] + ")");
				continue;
			}
			
			for (var j in xtrn_cnf['xtrn']) {
				if (xtrn_cnf['xtrn'][j]['code'].toUpperCase() === data[5].toUpperCase()) {
					writeln("Updating: " + data[5] + ": " + data[6]);
					xtrn_cnf['xtrn'][j]['sec_code'] = secs[data[4].toUpperCase()];
					xtrn_cnf['xtrn'][j]['name'] = data[6];
					xtrn_cnf['xtrn'][j]['cmd'] = data[7];
					xtrn_cnf['xtrn'][j]['clean_cmd'] = data[8];
					xtrn_cnf['xtrn'][j]['startup_dir'] = data[9];
					xtrn_cnf['xtrn'][j]['ars'] = data[10];
					xtrn_cnf['xtrn'][j]['execution_ars'] = data[11];
					xtrn_cnf['xtrn'][j]['settings'] = data[12];
					xtrn_cnf['xtrn'][j]['type'] = data[13];
					xtrn_cnf['xtrn'][j]['event'] = data[14];
					xtrn_cnf['xtrn'][j]['textra'] = data[15];
					xtrn_cnf['xtrn'][j]['max_time'] = data[16];
					xtrn_cnf['xtrn'][j]['cost'] = data[17];
					prog_updated = true;
				}
			}
				
			if (!prog_updated) {
				writeln("Adding: " + data[5] + ": " + data[6]);
				xtrn_cnf['xtrn'].push({
					sec: secs[data[4].toUpperCase()],
					code: data[5],
					name: data[6],
					cmd: data[7],
					clean_cmd: data[8],
					startup_dir: data[9],
					ars: data[10],
					execution_ars: data[11],
					settings: data[12],
					type: data[13],
					event: data[14],
					textra: data[15],
					max_time: data[16],
					cost: data[17]
				});
			}
		}
		if (!cnflib.write(system.ctrl_dir + "xtrn.cnf", undefined, xtrn_cnf)) {
			throw("Failed to write " + system.ctrl_dir + "xtrn.cnf");
		} else {
			writeln("External programs saved.");
		}
		
		break;
	
	case 'xtrn-events':
		for (var i in lines) {
			// 0 cmd 1 startup_dir 2 node_num 3 time 
			// 4 freq 5 days 6 mdays 7 months 8 last_run 
			// 9 next_run 10 error_level 11 settings 12 code
			var data = lines[i].split('\t');
			
			var event_updated = false;

			for (var j in xtrn_cnf['event']) {
				if (xtrn_cnf['event'][j]['code'].toUpperCase() === data[12].toUpperCase()) {
					writeln("Updating: " + data[12] + ": " + data[0]);
					xtrn_cnf['event'][j]['cmd'] = data[0];
					xtrn_cnf['event'][j]['days'] = data[5];
					xtrn_cnf['event'][j]['time'] = data[3];
					xtrn_cnf['event'][j]['node_num'] = data[2];
					xtrn_cnf['event'][j]['settings'] = data[11];
					xtrn_cnf['event'][j]['startup_dir'] = data[1];
					xtrn_cnf['event'][j]['freq'] = data[4];
					xtrn_cnf['event'][j]['mdays'] = data[6];
					xtrn_cnf['event'][j]['months'] = data[7];
					event_updated = true;
				}
			}
				
			if (!event_updated) {
				writeln("Adding: " + data[12] + ": " + data[0]);
				xtrn_cnf['event'].push({
					code: data[12].toUpperCase(),
					cmd: data[0],
					days: data[5],
					time: data[3],
					node_num: data[2],
					settings: data[11],
					startup_dir: data[1],
					freq: data[4],
					mdays: data[6],
					months: data[7]
				});
			}
			
			if (!cnflib.write(system.ctrl_dir + "xtrn.cnf", undefined, xtrn_cnf)) {
				throw("Failed to write " + system.ctrl_dir + "xtrn.cnf");
			} else {
				writeln("External events saved.");
			}
		}
		break;
		
	case 'xtrn-editors':
		for (var i in lines) {
			// 0 name 1 cmd 2 ars 3 settings 4 type 5 code
			var data = lines[i].split('\t');
			
			var editor_updated = false;
			
			for (var j in xtrn_cnf['xedit']) {
				if (xtrn_cnf['xedit'][j]['code'].toUpperCase() === data[5].toUpperCase()) {
					writeln("Updating: " + data[5] + ": " + data[0]);
					xtrn_cnf['xedit'][j]['name'] = data[0];
					xtrn_cnf['xedit'][j]['rcmd'] = data[1];
					xtrn_cnf['xedit'][j]['ars'] = data[2];
					xtrn_cnf['xedit'][j]['settings'] = data[3];
					xtrn_cnf['xedit'][j]['type'] = data[4];
					event_updated = true;
				}
			}
			
			if (!editor_updated) {
				writeln("Adding: " + data[5] + ": " + data[0]);
				xtrn_cnf['xedit'].push({
					code: data[5].toUpperCase(),
					rcmd: data[1],
					ars: data[2],
					settings: data[3],
					type: data[4]
				});
			}
			
			if (!cnflib.write(system.ctrl_dir + "xtrn.cnf", undefined, xtrn_cnf)) {
				throw("Failed to write " + system.ctrl_dir + "xtrn.cnf");
			} else {
				writeln("External editors saved.");
			}
		}
		
		writeln(JSON.stringify(xtrn_cnf["xedit"]));
		break;
		
	default:
		throw("Unknown type");
}
