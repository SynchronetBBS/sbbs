// Add a parameter to the query string
var insertParam = function(key, value) {
    key = encodeURIComponent(key);
    value = encodeURIComponent(value);
    var kvp = window.location.search.substr(1).split('&');
    var i = kvp.length,	x;
    while(i--) {
        x = kvp[i].split('=');
        if (x[0]==key) {
            x[1] = value;
            kvp[i] = x.join('=');
            break;
        }
    }
    if(i<0)
    	kvp[kvp.length] = [key,value].join('=');
    window.location.search = kvp.join('&'); 
}

// For now we'll just remove nested quotes from the parent post
var quotify = function(id) {
	$('#quote-' + id).attr("disabled", true);
	var html = $('#message-' + id).clone();
	html.find('blockquote').remove();
	$('#replytext-' + id).val(
		html.text().replace(/\n\s*\n\s*\n/g, '\n\n').split(/\r?\n/).map(
			function(line) { return ("> " + line); }
		).join("\n") +
		$('#replytext-' +id).val()
	);
}

// (Try to) post a new message to 'sub' via the web API
var postNew = function(sub) {
	$('#newmessage-button').attr("disabled", true);
	var to = $('#newmessage-to').val();
	var subject = $('#newmessage-subject').val();
	var body = $('#newmessage-body').val();
	$.ajax(
		{	'url' : "./api/forum.ssjs",
			'method' : "POST",
			'data' : {
				'call' : "post",
				'sub' : sub,
				'to' : to,
				'subject' : subject,
				'body' : body
			}
		}
	).done(
		function(data) {
			if(data.success) {
				$('#newmessage').remove();
				insertParam("notice", "Your message has been posted.");
			}
			$('#newmessage-button').attr("disabled", false);
		}
	);
}

// (Try to) post a reply to message number 'id' of 'sub' via the web API
var postReply = function(sub, id) {
	$('#reply-button-' + id).attr("disabled", true);
	var body = $('#replytext-' + id).val();
	$.ajax(
		{	'url' : "./api/forum.ssjs",
			'method' : "POST",
			'data' : {
				'call' : "post-reply",
				'sub' : sub,
				'body' : $('#replytext-' + id).val(),
				'pid' : id
			}
		}
	).done(
		function(data) {
			if(data.success) {
				$('#quote-' + id).attr("disabled", false);
				$('#replybox-' + id).remove();
				insertParam("notice", "Your message has been posted.");
			} else {
				$('#reply-button-' + id).attr("disabled", false);
			}
		}
	);
}

// (Try to) delete a message via the web API
var deleteMessage = function(sub, id) {
	$.getJSON(
		'./api/forum.ssjs?call=delete-message&sub=' + sub + '&number=' + id,
		function(data) {
			console.log(data);
			if(data.success) {
				$('#li-' + id).remove();
				insertParam("notice", "Message deleted.");
			}
		}
	);
}

// Add a new message input form to the element with id 'forum-list-container' for sub 'sub'
var addNew = function(sub) {
	if($('#newmessage').length > 0)
		return;
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
		function(data) {
			$('#newmessage-body').val($('#newmessage-body').val() + "\r\n" + data.signature);
		}
	);
	window.location.hash = "#newmessage";
}

// Add a reply input form to the page for message with number 'id' in sub 'sub'
var addReply = function(sub, id) {
	if($('#replybox-' + id).length > 0)
		return;
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
		function(data) {
			$('#replytext-' + id).val($('#replytext-' + id).val() + "\r\n" + data.signature);
		}
	);
}

// 'sub' can be a single sub code, or a string of <sub1>&sub=<sub2>&sub=<sub3>...
var getSubUnreadCount = function(sub) {
	$.getJSON(
		"./api/forum.ssjs?call=get-sub-unread-count&sub=" + sub,
		function(data) {
			for(sub in data) {
				if(data[sub].scanned > 0)
					$('#badge-' + sub).text(data[sub].total);
				else if(data[sub].total > 0)
					$('#badge-' + sub).text(data[sub].total);
				else
					$('#badge-' + sub).text("");
			}
		}
	);
}

// 'group' can be a single group index, or a string of 0&group=1&group=2...
var getGroupUnreadCount = function(group) {
	$.getJSON(
		"./api/forum.ssjs?call=get-group-unread-count&group=" + group,
		function(data) {
			for(group in data) {
				$('#badge-scanned-' + group).text((data[group].scanned == 0 ? "" : data[group].scanned));
				$('#badge-ignored-' + group).text((data[group].total == 0 || data[group].total == data[group].scanned ? "" : (data[group].total - data[group].scanned)));
			}
		}
	);
}

/*  Fetch a private mail message's body (with links to attachments) where 'id'
	is the message number.	Output it to an element with id 'message-<id>'. */
var getMailBody = function(id) {
	if(!$('#message-' + id).attr('hidden')) {
		$('#message-' + id).attr('hidden', true);
	} else if($('#message-' + id).html() != "") {
		$('#message-' + id).attr('hidden', false);
	} else {
		$.getJSON(
			"./api/forum.ssjs?call=get-mail-body&number=" + id,
			function(data) {
				var str = data.body;
				if(data.inlines.length > 0)
					str += "<br>Inline attachments: " + data.inlines.join("<br>") + "<br>";
				if(data.attachments.length > 0)
					str += "<br>Attachments: " + data.attachments.join("<br>") + "<br>";
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

var setScanCfg = function(sub, cfg) {
	var opts = [
		"scan-cfg-off",
		"scan-cfg-new",
		"scan-cfg-youonly"
	];
	$.getJSON(
		"./api/forum.ssjs?call=set-scan-cfg&sub=" + sub + "&cfg=" + cfg,
		function(data) {
			if(!data.success)
				return;
			opts.forEach(
				function(opt, index) {
					$('#' + opt).toggleClass('btn-primary', (cfg == index));
					$('#' + opt).toggleClass('btn-default', (cfg != index));
				}
			);
		}
	);
}
