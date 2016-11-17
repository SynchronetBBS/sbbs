//Forum
if (typeof argv[0] !== 'boolean' || !argv[0]) exit();

load('sbbsdefs.js');
load(system.exec_dir + '../web/lib/init.js');
load(settings.web_lib + 'forum.js');

writeln('<script type="text/javascript" src="./js/forum.js"></script>');

if (typeof http_request.query.notice !== 'undefined') {
	writeln(
		'<div id="noticebox" class="alert alert-warning">' + 
		http_request.query.notice[0] + '</div>' +
		'<script type="text/javascript">' +
		'$("#noticebox").fadeOut(3000,function(){$("#noticebox").remove();});' +
		'</script>'
	);
}


if (typeof http_request.query.sub !== 'undefined' &&
	typeof msg_area.sub[http_request.query.sub[0]] !== 'undefined' &&
	typeof http_request.query.thread !== 'undefined' &&
	msg_area.sub[http_request.query.sub[0]].can_read
) {

	// Thread view
	var msgBase = new MsgBase(http_request.query.sub[0]);

	var firstUnread = '';

	function writeMessage(thread, keys, key, index) {

		var header = thread.messages[key];

		var body = msgBase.get_msg_body(header.number);
		if (body === null) return;

		writeln(
			'<a id="' + header.number + '"></a>' +
			'<li class="list-group-item striped' +
			'" id="li-' + header.number + '">'
		);

		// Show subject if first message
		if (index === 0) {
			writeln('<h4><strong>' + thread.subject + '</strong></h4>');
		}

		// Header
		writeln('<div class="row">');

		writeln('<div class="col-sm-8">');
		if (user.alias != settings.guest &&
			header.number > msg_area.sub[http_request.query.sub[0]].scan_ptr
		) {
			writeln(
				'<span title="Unread" class="glyphicon glyphicon-star"></span>'
			);
			if (firstUnread === '') firstUnread += header.number;
		}			
		writeln(
			'From <strong>' + header.from + "</strong>" +
			(	typeof header.from_net_addr === 'undefined'
				? ''
				: ('@' +  header.from_net_addr)
			) +
			' to <strong>' + header.to + '</strong> on ' +
			(new Date(header.when_written_time * 1000)).toLocaleString()
		);
		writeln('</div>');

		if (typeof settings.vote_buttons === 'undefined' || settings.vote_buttons) {		
			writeln(
				'<div class="col-sm-4">' +
					'<div class="pull-right">' +
					'<button id="uv-' + header.number + '" class="btn-uv btn btn-default icon"' + (user.alias == settings.guest || user.security.restrictions&UFLAG_V ? 'disabled' : '') + '>' +
						'<span title="Upvotes" class="glyphicon glyphicon-arrow-up">' +
						'</span>' +
						'<span id="uv-count-' + header.number + '" title="Upvotes">' +
						header.upvotes + '</span>' +
					'</button>' +
					'<button id="dv-' + header.number + '" class="btn-dv btn btn-default icon"' + (user.alias == settings.guest || user.security.restrictions&UFLAG_V ? 'disabled' : '') + '>' +
						'<span title="Downvotes" class="glyphicon glyphicon-arrow-down">' +
						'</span>' +
						'<span id="dv-count-' + header.number + '" title="Downvotes">' +
						header.downvotes + '</span>' +
					'</button>' +
					'</div>' +
				'</div>'
			);
		}
		
		writeln('</div>'); // message header

		// Body
		writeln(
			'<div class="message" id="message-' + header.number + '">' +
			formatMessage(body) + '</div>'
		);

		var prev = (
			index === 0
			? thread.messages[thread.__first].number
			: thread.messages[keys[index - 1]].number
		);

		var next = (
			key === thread.__last
			? thread.messages[thread.__last].number
			: thread.messages[keys[index + 1]].number
		);

		// Standard controls
		writeln(
			'<a class="btn btn-default icon" title="Jump to oldest message" ' +
			'aria-label="Jump to oldest message" href="#' +
			thread.messages[thread.__first].number + '">' +
			'<span class="glyphicon glyphicon-fast-backward"></span></a>' +

			'<a class="btn btn-default icon" title="Jump to previous message" '+
			'aria-label="Jump to previous message" href="#' + prev + '" ' +
			'id="pm-' + header.number + '">' +
			'<span class="glyphicon glyphicon-step-backward"></span></a>' +

			'<a class="btn btn-default icon" ' +
			'title="Direct link to this message" ' +
			'aria-label="Direct link to this message" ' +
			'href="#' + header.number + '">' +
			'<span class="glyphicon glyphicon-link"></span></a>' +

			'<a class="btn btn-default icon" title="Jump to next message" ' +
			'aria-label="Jump to next message" href="#' + next + '" ' +
			'id="nm-' + header.number + '">' +
			'<span class="glyphicon glyphicon-step-forward"></span></a>' +
			
			'<a class="btn btn-default icon" ' +
			'aria-label="Jump to newest message" ' +
			'title="Jump to newest message" href="#' + 
			thread.messages[thread.__last].number + 
			'"><span class="glyphicon glyphicon-fast-forward"></span></a>'
		);

		// User can post
		if (user.alias !== settings.guest &&
			msg_area.sub[msgBase.cfg.code].can_post
		) {
			writeln(
				'<button class="btn btn-default icon" ' +
				'aria-label="Reply to this message" ' +
				'title="Reply to this message" ' +
				'name="reply-' + header.number + '" ' +
				'onclick="addReply(\'' +
					msgBase.cfg.code + '\',' + header.number +
				')">' +
				'<span class="glyphicon glyphicon-comment"></span>' +
				'</button>'
			);
		}

		// User is operator
		if (user.alias != settings.guest &&
			msg_area.sub[msgBase.cfg.code].is_operator
		) {
			writeln(
				'<button class="btn btn-default icon" ' +
				'aria-label="Delete this message" title="Delete this message" '+
				'onclick="deleteMessage(\'' +
					msgBase.cfg.code + '\', ' + header.number +
				')" ' +
				'href="#">' +
				'<span class="glyphicon glyphicon-trash"></span>' +
				'</button>'
			);
		}

		writeln('</li>');

	}

	writeln(
		format(
			'<ol class="breadcrumb">' +
			'<li><a href="./?page=%s">Forum</a></li>' +
			'<li><a href="./?page=%s&amp;group=%s">%s</a></li>' +
			'<li><a href="./?page=%s&amp;sub=%s">%s</a></li>' +
			'</ol>',
			http_request.query.page[0],
			http_request.query.page[0],
			msg_area.sub[http_request.query.sub[0]].grp_index,
			msg_area.sub[http_request.query.sub[0]].grp_name,
			http_request.query.page[0],
			http_request.query.sub[0],
			msg_area.sub[http_request.query.sub[0]].name
		)
	);
	writeln(
		'<div id="jump-unread-container" style="margin-bottom:1em;" hidden>' +
		'<a class="btn btn-default" id="jump-unread" ' +
		'title="Jump to first unread message" href="#">' +
		'<span class="glyphicon glyphicon-star"></span>' +
		'</a></div>'
	);

	try {
		msgBase.open();
		var thread = getMessageThreads(
			http_request.query.sub[0],
			settings.max_messages
		).thread[
			http_request.query.thread[0]
		];
		var keys = Object.keys(thread.messages);
		thread.__first = keys[0];
		thread.__last = keys[keys.length - 1];
		writeln('<ul id="forum-list-container" class="list-group">');
		keys.forEach(
			function (key, index) {
				writeMessage(thread, keys, key, index);
			}
		);
		writeln('</ul>');
		msgBase.close();
		if (keys.length > 1 && firstUnread !== '') {
			writeln(
				'<script type="text/javascript">' +
				'$("#jump-unread").attr("href", "#' + firstUnread + '");' +
				'$("#jump-unread-container").attr("hidden", false);' +
				'</script>'
			);
		}
		// Update scan pointer
		if (thread.messages[thread.__last].number >
			msg_area.sub[http_request.query.sub[0]].scan_ptr
		) {
			msg_area.sub[http_request.query.sub[0]].scan_ptr =
			thread.messages[thread.__last].number;
		}
	} catch (err) {
		log(LOG_WARNING, err);
	}

	writeln('<script type="text/javascript">');
	if (settings.keyboard_navigation) {
		writeln('threadNav();');
	}
	if (user.alias != settings.guest || user.security.restrictions&UFLAG_V) {
		writeln('enableVoteButtonHandlers("'+http_request.query.sub[0]+'");');
	}
	writeln('</script>');

} else if (
	typeof http_request.query.sub !== 'undefined' &&
	typeof msg_area.sub[http_request.query.sub[0]] !== 'undefined' &&
	msg_area.sub[http_request.query.sub[0]].can_read
) {

	// Thread list
	function writeThread(thread) {
		if (user.number > 0 && user.alias != settings.guest) {
			var unread = getUnreadInThread(http_request.query.sub[0], thread);
		} else {
			var unread = 0;
		}
		var keys = Object.keys(thread.messages);
		var first = keys[0];
		var last = keys[keys.length - 1];
		writeln(
			format(
				'<a href="./?page=%s&amp;sub=%s&amp;thread=%s" ' +
				'class="list-group-item striped">',
				http_request.query.page[0],
				http_request.query.sub[0],
				thread.id
			)
		);
		writeln('<div class="row"><div class="col-sm-8">');
		writeln(
			format(
				'<strong>%s</strong>' +
				'<p>By <strong>%s</strong> on %s</p>' +
				'<p>Latest reply by <strong>%s</strong> on %s</p>',
				thread.subject,
				thread.messages[first].from,
				(new Date(
						thread.messages[first].when_written_time * 1000
					)
				).toLocaleString(),
				thread.messages[last].from,
				(new Date(
					thread.messages[last].when_written_time * 1000
				)).toLocaleString()
			)
		);
		writeln('</div>');
		if (settings.vote_buttons) {
			writeln(
				'<div class="col-sm-3">' +
				'<div class="pull-right">' +
				'<div title="Upvotes - Parent (Thread Total)" class="btn icon">' +
				'<span class="glyphicon glyphicon-arrow-up">' +
				'</span>' +
				thread.messages[first].upvotes + ' (' + thread.votes.up + ')' +
				'</div>' +
				'<div title="Downvotes - Parent (Thread Total)" class="btn icon">' +
				'<span class="glyphicon glyphicon-arrow-down">' +
				'</span>' +
				thread.messages[first].downvotes + ' (' + thread.votes.down + ')' +
				'</div>' +
				'</div>' +
				'</div>'
			);
		}
		writeln(
			format(
				'<div class="col-sm-1">' +
				'<div class="btn icon">' +
				'<span title="Unread messages" class="badge%s" id="badge-%s">' +
				'%s</span></div></div>',
				(	msg_area.sub[http_request.query.sub[0]].scan_cfg&SCAN_CFG_NEW
					? ' scanned' : ''
				),
				thread.messages[first].number,
				(unread == 0 ? '' : unread)
			)
		);
		writeln('</div></a>');
	}

	writeln(
		format(
			'<ol class="breadcrumb">' +
			'<li><a href="./?page=%s">Forum</a></li>' +
			'<li><a href="./?page=%s&amp;group=%s">%s</a></li>' +
			'<li><a href="./?page=%s&amp;sub=%s">%s</a></li>' +
			'</ol>',
			http_request.query.page[0],
			http_request.query.page[0],
			msg_area.sub[http_request.query.sub[0]].grp_index,
			msg_area.sub[http_request.query.sub[0]].grp_name,
			http_request.query.page[0],
			http_request.query.sub[0],
			msg_area.sub[http_request.query.sub[0]].name
		)
	);

	if (user.alias !== settings.guest &&
		msg_area.sub[http_request.query.sub[0]].can_post
	) {
		writeln(
			'<button class="btn btn-default icon" ' +
			'aria-label="Post a new message" title="Post a new message" ' +
			'onclick="addNew(\'' + http_request.query.sub[0] + '\')">' +
			'<span class="glyphicon glyphicon-pencil"></span>' +
			'</button>'
		);
	}

	if (user.alias !== settings.guest) {
		writeln(
			'<button id="scan-cfg-new" class="btn ' +
			(	!(msg_area.sub[http_request.query.sub[0]].scan_cfg&SCAN_CFG_YONLY) &&
				(msg_area.sub[http_request.query.sub[0]].scan_cfg&SCAN_CFG_NEW)
				? 'btn-primary'
				: 'btn-default'
			) +
			' icon" aria-label="Scan for new messages" ' +
			'title="Scan for new messages" ' +
			'onclick="setScanCfg(\'' + http_request.query.sub[0] + '\',1)"' +
			'>' +
			'<span class="glyphicon glyphicon-ok-sign"></span>' +
			'</button>'
		);
		writeln(
			'<button id="scan-cfg-youonly" class="btn ' +
			(	msg_area.sub[http_request.query.sub[0]].scan_cfg&SCAN_CFG_YONLY
				? 'btn-primary'
				: 'btn-default'
			) +
			' icon" aria-label="Scan for new messages to you only" ' +
			'title="Scan for new messages to you only" ' +
			'onclick="setScanCfg(\'' + http_request.query.sub[0] + '\',2)"' +
			'>' +
			'<span class="glyphicon glyphicon-user"></span>' +
			'</button>'
		);
		writeln(
			'<button id="scan-cfg-off" class="btn ' +
			(	!(msg_area.sub[http_request.query.sub[0]].scan_cfg&SCAN_CFG_NEW) &&
				!(msg_area.sub[http_request.query.sub[0]].scan_cfg&SCAN_CFG_YONLY)
				? 'btn-primary'
				: 'btn-default'
			) +
			' icon" aria-label="Do not scan this sub" ' +
			'title="Do not scan this sub" ' +
			'onclick="setScanCfg(\'' + http_request.query.sub[0] + '\',0)"' +
			'>' +
			'<span class="glyphicon glyphicon-ban-circle"></span>' +
			'</button>'
		);
	}

	try {
		var threads = getMessageThreads(
			http_request.query.sub[0],
			settings.max_messages
		);
		writeln('<div id="forum-list-container" class="list-group">');
		threads.order.forEach(function(t){writeThread(threads.thread[t]);});
		writeln('</div>');
	} catch (err) {
		log(LOG_WARNING, err);
	}

} else if (
	typeof http_request.query.group !== 'undefined' &&
	typeof msg_area.grp_list[http_request.query.group[0]] !== 'undefined'
) {

	// Sub list
	function writeSub(sub) {
		writeln(
			format(
				'<a href="./?page=%s&amp;sub=%s" ' +
				'class="list-group-item striped%s">' +
				'<h4><strong>%s</strong></h4>' +
				'<span title="Unread messages" class="badge %s" id="badge-%s">'+
				'</span>' +
				'<p>%s</p>' +
				'</a>',
				http_request.query.page[0],
				sub.code,
				(	sub.scan_cfg&SCAN_CFG_NEW || sub.scan_cfg&SCAN_CFG_YONLY
					? ' scanned'
					: ''
				),
				sub.name,
				(	sub.scan_cfg&SCAN_CFG_NEW || sub.scan_cfg&SCAN_CFG_YONLY
					? 'scanned'
					: 'total'
				),
				sub.code,
				sub.description
			)
		);
	}

	function writeApiCall(subs) {
		writeln('<script type="text/javascript">');
		var codes = [];
		subs.forEach(function (sub) { codes.push(sub.code); });
		codes = codes.join('&sub=');
		writeln('getSubUnreadCount("' + codes + '");');
		writeln(
			'setInterval(' +
				'function(){getSubUnreadCount("' + codes + '");},' +
				'60000' +
			');'
		);
		writeln('</script>');
	}

	writeln(
		format(
			'<ol class="breadcrumb">' +
			'<li><a href="./?page=%s">Forum</a></li>' +
			'<li><a href="./?page=%s&amp;group=%s">%s</a></li>' +
			'</ol>',
			http_request.query.page[0],
			http_request.query.page[0],
			http_request.query.group[0],
			msg_area.grp_list[http_request.query.group[0]].name
		)
	);

	try {
		var subs = listSubs(http_request.query.group[0]);
		writeln('<div id="forum-list-container" class="list-group">');
		subs.forEach(writeSub);
		writeln('</div>');
		if (user.number > 0 && user.alias !== settings.guest) {
			writeApiCall(subs);
		}
	} catch (err) {
		log(LOG_WARNING, err);
	}

} else {

	// Group list
	function writeGroup(group) {
		writeln(
			format(
				'<a href="./?page=%s&amp;group=%s" ' +
				'class="list-group-item striped">' +
				'<h3><strong>%s</strong></h3>' +
				'<span title="Unread messages (other)" ' +
				'class="badge ignored" id="badge-ignored-%s">' +
				'</span>' +
				'<span title="Unread messages (scanned subs)" ' +
				'class="badge scanned" id="badge-scanned-%s">' +
				'</span>' +
				'<p>%s : %s sub-boards</p>' +
				'</a>',
				http_request.query.page[0],
				group.index,
				group.name,
				group.index,
				group.index,
				group.description,
				group.sub_count
			)
		);
	}

	function writeApiCall(groups) {
		writeln('<script type="text/javascript">');
		var indexes = [];
		groups.forEach(function(group) { indexes.push(group.index); });
		indexes = indexes.join('&group=');
		writeln('getGroupUnreadCount("' + indexes + '");');
		writeln(
			'setInterval(' +
				'function(){getGroupUnreadCount("' + indexes + '");},' +
				'60000' +
			');'
		);
		writeln('</script>');
	}
	
	writeln(
		'<ol class="breadcrumb">' +
		'<li>' +
		'<a href="./?page=' + http_request.query.page[0] + '">Forum</a>' +
		'</li>' +
		'</ol>'
	);

	try {
		var groups = listGroups();
		writeln('<div id="forum-list-container" class="list-group">');
		groups.forEach(writeGroup);
		writeln('</div>');
		if (user.number > 0 && user.alias !== settings.guest) {
			writeApiCall(groups);
		}
	} catch (err) {
		log(LOG_WARNING, err);
	}

}