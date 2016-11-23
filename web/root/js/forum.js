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
		function (evt) { evt.stopImmediatePropagation(); }
	);
}

function submitPoll(sub) {

	$('#newpoll-submit').attr('disabled', true);

	if ($('input[name="newpoll-answers"]:checked').length !== 1) return;

	var subject = $('#newpoll-subject').val();
	if (subject.length < 1) return;

	var answerCount = $('input[name="newpoll-answers"]:checked:first').val();
	if (answerCount == 2) answerCount = $('input[name="newpoll-answer-count"]').val();
	if (answerCount < 0 || answerCount > 15) return;

	var results = parseInt($('input[name="newpoll-results"]:checked').val());
	if (results < 0 || results > 3) return;

	var answers = [];
	$('input[name="newpoll-answer-input"]').each(
		function () {
			var val = $(this).val();
			if (val !== '') answers.push(val);
		}
	);
	if (answers.length < 1) return;

	var comments = [];
	$('input[name="newpoll-comment-input"]').each(
		function () {
			var val = $(this).val();
			if (val !== '') comments.push(val);
		}
	);

	var url =
		'./api/forum.ssjs?call=submit-poll' +
		'&sub=' + sub +
		'&subject=' + subject +
		'&votes=' + answerCount +
		'&results=' + results +
		'&answer=' + answers.join('&answer=');

	if (comments.length > 0) url += '&comment=' + comments.join('&comment=');

	$.getJSON(
		url, function (data) {
			$('#newpoll-submit').attr('disabled', false);
			if (data.success) {
				$('#newpoll').remove();
				insertParam('notice', 'Your poll has been posted.');
			}
		}
	);

}

function addPollField(type, elem) {

	var prefix = 'newpoll-' + type;

	var count = $('div[name="' + prefix + '"]').length;
	if (type === 'answer' && count > 15) return;
	var number = count + 1;

	$(elem).append(
		'<div id="' + prefix + '-container-' + number + '" name="' + prefix + '" class="form-group">' +
			'<label for="' + prefix + '-' + number + '" class="col-sm-2 control-label">' +
				(type === 'answer' ? 'Answer' : 'Comment') +
			'</label>' +
			'<div class="col-sm-9">' +
				'<input id="' + prefix + '-' + number + '" class="form-control" name="' + prefix + '-input" type="text" maxlength="70"> ' +
			'</div>' +
			'<div class="col-sm-1">' +
				'<button type="button" class="btn btn-danger" onclick="$(\'#' + prefix + '-container-' + number + '\').remove()">' +
					'<span class="glyphicon glyphicon-remove"></span>' +
				'</button> ' +
			'</div>' +
		'</div>'
	);

	$('#' + prefix + '-' + number).keydown(
		function (evt) { evt.stopImmediatePropagation(); }
	);

}

function addPoll(sub) {
	if ($('#newpoll').length > 0) return;
	$('#forum-list-container').append(
		'<li id="newpoll" class="list-group-item">' +
			'<strong>Add a new poll</strong>' +
			'<form id="newpoll-form" class="form-horizontal">' +
				'<div class="form-group">' +
					'<label for="newpoll-subject" class="col-sm-2 control-label">Question</label>' +
					'<div class="col-sm-10">' +
						'<input id="newpoll-subject" class="form-control" type="text" placeholder="Required" maxlength="70">' +
					'</div>' +
				'</div>' +
				'<div id="newpoll-comment-group"></div>' +
				'<div class="form-group">' +
					'<label for="newpoll-answers" class="col-sm-2 control-label">Selection</label>' +
					'<div class="col-sm-10">' +
						'<label class="radio-inline">' +
							'<input type="radio" name="newpoll-answers" value="1" checked> Single' +
						'</label>' +
						'<label class="radio-inline">' +
							'<input type="radio" name="newpoll-answers" value="2"> Multiple ' +
							'<input type="number" name="newpoll-answer-count" min="1" max="15" value="1">' +
						'</label>' +
					'</div>' +
				'</div>' +
				'<div class="form-group">' +
					'<label for="newpoll-results" class="col-sm-2 control-label">Show results</label>' +
					'<div class="col-sm-10">' +
						'<label class="radio-inline">' +
							'<input type="radio" name="newpoll-results" value="0" checked> Voters' +
						'</label>' +
						'<label class="radio-inline">' +
							'<input type="radio" name="newpoll-results" value="1">  Everyone' +
						'</label>' +
						'<label class="radio-inline">' +
							'<input type="radio" name="newpoll-results" value="2"> Me Only (Until closed) ' +
						'</label>' +
						'<label class="radio-inline">' +
							'<input type="radio" name="newpoll-results" value="3"> Me Only ' +
						'</label>' +
					'</div>' +
				'</div>' +
				'<div id="newpoll-answer-group"></div>' +
				'<div id="newpoll-button" class="form-group">' +
					'<div class="col-sm-offset-2 col-sm-10">' +
						'<button id="newpoll-submit" type="button" class="btn btn-primary" onclick="submitPoll(\'' + sub + '\')">' +
							'Submit' +
						'</button>' +
						'<div class="pull-right">' +
							'<button type="button" title="Add another comment" class="btn btn-success" onclick="addPollField(\'comment\', \'#newpoll-comment-group\')">' +
								'<span class="glyphicon glyphicon-pencil"></span>' +
							'</button> ' +
							'<button type="button" title="Add another answer" class="btn btn-success" onclick="addPollField(\'answer\', \'#newpoll-answer-group\')">' +
								'<span class="glyphicon glyphicon-plus"></span>' +
							'</button> ' +
						'</div>' +
				    '</div>' +
				'</div>' +
			'</form>' +
		'</li>'
	);
	addPollField('comment', '#newpoll-comment-group');
	addPollField('answer', '#newpoll-answer-group');
	addPollField('answer', '#newpoll-answer-group');
	window.location.hash = '#newpoll';
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

function submitPollAnswers(sub, id) {
	if ($('input[name="poll-' + id + '"]:checked').length < 1) return;
	var answers = [];
	$('input[name="poll-' + id + '"]:checked').each(
		function () { answers.push($(this).val()); }
	);
	answers = answers.join('&answer=');
	$.getJSON(
		'./api/forum.ssjs?call=submit-poll-answers&sub=' + sub + '&id=' + id + '&answer=' + answers,
		function (data) {
			$('input[name="poll-' + id + '"]').each(
				function () {
					$(this).attr('disabled', true);
					if ($(this).prop('checked')) {
						$(this).parent().parent().addClass('upvote-bg');
					}
				}
			);
			$('submit-poll-' + id).attr('disabled', true);
		}
	);
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

function getPollData(sub, id) {
	$.getJSON(
		'./api/forum.ssjs?call=get-poll-results&sub=' + sub + '&id=' + id,
		function (data) {
			data.tally.forEach(
				function (e, i) {
					if (e > 0) $('#poll-count-' + id + '-' + i).text(e);
				}
			);
			if (data.answers > 0) {
				$('input[name="poll-' + id + '"]').each(
					function () { $(this).attr('disabled', true); }
				);
				$('#submit-poll-' + id).attr('disabled', true);
			}
		}
	);
}