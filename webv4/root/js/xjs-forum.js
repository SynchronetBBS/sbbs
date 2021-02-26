let __v4_forum_offset = 0; // Feels hacky

async function addNew(sub) {

    if (document.getElementById('newmessage') !== null) return;

    const elem = document.getElementById('forum-new-message-template').cloneNode(true);
    elem.id = 'newmessage';
    elem.innerHTML = elem.innerHTML.replace(/SUB/g, sub);

    const li = document.createElement('li');
    li.id = 'newmessage-li';
    li.className = 'list-group-item';
    li.appendChild(elem);
    document.getElementById('forum-list-container').appendChild(li);

    elem.removeAttribute('hidden');

    const data = await v4_get('./api/forum.ssjs?call=get-signature');
    const nmb = elem.getElementsByTagName('textarea')[0];
    nmb.value += `\r\n${data.signature}`;
    nmb.setSelectionRange(0, 0);
	window.location.hash = '#newmessage';
	nmb.onkeydown = evt => evt.stopImmediatePropagation();

}

async function postNew(sub) {

    document.getElementById('newmessage').getElementsByTagName('input')[2].setAttribute('disabled', true);

	const data = await v4_post('./api/forum.ssjs', {
		call: 'post',
		sub,
		to: document.getElementById('newmessage-to').value,
		subject: document.getElementById('newmessage-subject').value,
		body: document.getElementById('newmessage-body').value,
	});

    document.getElementById('newmessage').getElementsByTagName('input')[2].setAttribute('disabled', true);

    if (data.success) {
        const li = document.getElementById('newmessage-li');
        li.parentNode.removeChild(li);
		insertParam('notice', 'Your message has been posted.'); // This is stupid.
	}

}

async function addReply(sub, id) {

    if (document.getElementById(`replybox-${id}`) !== null) return;

    const elem = document.getElementById('forum-message-reply-template').cloneNode(true);
    elem.id = `replybox-${id}`;
    elem.innerHTML = elem.innerHTML.replace(/SUB/g, sub);
    elem.innerHTML = elem.innerHTML.replace(/ID/g, id);
    elem.removeAttribute('hidden');

	const data = await v4_get('./api/forum.ssjs?call=get-signature');
    const nmb = elem.getElementsByTagName('textarea')[0];
    nmb.value += `\r\n${data.signature}`;
    nmb.setSelectionRange(0, 0);
    nmb.onkeydown = evt => evt.stopImmediatePropagation();

}

async function postReply(sub, id) {
    document.getElementById(`reply-button-${id}`).setAttribute('disabled', true);
	const data = await v4_post('./api/forum.ssjs', {
		call: 'post-reply',
		sub,
		body: body = document.getElementById(`reply-button-${id}`).value,
		pid: id,
	});
	if (data.success) {
        document.getElementById(`quote-${id}`).setAttribute('disabled', false);
        const rb = document.getElementById(`replybox-${id}`);
        rb.parentNode.removeChild(rb);
		insertParam('notice', 'Your message has been posted.'); // This is stupid.
	} else {
        document.getElementById(`reply-button-${id}`).setAttribute('disabled', false);
	}
}

async function postNewPoll(sub) {

    document.getElementById('newpoll-submit').setAttribute('disabled', true);

    if (document.querySelectorAll('input[name="newpoll-answers"]:checked').length !== 1) return;

	const subject = document.getElementById('newpoll-subject').value;
	if (subject.length < 1) return;

	let answerCount = document.querySelector('input[name="newpoll-answers"]:checked:first').value;
	if (answerCount == 2) answerCount = document.querySelector('input[name="newpoll-answer-count"]').value;
	if (answerCount < 0 || answerCount > 15) return;

	const results = parseInt(document.querySelector('input[name="newpoll-results"]:checked').value);
	if (results < 0 || results > 3) return;

    const answers = Array.from(document.querySelectorAll('input[name="forum-new-poll-field-answer"]')).reduce((a, c) => {
        if (c.value !== '') a.push(c.value);
        return a;
    }, []);
    if (!answers.length) return;

    const comments = Array.from(document.querySelectorAll('input[name="forum-new-poll-field-comment"]')).reduce((a, c) => {
        if (c.value !== '') a.push(c);
        return a;
    }, []);

	const post_data = {
		sub,
		subject,
		votes: answerCount,
		results,
		answer: answers
	};
	if (comments.length) post_data.comment = comments;
    const res = await v4_post('./api/forum.ssjs?call=submit-poll', post_data);
    document.getElementById('newpoll-submit').setAttribute('disabled', false);
	if (res.success) {
        const np = document.getElementById('forum-new-poll');
        np.parentNode.removeChild(np);
		insertParam('notice', 'Your poll has been posted.'); // This is stupid
	}

}

