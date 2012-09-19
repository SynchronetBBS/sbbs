// bbs-scene.org global onelinerz for ecweb
// echicken -at- bbs.electronicchicken.com

var scriptname = "onelinerz.ssjs";
if(http_request.query.hasOwnProperty('action') && http_request.query.action == 'showOnelinerz') {
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
			print("<div style='padding:5px;background-color:#000060;color:#FFFFFF;word-wrap:break-word;'>");
		else
			print("<div style='padding:5px;background-color:#000080;color:#FFFFFF;word-wrap:break-word;'>");
		print("<b>" + onelinerz[o].alias + "@" + onelinerz[o].bbsname + "</b>:<br>" + onelinerz[o].oneliner);
		print("</div>");
		n++;
	}
} else {
	print("<div id='bbssceneonelinerz'></div>");
	print("<script type='text/javascript'>");
	print("function xhrbbssceneonelinerz() {");
	print("var XMLReq = new XMLHttpRequest();");
	print("XMLReq.open('GET', './sidebar/" + scriptname + "?action=showOnelinerz', true);");
	print("XMLReq.send(null);");
	print("XMLReq.onreadystatechange = function() { if(XMLReq.readyState == 4) { document.getElementById('bbssceneonelinerz').innerHTML = XMLReq.responseText; } }");
	print("}");
	print("xhrbbssceneonelinerz();");
	print("</script>");
}