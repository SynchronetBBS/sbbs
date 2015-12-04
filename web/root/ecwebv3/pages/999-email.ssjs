//HIDDEN:eMail
// Private email page for ecwebv3
// echicken -at- bbs.electronicchicken.com

load('webInit.ssjs');
load(webIni.WebDirectory+"/lib/forum.ssjs");

if(user.alias != webIni.WebGuest) {

	print("<span class='title'>Email</span><br /><br />");
	print("<div class='border box msg'>");
	print(
		format(
			"<a class='ulLink' onclick='addPost(\"http://%s:%s%s/forum-async.ssjs\", \"%s\", \"%s\", \"%s\", \"%s\")'>Compose a new email</a>",
			http_request.host,
			webIni.HTTPPort,
			webIni.appendURL,
			"mail",
			user.alias,
			user.name,
			""
		)
	);
	print("<div id='sub-mail-newMsgBox'></div>");
	print("</div>");
	print("<div id='sub-mail' style='display:none;'></div>");
	print("<div id='sub-mail-info' class='border box msg' style='display:none;'></div>");
	print("<script type='text/javascript'>");
	print(
		"loadThreads('http://"
		+ http_request.host
//		+ ":"
//		+ webIni.HTTPPort
		+ webIni.appendURL
		+ "/forum-async.ssjs', 'mail', "
		+ ((http_request.query.hasOwnProperty('thread'))?false:true) + ");"
	);
	if(typeof http_request.query.thread != "undefined") {
		print(
			"loadThread('http://"
			+ http_request.host
//			+ ":"
//			+ webIni.HTTPPort
			+ webIni.appendURL
			+ "/forum-async.ssjs', 'mail', '"
			+ http_request.query.thread
			+ "', "
			+ false
			+ ");"
		);
	}
	print("</script>");

}
