// Forum

//load('webInit.ssjs');
//load(webIni.webRoot + '/themes/' + webIni.theme + '/layout.ssjs');
load(webIni.webRoot + '/lib/forumLib.ssjs');

//openPage("Message Forum");

if(!http_request.query.hasOwnProperty('action')) {
	print('<span class=titleFont>Message Forum</span><br><br>');
	printBoards();
} else if(http_request.query.action.toString() == 'viewSubBoard' && http_request.query.hasOwnProperty('subBoard')) {
	print('<span class=titleFont>Message Forum: Sub-Board View</span><br>');
	printSubBoard(http_request.query.subBoard.toString(), 0, false, 0);
} else if(http_request.query.action.toString() == 'newMessageScan' && user.alias != webIni.guestUser) {
	print('<span class=titleFont>Message Forum: New Message Scan</span><br>');
	newMessageScan();
}

//closePage();
