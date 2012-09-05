load("http.js");

var f = new File(system.ctrl_dir + "modopts.ini");
f.open("r");
if(!f.is_open)
	exit;
var bbsScene = f.iniGetObject("bbs-scene.org");
f.close();

var http = new HTTPRequest(bbsScene.username, bbsScene.password);

var response = http.Get("http://bbs-scene.org/api/onelinerzjson.php?limit=10&ansi=0");
try {
	if(response === undefined || response === null)
		throw("Empty response");
}
catch(err) {
	if(err)
		exit;
}
var onelinerz = JSON.parse(response);
print("<b>BBS-Scene.org Global Onelinerz</b><br><br>");
var n = 0;
for(var o in onelinerz) {
	if(n % 2 == 0)
		print("<div style='padding:5px;background-color:#585858;color:#FFFFFF;'>");
	else
		print("<div style='padding:5px;background-color:#2E2E2E;color:#FFFFFF;'>");
	print("<b>" + onelinerz[o].alias + "@" + onelinerz[o].bbsname + "</b>:<br>" + onelinerz[o].oneliner);
	print("</div>");
	n++;
}