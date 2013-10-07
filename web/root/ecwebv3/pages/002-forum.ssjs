//Forum
// Web-forum page for ecWeb v3
// echicken -at- bbs.electronicchicken.com

load('webInit.ssjs');
load(webIni.WebDirectory + "/lib/forum.ssjs");

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
		format(
			"loadThreads('http://%s%s/forum-async.ssjs', '%s', %s);",
			http_request.host,
			webIni.appendURL,
			http_request.query.sub,
			(typeof http_request.query.thread == "undefined") ? true : false
		)
	);
	if(typeof http_request.query.thread != "undefined") {
		print(
			format(
				"loadThread('http://%s%s/forum-async.ssjs', '%s', '%s', %s);",
				http_request.host,
				webIni.appendURL,
				http_request.query.sub,
				http_request.query.thread,
				false
			)
		);
	}
	print("</script>");
}
