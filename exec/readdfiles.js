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
	print("Updating " + file.name);
	fbase.update(file.name, file, /* use_diz_always: */true, /* re-add: */true);
}
