load("http.js");

var f = new File(system.ctrl_dir + "modopts.ini");
f.open("r");
if(!f.is_open) {
	var http = false;
} else {
	var bbsScene = f.iniGetObject("bbs-scene.org");
	f.close();
	var http = new HTTPRequest(bbsScene.username, bbsScene.password);
}