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

var file_list = filebase.get_file_list();
for(var i = 0; i < file_list.length; i++) {
	var file = file_list[i];
	print(JSON.stringify(file, null, 4));
	var hash = filebase.hash_file(file.name);
	if(hash == null) {
		alert("hash is null");
		break;
	}
	file.size = hash.size;
	file.crc16 = hash.crc16;
	file.crc32 = hash.crc32;
	file.md5 = hash.md5;
	if(!filebase.update_file(file.name, file)) {
		alert(filebase.status + " " + filebase.last_error);
		break;
	}
}
