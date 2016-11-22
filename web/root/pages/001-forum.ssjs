//Forum
if (typeof argv[0] !== 'boolean' || !argv[0]) exit();

load('sbbsdefs.js');
load(system.exec_dir + '../web/lib/init.js');
load(settings.web_lib + 'forum.js');

var strings = {
	script : {
		open : '<script type="text/javascript">',
		thread_navigation : 'threadNav();',
		interval : 'setInterval(function () { %s }, %s);',
		vote_buttons : 'enableVoteButtonHandlers("%s");',
		vote_refresh_thread : 'getVotesInThread("%s", %s)',
		vote_refresh_threads : 'getVotesInThreads("%s")',
		get_group_unread : 'getGroupUnreadCount("%s")',
		get_sub_unread : 'getSubUnreadCount("%s")',
		poll_control : 'pollControl("%s", %s)',
		close : '</script>'
	},
	notice_box : '<div id="noticebox" class="alert alert-warning">%s<script type="text/javascript">$("#noticebox").fadeOut(3000,function(){$("#noticebox").remove();});</script>',
	group_list : {
		breadcrumb : '<ol class="breadcrumb"><li><a href="./?page=%s">Forum</a></li></ol>',
		container : {
			open : '<div id="forum-list-container" class="list-group">',
			close : '</div>'
		},
		item : '<a href="./?page=%s&amp;group=%s" class="list-group-item striped"><h3><strong>%s</strong></h3><span title="Unread messages (other)" class="badge ignored" id="badge-ignored-%s"></span><span title="Unread messages (scanned subs)" class="badge scanned" id="badge-scanned-%s"></span><p>%s : %s sub-boards</p></a>'
	},
	sub_list : {
		breadcrumb : '<ol class="breadcrumb"><li><a href="./?page=%s">Forum</a></li><li><a href="./?page=%s&amp;group=%s">%s</a></li></ol>',
		container : {
			open : '<div id="forum-list-container" class="list-group">',
			close : '</div>'
		},
		item : '<a href="./?page=%s&amp;sub=%s" class="list-group-item striped%s"><h4><strong>%s</strong></h4><span title="Unread messages" class="badge %s" id="badge-%s"></span><p>%s</p></a>'
	},
	thread_list : {
		breadcrumb : '<ol class="breadcrumb"><li><a href="./?page=%s">Forum</a></li><li><a href="./?page=%s&amp;group=%s">%s</a></li><li><a href="./?page=%s&amp;sub=%s">%s</a></li></ol>',
		container : {
			open : '<div id="forum-list-container" class="list-group">',
			close : '</div>'
		},
		item : {
			open : '<a href="./?page=%s&amp;sub=%s&amp;thread=%s" class="list-group-item striped"><div class="row">',
			details : {
				open : '<div class="col-sm-9">',
				info : '<strong>%s</strong><p>By <strong>%s</strong> on %s</p><p>Latest reply by <strong>%s</strong> on %s</p>',
				close : '</div>'
			},
			badges : {
				open : '<div class="col-sm-3"><div class="pull-right">',
				unread : '<span title="Unread messages" class="badge%s" id="badge-%s">%s</span>',
				votes : {
					up : '<span id="uv-%s" title="Upvotes - Parent / Thread Total" class="badge upvote-bg"><span class="glyphicon glyphicon-arrow-up"></span><span id="uv-count-%s">%s / %s</span></span>&nbsp;',
					down : '<span id="dv-%s" title="Downvotes - Parent / Thread Total" class="badge downvote-bg"><span class="glyphicon glyphicon-arrow-down"></span><span id="dv-count-%s">%s / %s</span></span>',
				},
				poll : '<span class="badge">POLL</span>',
				close : '</div></div>'
			},
			close : '</div></a>'
		},
		controls : {
			post : '<button class="btn btn-default icon" aria-label="Post a new message" title="Post a new message" onclick="addNew(\'%s\')"><span class="glyphicon glyphicon-pencil"></span></button>',
			scan_new : '<div class="pull-right"><button id="scan-cfg-new" class="btn %s icon" aria-label="Scan for new messages" title="Scan for new messages" onclick="setScanCfg(\'%s\', 1)"><span class="glyphicon glyphicon-ok-sign"></span></button>',
			scan_you : '<button id="scan-cfg-youonly" class="btn %s icon" aria-label="Scan for new messages to you only" title="Scan for new messages to you only" onclick="setScanCfg(\'%s\', 2)"><span class="glyphicon glyphicon-user"></span></button>',
			scan_off : '<button id="scan-cfg-off" class="btn %s icon" aria-label="Do not scan this sub" title="Do not scan this sub" onclick="setScanCfg(\'%s\', 0)"><span class="glyphicon glyphicon-ban-circle"></span></button></div>'
		}
	},
	thread_view : {
		breadcrumb : '<ol class="breadcrumb"><li><a href="./?page=%s">Forum</a></li><li><a href="./?page=%s&amp;group=%s">%s</a></li><li><a href="./?page=%s&amp;sub=%s">%s</a></li></ol>',
		jump_unread : '<div id="jump-unread-container" style="margin-bottom:1em;" hidden><a class="btn btn-default" id="jump-unread" title="Jump to first unread message" href="#"><span class="glyphicon glyphicon-star"></span></a></div>',
		set_unread : '$("#jump-unread").attr("href", "#%s");$("#jump-unread-container").attr("hidden", false);',
		container : {
			open : '<ul id="forum-list-container" class="list-group">',
			close : '</ul>'
		},
		subject : '<h4><strong>%s</strong></h4>'
	},
	message : {
		anchor : '<a id="%s"></a>',
		open : '<li class="list-group-item striped" id="li-%s">',
		header : {
			open : '<div class="row">',
			details : {
				open : '<div class="col-sm-9">',
				unread : '<span title="Unread" class="glyphicon glyphicon-star"></span>',
				info : 'From <strong>%s</strong>%s to <strong>%s</strong> on %s',
				close : '</div>'
			},
			voting : {
				open : '<div class="col-sm-3"><div class="pull-right">',
				poll : '<span class="badge">POLL</span>',
				buttons : {
					up : '<button id="uv-%s" class="btn-uv btn btn-default icon %s"><span title="Upvotes" class="glyphicon glyphicon-arrow-up"></span><span id="uv-count-%s" title="Upvotes">%s</span></button>',
					down : '<button id="dv-%s" class="btn-dv btn btn-default icon %s"><span title="Downvotes" class="glyphicon glyphicon-arrow-down"></span><span id="dv-count-%s" title="Downvotes">%s</span></button>'
				},
				close : '</div></div>'
			},
			close : '</div>'
		},
		body : {
			open : '<div class="message" id="message-%s">',
			poll : {
				comment : '<strong>%s</strong>',
				answer : {
					container : {
						open : '<ul class="list-group">',
						close : '</ul>'
					},
					open : '<li class="%s"><label><input type="%s" name="poll-%s" value="%s">%s</label> %s</li>',
					closed : '<li class="checkbox%s">%02d. %s %s</li>'
				},
				button : '<button id="submit-poll-%s" class="btn btn-default" onclick="submitPollAnswers(\'%s\', %s)">Vote</button>',
				closed : 'This poll has been closed.',
				disallowed : 'You cannot vote on this poll.',
				voted : 'You already voted on this poll.'
			},
			close : '</div>'
		},
		controls : {
			oldest : '<a class="btn btn-default icon" title="Jump to oldest message" aria-label="Jump to oldest message" href="#%s"><span class="glyphicon glyphicon-fast-backward"></span></a>',
			previous : '<a class="btn btn-default icon" title="Jump to previous message" aria-label="Jump to previous message" href="#%s" id="pm-%s"><span class="glyphicon glyphicon-step-backward"></span></a>',
			link : '<a class="btn btn-default icon" title="Direct link to this message" aria-label="Direct link to this message" href="#%s"><span class="glyphicon glyphicon-link"></span></a>',
			next : '<a class="btn btn-default icon" title="Jump to next message" aria-label="Jump to next message" href="#%s" id="nm-%s"><span class="glyphicon glyphicon-step-forward"></span></a>',
			newest : '<a class="btn btn-default icon" title="Jump to newest message" aria-label="Jump to newest message" href="#%s"><span class="glyphicon glyphicon-fast-forward"></span></a>',
			reply : '<button class="btn btn-default icon" title="Reply to this message" aria-label="Reply to this message" name="reply-%s" onclick="addReply(\'%s\', \'%s\')"><span class="glyphicon glyphicon-comment"></span></button>',
			remove : '<button class="btn btn-default icon" title="Delete this message" aria-label="Delete this message" onclick="deleteMessage(\'%s\',\'%s\')"><span class="glyphicon glyphicon-trash"></span></button>'
		},
		close : '</li>'
	}
};

