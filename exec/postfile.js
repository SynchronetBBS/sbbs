"use strict";

if(argv.indexOf("-help") >= 0 || argv.indexOf("-?") >= 0) {
	print("usage: [dir-code] [file-name] [file-description] [uploader-name]");
	exit(0);
}

var code = argv[0];
var file = { name: argv[1], desc: argv[2], from: argv[3] };

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

var name_list = filebase.get_names();

while(!file_exists(dir.path + file.name) && !js.terminated) {
	if(file.name)
		alert(dir.path + file.name + " does not exist");
	var list = directory(dir.path + '*');
	for(var i = 0; i < list.length; i++) {
		if(!file_isdir(list[i]) && name_list.indexOf(file_getname(list[i])) < 0)
			print(file_getname(list[i]));
	}
	file.name = prompt("File name");
}

if(filebase.get(file.name)) {
	alert("File '" + file.name + "' already added.");
	exit(1);
}

while(!file.desc && !js.terminated) {
	file.desc = prompt("Description");
}

while(!file.from && !js.terminated) {
	file.from = prompt("Uploader");
}

file.cost = file_size(dir.path + file.name);
print("Adding " + file.name + " to " + filebase.file);
if(filebase.add(file))
	print(format("File (%s) added successfully to: ", file.name) + code);
else
	alert("Error " + filebase.last_error + " adding file to: " + code);
filebase.close();

