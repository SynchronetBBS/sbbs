//Forum
// Web-forum page for ecWeb v3
// echicken -at- bbs.electronicchicken.com

load('webInit.ssjs');
load("../web/lib/forum.ssjs");

print("<span class='title'>Forum</span><br /><br />");

printBoards();

if(http_request.query.hasOwnProperty('board')) {
	print("<script type='text/javascript'>");
	print("toggleVisibility('group-" + http_request.query.board + "');");
	print("</script>");
}

if(http_request.query.hasOwnProperty('sub') && http_request.query.sub != 'mail') {
	print("<script type='text/javascript'>");
	print("loadThreads('http://" + system.inet_addr + ":" + webIni.HTTPPort + "/forum-async.ssjs', '" + http_request.query.sub + "', " + ((http_request.query.hasOwnProperty('thread'))?false:true) + ");");
	if(http_request.query.hasOwnProperty('thread'))
		print("loadThread('http://" + system.inet_addr + ":" + webIni.HTTPPort + "/forum-async.ssjs', '" + http_request.query.sub + "', '" + http_request.query.thread + "', " + false + ");");
	print("</script>");
}