//Forum
// Web-forum page for ecWeb v3
// echicken -at- bbs.electronicchicken.com

load('webInit.ssjs');
load("../web/lib/forum.ssjs");

print("<span class='title'>Forum</span><br /><br />");

printBoards();

if(typeof http_request.query.board != "undefined") {
	print("<script type='text/javascript'>");
	print("toggleVisibility('group-" + http_request.query.board + "');");
	print("</script>");
}

if(typeof http_request.query.sub != "undefined" && http_request.query.sub != 'mail') {
	print("<script type='text/javascript'>");
	print(
		"loadThreads('http://"
		+ http_request.host
		+ ":"
		+ webIni.HTTPPort
		+ "/forum-async.ssjs', '"
		+ http_request.query.sub
		+ "', "
		+ ((http_request.query.hasOwnProperty('thread'))?false:true)
		+ ");"
	);
	if(typeof http_request.query.thread != "undefined") {
		print("loadThread('http://"
			+ http_request.host
			+ ":"
			+ webIni.HTTPPort
			+ "/forum-async.ssjs', '"
			+ http_request.query.sub
			+ "', '"
			+ http_request.query.thread
			+ "', "
			+ false
			+ ");"
		);
	}
	print("</script>");
}
