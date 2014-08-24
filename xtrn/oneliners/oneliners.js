load("sbbsdefs.js");
load(js.exec_dir + "lib.js");

var settings = initSettings(js.exec_dir);

if(console.autoterm&USER_ANSI)
	load(js.exec_dir + "framed.js");
else
	load(js.exec_dir + "plain.js");