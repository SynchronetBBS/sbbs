// Add a parameter to the query string
function insertParam(key, value) {
    key = encodeURIComponent(key);
    value = encodeURIComponent(value);
    var kvp = window.location.search.substr(1).split('&');
    var i = kvp.length,	x;
    while (i--) {
        x = kvp[i].split('=');
        if (x[0] == key) {
            x[1] = value;
            kvp[i] = x.join('=');
            break;
        }
    }
    if (i<0) kvp[kvp.length] = [key,value].join('=');
    window.location.search = kvp.join('&'); 
}

// For now we'll just remove nested quotes from the parent post
function quotify(id) {
	$('#quote-' + id).attr('disabled', true);
	var html = $('#message-' + id).clone();
	html.find('blockquote').remove();
	$('#replytext-' + id).val(
		html.text().replace(/\n\s*\n\s*\n/g, '\n\n').split(/\r?\n/).map(
			function (line) { return ("> " + line); }
		).join('\n') +
		$('#replytext-' +id).val()
	);
}

// (Try to) post a new message to 'sub' via the web API
function postNew(sub) {
	$('#newmessage-button').attr('disabled', true);
	var to = $('#newmessage-to').val();
	var subject = $('#newmessage-subject').val();
	var body = $('#newmessage-body').val();
	$.ajax(
		{	url : './api/forum.ssjs',
			method : 'POST',
			data : {
				call : 'post',
				sub : sub,
				to : to,
				subject : subject,
				body : body
			}
		}
	).done(
		function (data) {
			if (data.success) {
				$('#newmessage').remove();
				insertParam('notice', 'Your message has been posted.');
			}
			$('#newmessage-button').attr('disabled', false);
		}
	);
}

// (Try to) post a reply to message number 'id' of 'sub' via the web API
function postReply(sub, id) {
	$('#reply-button-' + id).attr('disabled', true);
	var body = $('#replytext-' + id).val();
	$.ajax(
		{	url : './api/forum.ssjs',
			method : 'POST',
			data : {
				call : 'post-reply',
				sub : sub,
				body : $('#replytext-' + id).val(),
				pid : id
			}
		}
	).done(
		function (data) {
			if (data.success) {
				$('#quote-' + id).attr('disabled', false);
				$('#replybox-' + id).remove();
				insertParam('notice', 'Your message has been posted.');
			} else {
				$('#reply-button-' + id).attr('disabled', false);
			}
		}
	);
}

// (Try to) delete a message via the web API
function deleteMessage(sub, id) {
	$.getJSON(
		'./api/forum.ssjs?call=delete-message&sub=' + sub + '&number=' + id,
		function (data) {
			console.log(data);
			if (data.success) {
				$('#li-' + id).remove();
				insertParam('notice', 'Message deleted.');
			}
		}
	);
}

// Add a new message input form to the element with id 'forum-list-container' for sub 'sub'
function addNew(sub) {
	if ($('#newmessage').length > 0) return;
	$('#forum-list-container').append(
		'<li id="newmessage" class="list-group-item">' +
		'<input id="newmessage-to" class="form-control" type="text" placeholder="To"><br>' +
		'<input id="newmessage-subject" class="form-control" type="text" placeholder="Subject"><br>' +
		'<textarea id="newmessage-body" class="form-control" rows="8"></textarea><br>' +
		'<input id="newmessage-button" class="btn btn-primary" type="submit" value="Submit" onclick="postNew(\'' + sub + '\')">' +
		'</li>'
	);
	$.getJSON(
		'./api/forum.ssjs?call=get-signature',
		function (data) {
			$('#newmessage-body').val(
				$('#newmessage-body').val() + '\r\n' + data.signature
			);
		}
	);
	window.location.hash = '#newmessage';
	$('#newmessage-body').keydown(
		function (evt) {
			evt.stopImmediatePropagation();
		}
	);
}