function addPoll(sub) {

    if (document.getElementById('forum-new-poll') !== null) return;

    const elem = document.getElementById('forum-new-poll-template').cloneNode(true);
    elem.id = 'forum-new-poll';
    elem.innerHTML = elem.innerHTML.replace(/\-template/g, '');
    elem.innerHTML = elem.innerHTML.replace(/SUB/g, sub);
    elem.removeAttribute('hidden');

    const li = document.createElement('li');
    li.id = 'newpoll-li';
    li.className = 'list-group-item';
    li.appendChild(elem);
    document.getElementById('forum-list-container').appendChild(li);

    addPollField('comment', 'newpoll-comment-group');
	addPollField('answer', 'newpoll-answer-group');
	addPollField('answer', 'newpoll-answer-group');
	window.location.hash = '#newpoll';

}

function addPollField(type, target) {
    
    const prefix = `forum-new-poll-field-${type}`;
	const count = document.querySelectorAll(`div[name="${prefix}"]`).length;
	if (type === 'answer' && count > 15) return;
	const number = count + 1;

    const elem = document.getElementById(`forum-new-poll-field-container-template`).cloneNode(true);
    elem.id = `${prefix}-${number}`;
    elem.innerHTML = elem.innerHTML.replace(/\-template/g, '');
    elem.innerHTML = elem.innerHTML.replace(/TYPE/g, type);
    elem.innerHTML = elem.innerHTML.replace(/NUMBER/g, number);
	elem.onkeydown = evt => evt.stopImmediatePropagation();
    elem.removeAttribute('hidden');

    document.getElementById(target).appendChild(elem);

}

async function setScanCfg(sub, cfg) {
	var opts = [
        'scan-cfg-off',
        'scan-cfg-new',
        'scan-cfg-youonly',
    ];
	const data = await v4_get(`./api/forum.ssjs?call=set-scan-cfg&sub=${sub}&cfg=${cfg}`);
	if (!data.success) return;
	opts.forEach((e, i) => {
        const elem = document.getElementById(`${e}`);
        if (cfg == i) {
            elem.classList.add('btn-primary');
            elem.classList.remove('btn-default');
        } else {
            elem.classList.add('btn-default');
            elem.classList.remove('btn-primary');
        }
	});
}

function LoadingMessage() {

    let pos = 0;
    let cursor = ['|', '/', '—', '\\' ];
    let evt;

    const flc = document.getElementById('forum-list-container');
    
    this.start = function () {
        const elem = document.querySelector('div[data-loading-template]').cloneNode(true);
        const sc = elem.querySelector('span[data-spinning-cursor]');
        elem.removeAttribute('hidden');
        flc.appendChild(elem);
        evt = setInterval(() => {
            sc.innerHTML = cursor[pos % cursor.length];
            pos++;
        }, 250);
    }
    
    this.stop = function () {
        flc.removeChild(flc.querySelector('div[data-loading-template]'));
        clearInterval(evt);
    }

}

function loading(l) {
    var flc = document.getElementById('forum-list-container');
    if (l) {
        var elem = document.querySelector('div[data-loading-template]').cloneNode(true);
        elem.removeAttribute('hidden');
        flc.appendChild(elem);
    } else {
        flc.removeChild(flc.querySelector('div[data-loading-template]'));
    }
}

