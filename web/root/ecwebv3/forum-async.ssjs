// Asynchronous forum functions for ecWeb v3
// echicken -at- bbs.electronicchicken.com

load('webInit.ssjs');
load("../web/lib/forum.ssjs");

if(http_request.query.hasOwnProperty('postmessage')) {
	var x = postMessage(
		http_request.query.sub,
		http_request.query.irt,
		http_request.query.to,
		http_request.query.from,
		http_request.query.subject,
		http_request.query.body
	);
	if(!x)
		print("An error was encountered.  Your message was not posted.");
} else if(!http_request.query.hasOwnProperty('sub')) {
	printBoards();
} else {
	if(http_request.query.hasOwnProperty('getmessage'))
		printMessage(http_request.query.sub, parseInt(http_request.query.getmessage));
	else if(!http_request.query.hasOwnProperty('thread'))
		printThreads(http_request.query.sub);
	else if(http_request.query.hasOwnProperty('sub') && http_request.query.hasOwnProperty('thread'))
		printThread(http_request.query.sub, parseInt(http_request.query.thread));
}