// Add a reply input form to the page for message with number 'id' in sub 'sub'
function addReply(sub, id) {
	if ($('#replybox-' + id).length > 0) return;
	$('#li-' + id).append(
		'<div class="reply" id="replybox-' + id + '">' +
		'<strong>Reply</strong>' +
		'<textarea rows="8" class="form-control reply" id="replytext-' + id + '""></textarea>' +
		'<button id="quote-' + id + '" class="btn" onclick="quotify(' + id + ')">Quote</button> ' +
		'<input id="reply-button-' + id + '" class="btn btn-primary" type="submit" value="Submit" onclick="postReply(\'' + sub + '\', ' + id + ')">' +
		'</div>'
	);
	$.getJSON(
		'./api/forum.ssjs?call=get-signature',
		function (data) {
			$('#replytext-' + id).val(
				$('#replytext-' + id).val() + '\r\n' + data.signature
			);
		}
	);
	$('#replytext-' + id).keydown(
		function (evt) {
			evt.stopImmediatePropagation();
		}
	);
}

// 'sub' can be a single sub code, or a string of <sub1>&sub=<sub2>&sub=<sub3>...
function getSubUnreadCount(sub) {
	$.getJSON(
		'./api/forum.ssjs?call=get-sub-unread-count&sub=' + sub,
		function(data) {
			for (sub in data) {
				if (data[sub].scanned > 0) {
					$('#badge-' + sub).text(data[sub].total);
				} else if (data[sub].total > 0) {
					$('#badge-' + sub).text(data[sub].total);
				} else {
					$('#badge-' + sub).text('');
				}
			}
		}
	);
}

// 'group' can be a single group index, or a string of 0&group=1&group=2...
function getGroupUnreadCount(group) {
	$.getJSON(
		'./api/forum.ssjs?call=get-group-unread-count&group=' + group,
		function (data) {
			for (group in data) {
				$('#badge-scanned-' + group).text(
					(data[group].scanned == 0 ? "" : data[group].scanned)
				);
				$('#badge-ignored-' + group).text(
					(	data[group].total == 0 ||
						data[group].total == data[group].scanned
						? ''
						: (data[group].total - data[group].scanned)
					)
				);
			}
		}
	);
}

/*  Fetch a private mail message's body (with links to attachments) where 'id'
	is the message number.	Output it to an element with id 'message-<id>'. */
function getMailBody(id) {
	if (!$('#message-' + id).attr('hidden')) {
		$('#message-' + id).attr('hidden', true);
	} else if ($('#message-' + id).html() != '') {
		$('#message-' + id).attr('hidden', false);
	} else {
		$.getJSON(
			'./api/forum.ssjs?call=get-mail-body&number=' + id,
			function (data) {
				var str = data.body;
				if (data.inlines.length > 0) {
					str +=
						'<br>Inline attachments: ' +
						data.inlines.join('<br>') + '<br>';
				}
				if (data.attachments.length > 0) {
					str +=
						'<br>Attachments: ' +
						data.attachments.join('<br>') + '<br>';
				}
				str +=
					'<button class="btn btn-default icon" ' +
					'aria-label="Reply to this message" ' +
					'title="Reply to this message" ' +
					'name="reply-' + id + '" ' +
					'onclick="addReply(\'mail\',' + id + ')">' +
					'<span class="glyphicon glyphicon-comment"></span>' +
					'</button>' +
					'<button class="btn btn-default icon" aria-label="Delete this message" ' +
					'title="Delete this message" onclick="deleteMessage(\'mail\',' + id + ')">' +
					'<span class="glyphicon glyphicon-trash"></span>' +
					'</button>';
				$('#message-' + id).html(str);
				$('#message-' + id).attr('hidden', false);
			}
		);
	}
}

function setScanCfg(sub, cfg) {
	var opts = [
		'scan-cfg-off',
		'scan-cfg-new',
		'scan-cfg-youonly'
	];
	$.getJSON(
		'./api/forum.ssjs?call=set-scan-cfg&sub=' + sub + '&cfg=' + cfg,
		function (data) {
			if (!data.success) return;
			opts.forEach(
				function (opt, index) {
					$('#' + opt).toggleClass('btn-primary', (cfg == index));
					$('#' + opt).toggleClass('btn-default', (cfg != index));
				}
			);
		}
	);
}

