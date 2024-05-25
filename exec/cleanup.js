// Removes obsolete files

"use strict";

const quiet = argv.indexOf("-q") >= 0;
const verbose = argv.indexOf("-v") >= 0;

function cont(str)
{
	if(quiet) return true;

	return confirm(str || "Continue");
}

alert("This script deletes files.");
print();
print("The cleanup/removal of these files is completely optional.");
print();
alert("You should have a back-up of all files before continuing.");
print();
print("You should run the current verison of the Synchronet Terminal Server");
print("for a reasonable period of time before continuing.");
if(!cont())
	exit(0);

const file_list = [
	{ dir: system.data_dir + backslash("user"), file: "user.dat", desc: "User database (migrated to user.tab)" },
	{ dir: system.data_dir + backslash("user/ptrs"), file: "*.ixb", desc: "User message scan config/pointers (migrated to *.subs)" },
	{ dir: system.ctrl_dir, file: "*.cnf", desc: "System configuration settings (migrated to *.ini)" },
	{ dir: system.ctrl_dir, file: "sbbsecho.cfg", desc: "SBBSecho v2 configuration settings (migrated to sbbsecho.ini)" },
	{ dir: system.ctrl_dir, file: "dsts.dab", desc: "Daily system statistics (migrated to dsts.ini)" },
	{ dir: system.ctrl_dir, file: "csts.dab", desc: "Cumulative system statistics (migrated to csts.tab)" },
	{ dir: system.ctrl_dir, file: "pnet.dab", desc: "PostLink network call-out times (unused)" },
	{ dir: system.ctrl_dir, file: "qnet.dab", desc: "QWK network call-out times (migrated to time.ini)" },
	{ dir: system.ctrl_dir, file: "time.dab", desc: "Timed-event execution times (migrated to time.ini)" },
];

var data_dir = {};
for(var i in file_area.dir)
	data_dir[file_area.dir[i].data_dir] = true;

for(var i in data_dir) {
	file_list.push( { dir: i, file: "*.ixb", desc: "Filebase indexes (migrated to *.sid)" } );
	file_list.push( { dir: i, file: "*.dat", desc: "Filebase data (migrated to *.shd)" } );
	file_list.push( { dir: i, file: "*.exb", desc: "Filebase extended descriptions (migrated to *.sdt)" } );
	file_list.push( { dir: i, file: "*.dab", desc: "Filebase metadata (migrated to *.ini)" } );
}

for(var i in system.node_list) {
	file_list.push( { dir: system.node_list[i].dir, file: "node.cnf", desc: "Node configuration settings (migrated to node.ini" } );
	file_list.push( { dir: system.node_list[i].dir, file: "dsts.dab", desc: "Daily system statistics (migrated to dsts.ini)" } );
	file_list.push( { dir: system.node_list[i].dir, file: "csts.dab", desc: "Cumulative system statistics (migrated to csts.tab)" } );
}

for(var i in file_list) {
	var item = file_list[i];
	var path = item.dir + item.file;
	var count = directory(path).length;
	if(count < 1) {
		if(verbose) print(path + format(" (%s)", item.desc) + " does not exist");
		continue;
	}
	var multi = count > 1 ? format(" (%u files)", count) : "";
	if(!cont("Remove " + path + multi + " - " + item.desc))
		continue;
	if(rmfiles(item.dir, item.file))
		print(path + " removed successfully");
}
