// Asynchronous forum functions for ecWeb v3
// echicken -at- bbs.electronicchicken.com

load('webInit.ssjs');
load(webIni.WebDirectory + "/lib/forum.ssjs");

if(typeof http_request.query.postmessage != "undefined") {
	var x = postMessage(
		http_request.query.sub,
		http_request.query.irt,
		http_request.query.to,
		http_request.query.from,
		http_request.query.subject,
		lfexpand(http_request.query.body)
	);
	if(!x)
		print("An error was encountered.  Your message was not posted.");
} else if(typeof http_request.query.sub == "undefined") {
	printBoards();
} else {
	if(typeof http_request.query.getmessage != "undefined")
		printMessage(http_request.query.sub, http_request.query.getmessage);
	else if(typeof http_request.query.deletemessage != "undefined")
		deleteMessage(http_request.query.sub, http_request.query.deletemessage);
	else if(typeof http_request.query.thread == "undefined")
		printThreads(http_request.query.sub);
	else if(typeof http_request.query.sub != "undefined" && typeof http_request.query.thread != "undefined")
		printThread(http_request.query.sub, parseInt(http_request.query.thread));
}