async function listMessages(sub, thread, count, after) {

    const dlmm = document.getElementById('forum-load-more-messages');
    const blmm = document.getElementById('load-more-messages');
    dlmm.setAttribute('hidden', true);
    blmm.setAttribute('disabled', true);

    const lm = new LoadingMessage();
    lm.start();
    let _data;
    let data = JSON.parse(localStorage.getItem(`${sub}-${thread}`));
    if (data === null) {
        data = await v4_fetch_jsonl(`./api/forum.ssjs?call=get-thread&sub=${sub}&thread=${thread}&count=${count}`);
    } else if (after) {
        _data = await v4_fetch_jsonl(`./api/forum.ssjs?call=get-thread&sub=${sub}&thread=${thread}&count=${count}&after=${data[data.length - 1].number}`);
        data = data.concat(_data);
    }
    localStorage.setItem(`${sub}-${thread}`, JSON.stringify(data));
    lm.stop();

    const users = [];
    (_data || data).forEach((e, i) => {
        let akey;
        const elem = document.getElementById('forum-message-template').cloneNode(true);
        elem.id = elem.id.replace(/template$/, e.number);
        elem.querySelector('a[data-message-anchor]').id = e.number;
        if (i == 0 && !after && e.subject !== undefined) elem.querySelector('strong[data-message-subject]').innerHTML = e.subject;
        elem.querySelector('strong[data-message-from]').innerHTML = e.from;
        if (e.from_net_addr) {
            akey = `${e.from}@${e.from_net_addr}`;
            elem.querySelector('span[data-message-from-address]').innerHTML = `@${e.from_net_addr}`;
            elem.querySelector('div[data-avatar]').setAttribute('data-avatar', `${e.from}@${e.from_net_addr}`);
        } else {
            akey = e.from;
            elem.querySelector('div[data-avatar]').setAttribute('data-avatar', `${e.from}`);
        }
        elem.querySelector('strong[data-message-to]').innerHTML = e.to;
        elem.querySelector('strong[data-message-date]').innerHTML = formatMessageDate(e.when_written_time);
        elem.querySelector('span[data-upvote-count]').innerHTML = e.upvotes;
        elem.querySelector('span[data-downvote-count]').innerHTML = e.downvotes;
        elem.querySelector('div[data-message-body]').innerHTML = e.body;
        elem.querySelector('a[data-direct-link]').setAttribute('href', `#${e.number}`);
        elem.removeAttribute('hidden');
        document.getElementById('forum-list-container').appendChild(elem);
        if (users.indexOf(akey) < 0) users.push(akey);
    });

    dlmm.removeAttribute('hidden');
    blmm.removeAttribute('disabled');

    if (Avatars) Avatars.draw(users);

}

