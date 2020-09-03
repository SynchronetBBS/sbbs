load("graphic.js");

if(argc < 1) {
	alert("usage: " + js.exec_file + " <filename> <columns> <rows>");
	exit();
}

var fname = argv[0];
graphic = new Graphic(argv[1],argv[2]);
graphic.cpm_eof = false;
if(!graphic.load(fname))
	alert("Load failure");
else {
	console.clear();
	graphic.draw();
}