function threadNav() {

	if (window.location.hash === '') {
		$($('#forum-list-container').children('.list-group-item')[0]).addClass(
			'current'
		);
	} else if ($('#li-' + window.location.hash.substr(1)).length > 0) {
		$('#li-' + window.location.hash.substr(1)).addClass('current');
	}

	$(window).keydown(
		function (evt) {
			var cid = $(
				$('#forum-list-container').children('.current')[0]
			).attr(
				'id'
			).substr(3);
			switch (evt.keyCode) {
				case 37:
					// Left
					window.location.hash = $('#pm-' + cid).attr('href');
					break;
				case 39:
					// Right
					window.location.hash = $('#nm-' + cid).attr('href');
					break;
				default:
					break;
			}
		}
	);

	$(window).on(
		'hashchange',
		function () {
			$('#forum-list-container').children('.current').removeClass(
				'current'
			);
			var id = window.location.hash.substr(1);
			if ($('#li-' + id).length < 1) return;
			$('#li-' + id).addClass('current');
		}
	);

}

function vote(sub, id) {
	id = id.split('-');
	if (id.length !== 2 ||
		(id[0] !== 'uv' && id[0] !== 'dv') ||
		isNaN(parseInt(id[1]))
	) {
		return;
	}
	$.getJSON(
		'./api/forum.ssjs?call=vote&sub=' + sub + '&id=' + id[1] + '&up=' + (id[0] === 'uv' ? 1 : 0),
		function (data) {
			if (!data.success) return;
			$('#' + id[0] + '-' + id[1]).addClass(
				id[0] === 'uv' ? 'upvote-fg' : 'downvote-fg'
			);
			$('#' + id[0] + '-' + id[1]).attr('disabled', true);
			$('#' + id[0] + '-' + id[1]).blur();
			var count = parseInt($('#' + id[0] + '-count-' + id[1]).text()) + 1;
			$('#' + id[0] + '-count-' + id[1]).text(count);
		}
	);
}

function enableVoteButtonHandlers(sub) {
	$('.btn-uv').click(function () { vote(sub, this.id); });
	$('.btn-dv').click(function () { vote(sub, this.id); });
}

function getVotesInThread(sub, id) {
	$.getJSON(
		'./api/forum.ssjs?call=get-thread-votes&sub=' + sub + '&id=' + id,
		function (data) {
			Object.keys(data.m).forEach(
				function (m) {
					var uv = parseInt($('#uv-count-' + m).text());
					var dv = parseInt($('#dv-count-' + m).text());
					if (uv !== data.m[m].u) {
						$('#uv-count-' + m).text(data.m[m].u);
						$('#uv-' + m).addClass('indicator');
					}
					if (dv !== data.m[m].d) {
						$('#dv-count-' + m).text(data.m[m].d);
						$('#dv-' + m).addClass('indicator');
					}
					switch (data.m[m].v) {
						case 1:
							$('#uv-' + m).addClass('upvote-fg');
							$('#uv-' + m).attr('disabled', true);							
							$('#dv-' + m).attr('disabled', true);							
							break;
						case 2:
							$('#dv-' + m).addClass('downvote-fg');
							$('#uv-' + m).attr('disabled', true);							
							$('#dv-' + m).attr('disabled', true);							
							break;
						default:
							break;
					}
				}
			);
		}
	);
}

function getVotesInThreads(sub) {
	$.getJSON(
		'./api/forum.ssjs?call=get-sub-votes&sub=' + sub,
		function (data) {
			Object.keys(data).forEach(
				function (t) {
					var uv = data[t].p.u + ' / ' + data[t].t.u;
					var dv = data[t].p.d + ' / ' + data[t].t.d;
					if (uv !== $('#uv-count-' + t).text()) {
						$('#uv-count-' + t).text(uv);
					}
					if (dv !== $('#dv-count-' + t).text()) {
						$('#dv-count-' + t).text(dv);
					}
				}
			);
		}
	);
}

function submitPollAnswers(id) {
	$('input[name="poll-' + id + '"]:checked').each(
		function () {
			alert($(this).val());
		}
	);
	// async ./api/forum.ssjs?call=submit-poll-answers&sub=x&id=x&answer=x&answer=x...
}

function pollControl(id, count) {
	$('input[name="poll-' + id + '"]').each(
		function () {
			$(this).change(
				function () {
					if ($('input[name="poll-' + id + '"]:checked').length >= count) {
						$('input[name="poll-' + id + '"]:not(:checked)').each(
							function () { $(this).attr('disabled', true); }
						);
					} else {
						$('input[name="poll-' + id + '"]:not(:checked)').each(
							function () { $(this).attr('disabled', false); }
						);
					}
				}
			);
		}
	);
}