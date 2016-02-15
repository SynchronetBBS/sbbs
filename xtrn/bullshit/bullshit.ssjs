// Bullshit lister function for use with web-v4

load("sbbsdefs.js");

// Deuce's URL-ifier
function linkify(body) {
	urlRE=/(?:https?|ftp|telnet|ssh|gopher|rlogin|news):\/\/[^\s'"'<>()]*|[-\w.+]+@(?:[-\w]+\.)+[\w]{2,6}/gi;
	body=body.replace(
		urlRE, 
		function (str) {
			var ret = ''
			var p = 0;
			var link = str.replace(/\.*$/, '');
			var linktext = link;
			if (link.indexOf('://') == -1) link = 'mailto:' + link;
			return (
				'<a class="ulLink" href="' + link + '">' + linktext + '</a>' +
				str.substr(linktext.length)
			);
		}
	);
	return body;
}

function bullshit(sub, maxMessages) {

	writeln('<div><ul class="list-group">');
	var msgBase = new MsgBase(sub);
	if (!msgBase.open()) return;
	var shown = 0;
	for (	var m = msgBase.last_msg;
			m > msgBase.first_msg, shown < maxMessages;
			m = m - 1
	) {
		var header = msgBase.get_msg_header(m);
		if (header === null || header.attr&MSG_DELETE) continue;
		writeln(
			'<li class="list-group-item striped">' +
			'<strong>' + header.subject + '</strong><br>' +
			'<em>' + system.timestr(header.when_written_time) + '</em>' +
			'<div class="message">' +
			linkify(msgBase.get_msg_body(m)).replace(/\r?\n/g, "<br>") +
			'</div>' +
			'</li>'
		);
		shown++;
	}
	msgBase.close();
	writeln('</ul></div>');
}