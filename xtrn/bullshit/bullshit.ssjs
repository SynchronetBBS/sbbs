// Bullshit lister function for use with web-v4

load("sbbsdefs.js");

// Deuce's URL-ifier
function linkify(body) {
	urlRE = /(?:https?|ftp|telnet|ssh|gopher|rlogin|news):\/\/[^\s'"'<>()]*|[-\w.+]+@(?:[-\w]+\.)+[\w]{2,6}/gi;
	body = body.replace(
		urlRE, function (str) {
			var ret = '';
			var p = 0;
			var link = str.replace(/\.*$/, '');
			var linktext = link;
			if (link.indexOf('://') == -1) link = 'mailto:' + link;
			return format(
				'<a class="ulLink" href="%s">%s</a>%s',
				link, linktext, str.substr(linktext.length)
			);
		}
	);
	return body;
}

function bullshit(sub, max) {

	var mb = new MsgBase(sub);
	if (!mb.open()) return;
	var shown = 0;
	writeln('<div><ul class="list-group">');
	for (var m = mb.last_msg; (m >= mb.first_msg && shown < max); m = m - 1) {
		var header = mb.get_msg_header(m);
		if (header !== null && !(header.attr&MSG_DELETE)) {
			writeln(format(
				'<li class="list-group-item striped">' +
				'<strong>%s</strong><br><em>%s</em>' +
				'<div class="message">%s</div></li>',
				header.subject, system.timestr(header.when_written_time),
				linkify(mb.get_msg_body(m)).replace(/\r?\n/g, '<br>')
			));
			shown++;
		}
	}
	mb.close();
	writeln('</ul></div>');
}