writeln('<script type="text/javascript" src="./js/forum.js"></script>');

if (typeof http_request.query.notice !== 'undefined') {
	writeln(format(strings.notice_box, http_request.query.notice[0]));
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

		writeln(format(strings.message.anchor, header.number));
		writeln(format(strings.message.open, header.number));

		// Show subject if first message
		if (index === 0) writeln(format(strings.thread_view.subject, thread.subject));

		// Header
		writeln(strings.message.header.open);

		writeln(strings.message.header.details.open);
		if (user.alias != settings.guest &&
			header.number > msg_area.sub[http_request.query.sub[0]].scan_ptr
		) {
			writeln(strings.message.header.details.unread);
			if (firstUnread === '') firstUnread += header.number;
		}			
		writeln(
			format(
				strings.message.header.details.info,
				header.from,
				typeof header.from_net_addr === 'undefined' ? '' : '@' + header.from_net_addr,
				header.to,
				(new Date(header.when_written_time * 1000)).toLocaleString()
			)
		);
		writeln(strings.message.header.details.close);

		writeln(strings.message.header.voting.open);
		if ((typeof settings.vote_buttons === 'undefined' || settings.vote_buttons) && !(header.attr&MSG_POLL)) {
			writeln(
				format(
					strings.message.header.voting.buttons.up,
					header.number,
					user.alias == settings.guest || user.security.restrictions&UFLAG_V || msgBase.cfg.settings&SUB_NOVOTING ? 'disabled' : '',
					header.number,
					header.upvotes
				)
			);
			writeln(
				format(
					strings.message.header.voting.buttons.down,
					header.number,
					user.alias == settings.guest || user.security.restrictions&UFLAG_V || msgBase.cfg.settings&SUB_NOVOTING ? 'disabled' : '',
					header.number,
					header.downvotes
				)
			);
		} else if (header.attr&MSG_POLL) {
			writeln(strings.message.header.voting.poll);
		}
		writeln(strings.message.header.voting.close);
		
		writeln(strings.message.header.close);

		// Body
		writeln(format(strings.message.body.open, header.number));
		// If this is a poll message
		if (header.attr&MSG_POLL) {

			var pollData = getUserPollData(http_request.query.sub[0], header.number);

			header.poll_comments.forEach(
				function (e) {
					writeln(format(strings.message.body.poll.comment, e.data));
				}
			);

			writeln(strings.message.body.poll.answer.container.open);
			// Poll is closed or user has voted
			if (header.auxattr&POLL_CLOSED || pollData.answers > 0) {
				header.poll_answers.forEach(
					function (e, i) {
						writeln(
							format(
								strings.message.body.poll.answer.closed,
								pollData.answers&(1<<i) ? ' upvote-bg' : '',
								i + 1, e.data,
								pollData.show_results ? header.tally[i] || 0 : ''
							)
						);
					}
				);
			// Poll is open and user hasn't voted
			} else {
				header.poll_answers.forEach(
					function (e, i) {
						writeln(
							format(
								strings.message.body.poll.answer.open,
								header.votes < 2 ? 'radio' : 'checkbox',
								header.votes < 2 ? 'radio' : 'checkbox',
								header.number, i, e.data,
								pollData.show_results ? header.tally[i] || 0 : ''
							)
						);
					}
				);
			}
			writeln(strings.message.body.poll.answer.container.close);

			if (header.auxattr&POLL_CLOSED) {
				writeln(strings.message.body.poll.closed);
			} else if (pollData.answers > 0) {
				writeln(strings.message.body.poll.voted);
			} else if (user.alias == settings.guest || user.security.restrictions&UFLAG_V || msgBase.cfg.settings&SUB_NOVOTING) {
				writeln(strings.message.body.poll.disallowed);
			} else {
				writeln(format(strings.message.body.poll.button, header.number, http_request.query.sub[0], header.number));
			}

			if (header.votes > 1) {
				writeln(strings.script.open);
				writeln(format(strings.script.poll_control, header.number, header.votes));
				writeln(strings.script.close);
			}

		// This is a normal message
		} else {
			writeln(formatMessage(body));
		}
		writeln(strings.message.body.close);

		var prev = index === 0 ? thread.messages[thread.__first].number : thread.messages[keys[index - 1]].number;
		var next = key === thread.__last ? thread.messages[thread.__last].number : thread.messages[keys[index + 1]].number;

		// Standard controls
		writeln(format(strings.message.controls.oldest, thread.messages[thread.__first].number));
		writeln(format(strings.message.controls.previous, prev, header.number));
		writeln(format(strings.message.controls.link, header.number));
		writeln(format(strings.message.controls.next, next, header.number));
		writeln(format(strings.message.controls.newest, thread.messages[thread.__last].number));

		// User can post
		if (user.alias !== settings.guest && msg_area.sub[msgBase.cfg.code].can_post) {
			writeln(format(strings.message.controls.reply, header.number, msgBase.cfg.code, header.number));
		}

		// User is operator
		if (user.alias != settings.guest && msg_area.sub[msgBase.cfg.code].is_operator) {
			writeln(format(strings.message.controls.remove, msgBase.cfg.code, header.number));
		}

		writeln(strings.message.close);

	}

	writeln(
		format(
			strings.thread_view.breadcrumb,
			http_request.query.page[0],
			http_request.query.page[0],
			msg_area.sub[http_request.query.sub[0]].grp_index,
			msg_area.sub[http_request.query.sub[0]].grp_name,
			http_request.query.page[0],
			http_request.query.sub[0],
			msg_area.sub[http_request.query.sub[0]].name
		)
	);
	writeln(strings.thread_view.jump_unread);

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
		writeln(strings.thread_view.container.open);
		keys.forEach(function (key, index) { writeMessage(thread, keys, key, index); });
		writeln(strings.thread_view.container.close);
		msgBase.close();
		if (keys.length > 1 && firstUnread !== '') {
			writeln(strings.script.open);
			writeln(format(strings.thread_view.set_unread, firstUnread));
			writeln(strings.script.close);
		}
		// Update scan pointer
		if (thread.messages[thread.__last].number > msg_area.sub[http_request.query.sub[0]].scan_ptr) {
			msg_area.sub[http_request.query.sub[0]].scan_ptr =
			thread.messages[thread.__last].number;
		}
	} catch (err) {
		log(LOG_WARNING, err);
	}

	writeln(strings.script.open);
	if (settings.keyboard_navigation) writeln(strings.script.thread_navigation);
	if (user.alias != settings.guest || user.security.restrictions&UFLAG_V) {
		writeln(format(strings.script.vote_buttons, http_request.query.sub[0]));
	}
	writeln(
		format(
			strings.script.vote_refresh_thread,
			http_request.query.sub[0], thread.__first
		)
	);
	writeln(
		format(
			strings.script.interval,
			format(
				strings.script.vote_refresh_thread,
				http_request.query.sub[0], thread.__first
			),
			settings.refresh_interval || 60000
		)
	);
	writeln(strings.script.close);

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
				strings.thread_list.item.open,
				http_request.query.page[0],
				http_request.query.sub[0],
				thread.id
			)
		);

		writeln(strings.thread_list.item.details.open);
		writeln(
			format(
				strings.thread_list.item.details.info,
				thread.subject,
				thread.messages[first].from,
				(new Date(thread.messages[first].when_written_time * 1000)).toLocaleString(),
				thread.messages[last].from,
				(new Date(thread.messages[last].when_written_time * 1000)).toLocaleString()
			)
		);
		writeln(strings.thread_list.item.details.close);

		writeln(strings.thread_list.item.badges.open);
		if (settings.vote_buttons && (thread.votes.up > 0 || thread.votes.down > 0)) {
			writeln(
				format(
					strings.thread_list.item.badges.votes.up,
					thread.messages[first].number,
					thread.messages[first].number,
					thread.messages[first].upvotes,
					thread.votes.up
				)
			);
			writeln(
				format(
					strings.thread_list.item.badges.votes.down,
					thread.messages[first].number,
					thread.messages[first].number,
					thread.messages[first].downvotes,
					thread.votes.down
				)
			);
		}
		writeln(
			format(
				strings.thread_list.item.badges.unread,
				msg_area.sub[http_request.query.sub[0]].scan_cfg&SCAN_CFG_NEW ? ' scanned' : '',
				thread.messages[first].number,
				(unread == 0 ? '' : unread)
			)
		);
		if (thread.messages[first].attr&MSG_POLL) {
			writeln(strings.thread_list.item.badges.poll);
		}
		writeln(strings.thread_list.item.badges.close);

		writeln(strings.thread_list.item.close);

	}

	writeln(
		format(
			strings.thread_list.breadcrumb,
			http_request.query.page[0],
			http_request.query.page[0],
			msg_area.sub[http_request.query.sub[0]].grp_index,
			msg_area.sub[http_request.query.sub[0]].grp_name,
			http_request.query.page[0],
			http_request.query.sub[0],
			msg_area.sub[http_request.query.sub[0]].name
		)
	);

	if (user.alias !== settings.guest && msg_area.sub[http_request.query.sub[0]].can_post) {
		writeln(format(strings.thread_list.controls.post, http_request.query.sub[0]));
	}

	if (user.alias !== settings.guest) {
		writeln(
			format(
				strings.thread_list.controls.scan_new,
				!(msg_area.sub[http_request.query.sub[0]].scan_cfg&SCAN_CFG_YONLY) && msg_area.sub[http_request.query.sub[0]].scan_cfg&SCAN_CFG_NEW ? 'btn-primary' : 'btn-default',
				http_request.query.sub[0]
			)
		);
		writeln(
			format(
				strings.thread_list.controls.scan_you,
				msg_area.sub[http_request.query.sub[0]].scan_cfg&SCAN_CFG_YONLY ? 'btn-primary' : 'btn-default',
				http_request.query.sub[0]
			)
		);
		writeln(
			format(
				strings.thread_list.controls.scan_off,
				!(msg_area.sub[http_request.query.sub[0]].scan_cfg&SCAN_CFG_NEW) && !(msg_area.sub[http_request.query.sub[0]].scan_cfg&SCAN_CFG_YONLY) ? 'btn-primary' : 'btn-default',
				http_request.query.sub[0]
			)
		);
	}

	try {
		var threads = getMessageThreads(http_request.query.sub[0], settings.max_messages);
		writeln(strings.thread_list.container.open);
		threads.order.forEach(function(t){writeThread(threads.thread[t]);});
		writeln(strings.thread_list.container.close);
	} catch (err) {
		log(LOG_WARNING, err);
	}

	writeln(strings.script.open);
	writeln(
		format(
			strings.script.interval,
			format(strings.script.vote_refresh_threads, http_request.query.sub[0]),
			settings.refresh_interval || 60000
		)
	);
	writeln(strings.script.close);

} else if (
	typeof http_request.query.group !== 'undefined' &&
	typeof msg_area.grp_list[http_request.query.group[0]] !== 'undefined'
) {

	// Sub list
	function writeSub(sub) {
		writeln(
			format(
				strings.sub_list.item,
				http_request.query.page[0],
				sub.code,
				sub.scan_cfg&SCAN_CFG_NEW || sub.scan_cfg&SCAN_CFG_YONLY ? ' scanned' : '',
				sub.name,
				sub.scan_cfg&SCAN_CFG_NEW || sub.scan_cfg&SCAN_CFG_YONLY ? 'scanned' : 'total',
				sub.code,
				sub.description
			)
		);
	}

	function writeApiCall(subs) {
		writeln(strings.script.open);
		var codes = [];
		subs.forEach(function (sub) { codes.push(sub.code); });
		codes = codes.join('&sub=');
		writeln(format(strings.script.get_sub_unread, codes));
		writeln(
			format(
				strings.script.interval,
				format(strings.script.get_sub_unread, codes),
				settings.refresh_interval || 60000
			)
		);
		writeln(strings.script.close);
	}

	writeln(
		format(
			strings.sub_list.breadcrumb,
			http_request.query.page[0],
			http_request.query.page[0],
			http_request.query.group[0],
			msg_area.grp_list[http_request.query.group[0]].name
		)
	);

	try {
		var subs = listSubs(http_request.query.group[0]);
		writeln(strings.sub_list.container.open);
		subs.forEach(writeSub);
		writeln(strings.sub_list.container.close);
		if (user.number > 0 && user.alias !== settings.guest) writeApiCall(subs);
	} catch (err) {
		log(LOG_WARNING, err);
	}

} else {

	// Group list
	function writeGroup(group) {
		writeln(
			format(
				strings.group_list.item,
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
		writeln(strings.script.open);
		var indexes = [];
		groups.forEach(function(group) { indexes.push(group.index); });
		indexes = indexes.join('&group=');
		writeln(format(strings.script.get_group_unread, indexes));
		writeln(
			format(
				strings.script.interval,
				format(strings.script.get_group_unread, indexes),
				settings.refresh_interval || 60000
			)
		);
		writeln(strings.script.close);
	}
	
	writeln(format(strings.group_list.breadcrumb, http_request.query.page[0]));

	try {
		var groups = listGroups();
		writeln(strings.group_list.container.open);
		groups.forEach(writeGroup);
		writeln(strings.group_list.container.close);
		if (user.number > 0 && user.alias !== settings.guest) writeApiCall(groups);
	} catch (err) {
		log(LOG_WARNING, err);
	}

}