function onThreadStats(data) {

    Object.entries(data).forEach(([k, v]) => {

        if (k == 'sub' || k == 'scan_cfg') return;

        let cache = JSON.parse(localStorage.getItem(`${data.sub}-threadList`));
        if (cache) {
            const idx = cache.threads.findIndex(e => e.id == k);
            if (idx > -1) {
                cache.total += (v.messages - cache.threads[idx].messages);
                cache.threads[idx].last.from = v.last.from;
                cache.threads[idx].last.when_written_time = v.last.when_written_time;
                cache.threads[idx].messages = v.messages;
                cache.threads[idx].unread = v.unread;
                cache.threads[idx].votes = v.votes;
            }
            localStorage.setItem(`${data.sub}-threadList`, JSON.stringify(cache));
        }

        const elem = document.getElementById(`forum-thread-link-${k}`);
        if (elem === null) return;

        const replies = elem.querySelector('div[data-replies]');
        if (v.total > 1) {
            replies.querySelector('strong[data-message-count]').innerHTML = v.messages - 1;
            if (v.messages == 2) {
                replies.querySelector('span[data-suffix-reply]').removeAttribute('hidden');
            } else {
                replies.querySelector('span[data-suffix-replies]').removeAttribute('hidden');
            }
            replies.querySelector('strong[data-last-from]').innerHTML = v.last.from;
            replies.querySelector('span[data-last-time]').innerHTML = formatMessageDate(v.last.when_written_time);
            replies.removeAttribute('hidden');
        }

        const stats = elem.querySelector('div[data-stats]');
        if (v.unread) {
			const urm = stats.querySelector('span[data-unread-messages]');
			if (urm !== null) { // If user is guest, this element will not exist
                urm.innerHTML = v.unread;
                if (data.scan_cfg&(1<<1) || data.scan_cfg&5 || data.scan_cfg&(1<<8)) {
                    urm.classList.add('scanned');
                } else {
                    urm.classList.remove('scanned');
                }
				urm.removeAttribute('hidden');
				stats.removeAttribute('hidden');
			}
        }

		if (v.votes.total) {
			if (v.votes.up.t) {
				stats.querySelector('span[data-upvotes]').innerHTML = `${v.votes.up.p}/${v.votes.up.t}`;
				stats.querySelector('span[data-upvotes-badge]').style.setProperty('display', '');
			}
			if (v.votes.down.t) {
				stats.querySelector('span[data-downvotes]').innerHTML = `${v.votes.down.p}/${v.votes.down.t}`;
				stats.querySelector('span[data-downvotes-badge]').style.setProperty('display', '');
			}
			stats.removeAttribute('hidden');
		}

    });

}

async function listThreads(sub, count, after) {

    const dlmt = document.getElementById('forum-load-more-threads');
    const blmt = document.getElementById('load-more-threads');
    dlmt.setAttribute('hidden', true);
    blmt.setAttribute('disabled', true);

    const lm = new LoadingMessage();
    lm.start();
    let _data;
    let data = JSON.parse(localStorage.getItem(`${sub}-threadList`));
    if (data === null) {
        data = await v4_get(`./api/forum.ssjs?call=list-threads&sub=${sub}&count=${count}`);
    } else if (after) {
        _data = await v4_get(`./api/forum.ssjs?call=list-threads&sub=${sub}&count=${count}&after=${data.threads[data.threads.length - 1].id}`);
        data.total = _data.total;
        data.threads = data.threads.concat(_data.threads);
    }
    localStorage.setItem(`${sub}-threadList`, JSON.stringify(data));
    lm.stop();

    (_data || data).threads.forEach(e => {

        const elem = document.getElementById('forum-thread-link-template').cloneNode(true);
        elem.id = elem.id.replace(/template$/, e.id);
        elem.setAttribute('href', `${elem.getAttribute('href')}&thread=${e.id}`);
        elem.querySelector('strong[data-thread-subject]').innerHTML = e.subject;
        elem.querySelector('strong[data-thread-from]').innerHTML = e.first.from;
        elem.querySelector('span[data-thread-date-start]').innerHTML = formatMessageDate(e.first.when_written_time);

		if (e.messages > 1) {
			elem.querySelector('strong[data-message-count]').innerHTML = e.messages - 1;
			if (e.messages == 2) {
				elem.querySelector('span[data-suffix-reply]').removeAttribute('hidden');
			} else {
				elem.querySelector('span[data-suffix-replies]').removeAttribute('hidden');
			}
			elem.querySelector('strong[data-last-from]').innerHTML = e.last.from;
            elem.querySelector('span[data-last-time]').innerHTML = formatMessageDate(e.last.when_written_time);
            elem.querySelector('div[data-replies]').removeAttribute('hidden');
		}

        const stats = elem.querySelector('div[data-stats]');
        if (e.unread) {
			const urm = stats.querySelector('span[data-unread-messages]');
			if (urm !== null) { // If user is guest, this element will not exist
                urm.innerHTML = e.unread;
                if (data.scan_cfg&(1<<1) || data.scan_cfg&5 || data.scan_cfg&(1<<8)) {
                    urm.classList.add('scanned');
                } else {
                    urm.classList.remove('scanned');
                }
				urm.removeAttribute('hidden');
				stats.removeAttribute('hidden');
			}
        }
		if (e.votes.total) {
			if (e.votes.up.t) {
				stats.querySelector('span[data-upvotes]').innerHTML = `${e.votes.up.p}/${e.votes.up.t}`;
				stats.querySelector('span[data-upvotes-badge]').style.setProperty('display', '');
			}
			if (e.votes.down.t) {
				stats.querySelector('span[data-downvotes]').innerHTML = `${e.votes.down.p}/${e.votes.down.t}`;
				stats.querySelector('span[data-downvotes-badge]').style.setProperty('display', '');
			}
			stats.removeAttribute('hidden');
		}

        elem.removeAttribute('hidden');
        document.getElementById('forum-list-container').appendChild(elem);

    });

    dlmt.removeAttribute('hidden');
    blmt.removeAttribute('disabled');

}

