"use strict";

if(argv.indexOf("-help") >= 0 || argv.indexOf("-?") >= 0) {
	print("usage: [dir-code] [file-name]");
	exit(0);
}

var code = argv[0];

while(!file_area.dir[code] && !js.terminated) {
	for(var d in file_area.dir)
		print(d);
	code = prompt("Directory code");
}

var dir = file_area.dir[code];

var filebase = new FileBase(code);
if(!filebase.open()) {
	alert("Failed to open: " + filebase.file);
	exit(1);
}

print(JSON.stringify(filebase.hash_file(argv[1]), null, 4));

print(filebase.last_error);
/*

var name_list = filebase.get_file_names();

*/