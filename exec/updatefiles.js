if(argc < 1) {
	alert("No directory code specfiied");
	exit(0);
}
var fbase = new FileBase(argv[0]);
if(!fbase.open()) {
	alert("failed to open base");
	exit(1);
}
var file_list = fbase.get_list(argv[1] || "*", FileBase.DETAIL.NORM);
for(var i in file_list) {
	var file = file_list[i];
	var copy = JSON.parse(JSON.stringify(file));
	for(var p in file) {
		var str = prompt(p + " [" + file[p] + "]");
		if(str)
			file[p] = str;
	}
	if(JSON.stringify(copy) != JSON.stringify(file)) {
		alert("changed");
		print(fbase.update(copy.name, file));
		print(fbase.status);
		print(fbase.last_error);
	}
}