async function listGroups() {

    loading(true);
    const data = await v4_get(`./api/forum.ssjs?call=list-groups`);
    loading(false);

    data.forEach(e => {
        const elem = document.getElementById('forum-group-link-template').cloneNode(true);
        elem.id = elem.id.replace(/template$/, e.index);
        elem.setAttribute('href', `${elem.getAttribute('href')}&group=${e.index}`);
        elem.querySelector('strong[data-group-name]').innerHTML = e.name;
        elem.querySelector('span[data-unread-unscanned]').innerHTML = '';
        elem.querySelector('span[data-unread-scanned]').innerHTML = '';
        elem.querySelector('span[data-group-description]').innerHTML = e.description;
        elem.querySelector('span[data-group-sub-count]').innerHTML = e.sub_count;
        document.getElementById('forum-list-container').appendChild(elem);
    });

}

async function listSubs(group) {

    loading(true);
    const data = await v4_get(`./api/forum.ssjs?call=list-subs&group=${group}`);
    loading(false);

    data.forEach(e => {
        const elem = document.getElementById('forum-sub-link-template').cloneNode(true);
        elem.id = elem.id.replace(/template$/, e.code);
        elem.setAttribute('href', `${elem.getAttribute('href')}&sub=${e.code}`);
        elem.querySelector('strong[data-sub-name]').innerHTML = e.name;
        elem.querySelector('p[data-sub-description]').innerHTML = e.description;
        document.getElementById('forum-list-container').appendChild(elem);
    });

}

function onNewestSubMessage(sub, msg) {
    const elem = document.getElementById(`forum-sub-link-${sub}`);
    elem.querySelector('strong[data-newest-message-subject]').innerHTML = msg.subject;
    elem.querySelector('span[data-newest-message-from]').innerHTML = msg.from;
    elem.querySelector('span[data-newest-message-date]').innerHTML = formatMessageDate(msg.date);
    elem.querySelector('span[data-newest-message-container]').removeAttribute('hidden');
}

async function getNewestMessagePerSub(group) {
    const data = await v4_get(`./api/forum.ssjs?call=get-newest-message-per-sub&group=${group}`);
    Object.entries(data).forEach(([k, v]) => {
        onNewestSubMessage(k, v)
    });
}

function formatMessageDate(t) {
    return (new Date(t * 1000)).toLocaleString();
}

function onGroupUnreadCount(data) {
    Object.entries(data).forEach(([k, v]) => {
        const elem = document.getElementById(`forum-group-link-${k}`);
        if (v.total - v.scanned > 0) elem.querySelector('span[data-unread-unscanned]').innerHTML = v.total - v.scanned;
        if (v.scanned > 0) elem.querySelector('span[data-unread-scanned]').innerHTML = v.scanned;
    });
}

function onSubUnreadCount(data) {
    Object.entries(data).forEach(([k, v]) => {
        const elem = document.getElementById(`forum-sub-link-${k}`);
        if (v.total - v.scanned > 0) elem.querySelector('span[data-unread-unscanned]').innerHTML = v.total - v.scanned;
        if (v.scanned > 0) elem.querySelector('span[data-unread-scanned]').innerHTML = v.scanned;
        if (v.newest) onNewestSubMessage(k, v.newest);
    });
}
