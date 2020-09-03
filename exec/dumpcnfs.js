// $Id: dumpcnfs.js,v 1.1 2018/03/11 00:43:01 rswindell Exp $

// Example use of cnflib.js
// Changes to the objects may be written back to the files using cnf.write()
// e.g.: cnflib.write("xtrn.cnf", undefined, xtrn_cnf);

var cnflib = load({}, 'cnflib.js');

print("msgs_cnf");
var msgs_cnf = cnflib.read("msgs.cnf");
if(msgs_cnf)
	print(JSON.stringify(msgs_cnf, null, 4));

print("xtrn_cnf");
var xtrn_cnf = cnflib.read(system.ctrl_dir + "xtrn.cnf");
if(xtrn_cnf)
	print(JSON.stringify(xtrn_cnf, null, 4));

print("file_cnf");
var file_cnf = cnflib.read(system.ctrl_dir + "file.cnf");
if(file_cnf)
	print(JSON.stringify(file_cnf, null, 4));
