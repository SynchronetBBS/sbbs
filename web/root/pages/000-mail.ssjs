//HIDDEN:Mail

if(typeof argv[0] != "boolean" || !argv[0])
	exit();

if(user.number == 0 || user.alias == settings.guest)
	exit();

load("sbbsdefs.js");
load(system.exec_dir + "../web/lib/init.js");
load(settings.web_lib + "forum.js");

writeln('<script type="text/javascript" src="./js/forum.js"></script>');

if(typeof http_request.query.notice != "undefined") {
	writeln(
		'<div id="noticebox" class="alert alert-warning">' + 
		http_request.query.notice[0] + '</div>' +
		'<script type="text/javascript">' +
		'$("#noticebox").fadeOut(3000,function(){$("#noticebox").remove();});' +
		'</script>'
	);
}

if(	user.alias != settings.guest
	&&
	!(user.security.restrictions&UFLAG_E)
	&&
	!(user.security.restrictions&UFLAG_M)
) {
	writeln(
		'<button class="btn btn-default icon" ' +
		'aria-label="Post a new message" title="Post a new message" ' +
		'onclick="addNew(\'mail\')">' +
		'<span class="glyphicon glyphicon-pencil"></span>' +
		'</button>'
	);
}


writeln(
	format(
		'<ul class="nav nav-tabs">' +
		'<li role="presentation" class="%s">' +
		'<a href="./?page=%s&amp;sent=0">Inbox</a>' +
		'</li>' +
		'<li role="presentation" class="%s">' +
		'<a href="./?page=%s&amp;sent=1">Sent</a>' +
		'</li>' +
		'</ul><br>',
		(typeof http_request.query.sent == "undefined" || http_request.query.sent[0] == "0" ? "active" : ""),
		http_request.query.page[0],
		(typeof http_request.query.sent != "undefined" && http_request.query.sent[0] == "1" ? "active" : ""),
		http_request.query.page[0]
	)
);

writeln('<ul id="forum-list-container" class="list-group">');

var writeMessage = function(header) {
	writeln(
		format(
			'<li id="li-' + header.number + '" class="list-group-item mail striped %s">',
			(header.attr&MSG_READ ? "read" : "unread"),
			header.number
		)
	);
	writeln(
		format(
			'<div style="cursor:pointer;" onclick="getMailBody(%s)">' +
			'%s: <strong>%s</strong> on %s' +
			'<p>Subject: <strong>%s</strong></p></div>' +
			'<div class="message" id="message-%s" hidden></div>',
			header.number,
			(typeof http_request.query.sent == "undefined" || http_request.query.sent[0] == "0" ? "From" : "To"),
			(typeof http_request.query.sent == "undefined" || http_request.query.sent[0] == "0" ? header.from : header.to),
			(new Date(header.when_written_time * 1000)).toLocaleString(),
			header.subject,
			header.number
		)
	);
	writeln('</li>');
}

getMailHeaders(
	(typeof http_request.query.sent == "undefined" || http_request.query.sent[0] == "0" ? false : true)
).forEach(writeMessage);

writeln('</ul>');