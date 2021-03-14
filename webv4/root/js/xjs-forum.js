// Deuce's URL-ifier
function linkify(body) {
    urlRE = /(?:https?|ftp|telnet|ssh|gopher|rlogin|news):\/\/[^\s'"'<>()]*|[-\w.+]+@(?:[-\w]+\.)+[\w]{2,6}/gi;
    body = body.replace(urlRE, function (str) {
        var ret = '';
        var p = 0;
        var link = str.replace(/\.*$/, '');
        var linktext = link;
        if (link.indexOf('://') === -1) link = 'mailto:' + link;
        return ('<a class="ulLink" href="' + link + '">' + linktext + '</a>' + str.substr(linktext.length));
    });
    return body;
}

// Somewhat modified version of Deuce's "magical quoting stuff" from v3
function quotify(body) {

    const blockquote_start = '<blockquote>';
    const blockquote_end = '</blockquote>';

    let quote_depth = 0;
    let prefixes = [];

    const ret = body.split(/\r?\n/).reduce((a, c) => {
        let line = '';
        let line_prefix = '';
        const m = c.match(/^((?:\s?[^\s]{0,3}>\s?)+)/);
        if (m !== null) {
            let p;
            let broken = false;            
            let new_prefixes = m[1].match(/\s?[^\s]{0,3}>\s?/g);
            line = c;
            // If the new length is smaller than the old one, close the extras
            for (p = new_prefixes.length; p < prefixes.length; p++) {
                if (quote_depth < 1) continue;
                line_prefix = line_prefix + blockquote_end;
                quote_depth--;
            }
            for (p in new_prefixes) {
                // Remove prefix from start of line
                line = line.substr(new_prefixes[p].length);
                if (prefixes[p] === undefined) {
                    /* New depth */
                    line_prefix = line_prefix + blockquote_start;
                    quote_depth++;
                } else if (broken) {
                    line_prefix = line_prefix + blockquote_start;
                    quote_depth++;
                } else if (prefixes[p].replace(/^\s*(.*?)\s*$/, '$1') != new_prefixes[p].replace(/^\s*(.*?)\s*$/, '$1')) {
                    // Close all remaining old prefixes and start one new one
                    for (let o = p; o < prefixes.length && o < new_prefixes.length; o++) {
                        if (quote_depth > 0) {
                            line_prefix = blockquote_end + line_prefix;
                            quote_depth--;
                        }
                    }
                    line_prefix = blockquote_start + line_prefix;
                    quote_depth++;
                    broken = true;
                }
            }
            prefixes = new_prefixes.slice();
            line = line_prefix + line;
        } else {
            for (p = 0; p < prefixes.length; p++) {
                if (quote_depth < 1) continue;
                line_prefix = line_prefix + blockquote_end;
                quote_depth--;
            }
            prefixes = [];
            line = line_prefix + c;
        }
        return a + line + '\r\n';
    }, '');

    if (quote_depth !== 0) {
        for (;quote_depth > 0; quote_depth--) {
            ret += blockquote_end;
        }
    }

    return ret.replace(/\<\/blockquote\>\r\n<blockquote\>/g, '\r\n');

}

// Format message body for the web
function formatMessageBody(body) {
    // Strip CTRL-A
    body = body.replace(/\1./g,'');
    // Strip ANSI
    body = body.replace(/\x1b\[[\x30-\x3f]*[\x20-\x2f]*[\x40-\x7e]/g,'');
    body = body.replace(/\x1b[\x40-\x7e]/g, '');
    // Strip unprintable control chars (NULL, BEL, DEL, ESC)
    body = body.replace(/[\x00\x07\x1b\x7f]/g,'');
    // Format for the web
    // body = word_wrap(body, body.length); // This was done server-side; but why was it done at all?
    body = quotify(body);
    body = linkify(body);
    body = body.replace(/\r\n$/,'');
    body = body.replace(/(\r?\n)/g, "<br>$1");
    return body;
}

function formatMessageDate(t) {
    return (new Date(t * 1000)).toLocaleString();
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

async function addNew(sub) {

    if (document.getElementById('newmessage') !== null) return;

    const elem = document.querySelector('div[data-new-message-template').cloneNode(true);
    elem.id = 'newmessage';

    const li = document.createElement('li');
    li.id = 'newmessage-li';
    li.className = 'list-group-item';
    li.appendChild(elem);
    document.getElementById('forum-list-container').appendChild(li);

    const data = await v4_get('./api/forum.ssjs?call=get-signature');
    const nmb = elem.getElementsByTagName('textarea')[0];
    nmb.value += `\r\n${data}`;
    nmb.setSelectionRange(0, 0);

    elem.removeAttribute('hidden');

    window.location.hash = '#newmessage';
	nmb.onkeydown = evt => evt.stopImmediatePropagation();

}

async function postNew(sub) {

    const elem = document.getElementById('newmessage');
    elem.querySelector('input[data-new-message-submit]').disabled = true;

	const data = await v4_post('./api/forum.ssjs', {
		call: 'post',
		sub,
		to: elem.querySelector('input[data-new-message-to]').value,
		subject: elem.querySelector('input[data-new-message-subject]').value,
		body: elem.querySelector('textarea[data-new-message-body]').value,
	});

    if (data.success) {
        const li = document.getElementById('newmessage-li');
        li.remove();
		insertParam('notice', 'Your message has been posted.'); // This is stupid. Perhaps 'data' should contain the header etc. to be appended
	}

}

async function addReply(sub, id, body, parentElem) {

    if (document.getElementById(`replybox-${id}`) !== null) return;
    parentElem.querySelector('button[data-add-reply]').disabled = true;

    const elem = document.querySelector('div[data-reply-message-template]').cloneNode(true);
    elem.id = `replybox-${id}`;

    const nmb = elem.querySelector('textarea[data-reply-message-body]');
	const data = await v4_get('./api/forum.ssjs?call=get-signature');
    nmb.value += `\r\n${data}`;
    nmb.setSelectionRange(0, 0);
    nmb.onkeydown = evt => evt.stopImmediatePropagation();

    const qb = elem.querySelector('button[data-quote-message-button');
    qb.onclick = () => {
        nmb.focus();
        nmb.setRangeText(`> ${body.split(/\r*\n/).join('\r\n> ')}\r\n`, nmb.selectionStart, nmb.selectionEnd, 'end');
        // cursor positioning is a bit wonky here; needs work
    };

    const sb = elem.querySelector('input[data-reply-message-submit]');
    sb.onclick = (evt) => {
        evt.preventDefault();
        postReply(sub, id, nmb.value, parentElem, elem);
    }

    parentElem.appendChild(elem);
    elem.removeAttribute('hidden');

}

async function postReply(sub, id, body, parentElem, replyElem) {
    replyElem.querySelector('input[data-reply-message-submit]').disabled = true;
    const qb = replyElem.querySelector('button[data-quote-message-button');
    qb.disabled = true;
	const data = await v4_post('./api/forum.ssjs', {
		call: 'post-reply',
		sub,
		body: body,
		pid: id,
	});
	if (data.success) {
        qb.disabled = false;
        replyElem.remove();
		insertParam('notice', 'Your message has been posted.'); // This is stupid. As with postNew, just append the message to the view
	} else {
        parentElem.querySelector('button[data-add-reply]').disabled = false;
	}
}

async function postNewPoll(sub) {

    document.getElementById('newpoll-submit').disabled = true;

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
        np.remove();
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


function renderBBSView(body) {

    const ANSI_COLORS = [
        "#000000", // Black
        "#A80000", // Red
        "#00A800", // Green
        "#A85400", // Brown
        "#0000A8", // Blue
        "#A800A8", // Magenta
        "#00A8A8", // Cyan
        "#A8A8A8", // Light Grey
        "#545454", // Dark Grey (High Black)
        "#FC5454", // Light Red
        "#54FC54", // Light Green
        "#FCFC54", // Yellow (High Brown)
        "#5454FC", // Light Blue
        "#FC54FC", // Light Magenta
        "#54FCFC", // Light Cyan
        "#FFFFFF", // White
    ];
    
    let x = 0;
    let y = 0;
    let _x = 0;
    let _y = 0;
    let fg = 7;
    let bg = 0;
    let high = 0;
    let match;
    let opts;
    const re = /(?<ANSI>^\u001b\[((?:[0-9]{0,2};?)*)([a-zA-Z]))|(?<CTRLA>^\x01(.))/;
    const data = [[]];

    while (body.length) {
        match = re.exec(body);
        if (match !== null) {
            console.debug(JSON.stringify(match), match.groups, match.groups.ANSI);
            body = body.substr(match[0].length);
            if (match.groups.ANSI !== undefined) {
                opts = match[1].split(';').map(e => parseInt(e, 10));
                switch (match[2]) {
                    case 'A':
                        y = Math.max(y - (isNaN(opts[0]) ? 1 : opts[0]), 0);
                        break;
                    case 'B':
                        y += (isNaN(opts[0]) ? 1 : opts[0]);
                        if (data[y] === undefined) data[y] = [];
                        break;
                    case 'C':
                        x = Math.min(x + (isNaN(opts[0]) ? 1 : opts[0]), 79);
                        break;
                    case 'D':
                        x = Math.max(x - (isNaN(opts[0]) ? 1 : opts[0]), 0);
                        break;
                    case 'f':
                    case 'H':
                        y = isNaN(opts[0]) ? 1 : opts[0];
                        x = isNaN(opts[1]) ? 1 : opts[0];
                        if (data[y] === undefined) data[y] = [];
                        break;
                    case 'm':
                        for (let o of opts) {
                            if (o == 0) {
                                fg = 7;
                                bg = 0;
                                high = 0;
                            } else if (o == 1) {
                                high = 1;
                            } else if (o == 5) {
                                // blink
                            } else if (o >= 30 && o <= 37) {
                                fg = o - 30;
                            } else if (o >= 40 && o <= 47) {
                                bg = o - 40;
                            }
                        }
                        break;
                    case 's': // push xy
                        _x = x;
                        _y = y;
                        break;
                    case 'u': // pop xy
                        x = _x;
                        y = _y;
                        break;
                    case 'J':
                        if (opts.length == 1 && opts[0] == 2) {
                            for (let yy = 0; yy < data.length; yy++) {
                                if (!Array.isArray(data[yy])) data[yy] = [];
                                for (let xx = 0; xx < 80; xx++) {
                                    data[yy][xx] = { c: ' ', fg: fg + (high ? 8 : 0), bg };
                                }
                            }
                        }
                        break;
                    case 'K':
                        for (let xx = 0; xx < 80; xx++) {
                            data[y][xx] = { c: ' ', fg: fg + (high ? 8 : 0), bg };
                        }
                        break;
                    default:
                        // Unknown or unimplemented command
                        break;
                }
            } else if (match.groups.CTRLA !== undefined) {
                switch (match[5]) {
                    case 'K':
                    case 'k':
                        fg = 0;
                        break;
                    case 'R':
                    case 'r':
                        fg = 1;
                        break;
                    case 'G':
                    case 'g':
                        fg = 2;
                        break;
                    case 'Y':
                    case 'y':
                        fg = 3;
                        break;
                    case 'B':
                    case 'b':
                        fg = 4;
                        break;
                    case 'M':
                    case 'm':
                        fg = 5;
                        break;
                    case 'C':
                    case 'c':
                        fg = 6;
                        break;
                    case 'W':
                    case 'w':
                        fg = 7;
                        break;
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                        bg = parseInt(match[5], 10);
                        break;
                    case 'H':
                        high = 1;
                        break;
                    case 'I': // blink
                    case 'E': // bright bg (ice)
                    case 'f': // blink font
                    case 'F': // high blink font
                        break;
                    case 'N':
                        high = 0;
                        break;
                    case '-': // optimized normal                        
                        // we need to support pushing current attributes (\1+), then use this to pop them
                        // or if nothing has been pushed, then unset any special attributes (only 'high' for now)
                        break;
                    case '_': // optimized normal
                        // when he says "the Background attribute", does he mean Bright-Background, or just any background colour?
                        break;
                    case 'L': // Clear the screen
                        // Same as ANSI parser case J? Easier to preserve the arrays since we're not homing the cursor
                        break;
                    case "'": // Home the cursor
                        x = 0;
                        y = 0;
                        break;
                    case 'J':
                    case 'j': // Clear to end of screen but keep cursor in place
                        break;
                    case '>': // Clear to end of line but keep cursor in place
                        break;
                    case '<': // Cursor left
                        if (x > 0) {
                            x--;
                        } else if (y > 0) { // Not sure if this is what would happen on terminal side; must test.
                            x = 79;
                            y--;
                        }
                        break;
                    case '[': // CR
                        x = 0;
                        break;
                    case ']': // LF
                        y++;
                        if (data[y] === undefined) data[y] = [];
                        break;
                    case '/': // Conditional newline - Send a new-line sequence (CRLF) only when the cursor is not already in the first column
                        if (x > 0) {
                            x = 0;
                            y++;
                            if (data[y] === undefined) data[y] = [];
                        }
                        break;
                    case '+': // push current attributes onto lifo stack
                        break;
                    case 'D':
                    case 'd': // current date in mm/dd/yy or dd/mm/yy (should be system date & format, but we'll use browser date & locale)
                        break;
                    case 'T':
                    case 't': // current system time in hh:mm am/pm or hh:mm:ss format (via browser, as with \1D)
                        break;
                    case '"': // Display a file
                        // the following string would be a filename from the 'text' directory
                        // I guess we could support this and make a request for the file
                        // but this isn't needed as long as we're only using this function for the forum
                        break;
                    default:
                        // if parseInt(match[5], 10) > 127 and < 256 then move cursor right by that many spaces, wrap at col 80
                        // Unknown or unhandled CTRL-A code
                        break;
                }
            }
        } else {
            let ch = body.substr(0, 1);
            switch (ch) {
                case '\x1a':
                    body = '';
                    break;
                case '\n':
                    y++;
                    if (data[y] === undefined) data[y] = [];
                    break;
                case '\r':
                    x = 0;
                    break;
                default:
                    data[y][x] = { c: ch, fg: fg + (high ? 8 : 0), bg };
                    x++;
                    if (x > 79) {
                        x = 0;
                        y++;
                        if (data[y] === undefined) data[y] = [];
                    }
                    break;
            }
            body = body.substr(1);
        }
    }

    const pre = document.createElement('div');
    pre.classList.add('bbs-view');
    let ofg;
    let obg;
    let span;
    for (let y = 0; y < data.length; y++) {
        for (let x = 0; x < data[y].length; x++) {
            if (data[y][x]) {
                if (!span || data[y][x].fg != ofg || data[y][x].bg != obg) {
                    ofg = data[y][x].fg;
                    obg = data[y][x].bg;
                    span = document.createElement('span');
                    span.style.setProperty('color', ANSI_COLORS[data[y][x].fg]);
                    span.style.setProperty('background-color', ANSI_COLORS[data[y][x].bg]);
                    pre.appendChild(span);
                }
                span.innerText += data[y][x].c;
            } else {
                if (!span || ofg !== 7 || obg !== 0) {
                    span = document.createElement('span');
                    span.style.setProperty('color', ANSI_COLORS[7]);
                    span.style.setProperty('background-color', ANSI_COLORS[0]);
                    pre.appendChild(span);
                }
                span.innerText += ' ';
            }
        }
        span = null;
        pre.innerHTML += '\r\n';
    }
    return pre;

}

function bbsView(elem, body) {
    const btn = elem.querySelector('button[data-button-bbs-view]');
    btn.disabled = true;
    const pre = renderBBSView(body);
    const target = elem.querySelector('div[data-message-body]')
    target.innerHTML = '';
    target.appendChild(pre);
    btn.onclick = () => {
        target.innerHTML = formatMessageBody(body);
        btn.onclick = () => bbsView(elem, body);
    }
    btn.disabled = false;
}

// Message list
async function listMessages(sub, thread) {

    const lm = new LoadingMessage();
    lm.start();
    const data = await v4_fetch_jsonl(`./api/forum.ssjs?call=get-thread&sub=${sub}&thread=${thread}`);
    lm.stop();

    // TO DO: what about poll messages? If they may show up in (_data || data) then they need to be handled differently and a template created in forum.xjs
    const users = [];
    data.forEach((e, i) => {
        let akey;
        let elem;
        let append = false;
        const elemId = `forum-message-${e.number}`;
        if ((elem = document.getElementById(elemId)) === null) {
            elem = document.getElementById('forum-message-template').cloneNode(true);
            elem.id = elemId;
            elem.setAttribute('data-message', e.number);
            append = true;
        }
        elem.querySelector('a[data-message-anchor]').id = e.number;
        if (i == 0 && e.subject !== undefined) {
            document.querySelector('span[data-message-subject]').innerHTML = e.subject; // Breadcrumb link to thread
            elem.querySelector('strong[data-message-subject]').innerHTML = e.subject;
        }
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
        elem.querySelector('span[data-upvote-count]').innerHTML = e.votes.up;
        elem.querySelector('span[data-downvote-count]').innerHTML = e.votes.down;
        elem.querySelector('div[data-message-body]').innerHTML = formatMessageBody(e.body);
        elem.querySelector('button[data-button-bbs-view]').onclick = evt => bbsView(elem, e.body);
        elem.querySelector('a[data-direct-link]').setAttribute('href', `#${e.number}`);
        elem.querySelector('button[data-button-add-reply]').onclick = evt => addReply(sub, e.number, e.body, elem);
        elem.removeAttribute('hidden');
        if (append) document.getElementById('forum-list-container').appendChild(elem);
        if (users.indexOf(akey) < 0) users.push(akey);
    });

    if (Avatars) Avatars.draw(users);

}


// Thread list

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

async function listThread(e) {

    let elem;
    let append = false;
    const elemId = `forum-thread-link-${e.id}`;

    if ((elem = document.getElementById(elemId)) === null) {
        elem = document.getElementById('forum-thread-link-template').cloneNode(true);
        elem.id = elemId;
        elem.setAttribute('data-thread', e.id);
        elem.setAttribute('href', `${elem.getAttribute('href')}&thread=${e.id}`);
        append = true;
    }

    elem.querySelector('strong[data-thread-subject]').innerHTML = e.first_message.subject;
    elem.querySelector('strong[data-thread-from]').innerHTML = e.first_message.from;
    elem.querySelector('span[data-thread-date-start]').innerHTML = formatMessageDate(e.first_message.when_written_time);

    if (e.messages > 1) {
        elem.querySelector('strong[data-message-count]').innerHTML = e.messages - 1;
        if (e.messages == 2) {
            elem.querySelector('span[data-suffix-reply]').removeAttribute('hidden');
        } else {
            elem.querySelector('span[data-suffix-replies]').removeAttribute('hidden');
        }
        elem.querySelector('strong[data-last-from]').innerHTML = e.last_message.from;
        elem.querySelector('span[data-last-time]').innerHTML = formatMessageDate(e.last_message.when_written_time);
        elem.querySelector('div[data-replies]').removeAttribute('hidden');
    }

    const stats = elem.querySelector('div[data-stats]');
    // if (e.unread) {
    //     const sub = await sbbs.forum.subs.get(e.sub);
    //     const urm = stats.querySelector('span[data-unread-messages]');
    //     if (urm !== null) { // If user is guest, this element will not exist
    //         urm.innerHTML = e.unread;
    //         if (sub.scan_cfg&(1<<1) || sub.scan_cfg&5 || sub.scan_cfg&(1<<8)) {
    //             urm.classList.add('scanned');
    //         } else {
    //             urm.classList.remove('scanned');
    //         }
    //         urm.removeAttribute('hidden');
    //         stats.removeAttribute('hidden');
    //     }
    // }

    if (e.votes.parent.up || e.votes.total.up) {
        stats.querySelector('span[data-upvotes]').innerHTML = `${e.votes.parent.up} / ${e.votes.total.up}`;
        stats.querySelector('span[data-upvotes-badge]').style.setProperty('display', '');
    }
    if (e.votes.parent.down || e.votes.total.down) {
        stats.querySelector('span[data-downvotes]').innerHTML = `${e.votes.parent.down} / ${e.votes.total.down}`;
        stats.querySelector('span[data-downvotes-badge]').style.setProperty('display', '');
    }
    
    stats.removeAttribute('hidden');

    elem.removeAttribute('hidden');
    if (append) document.getElementById('forum-list-container').appendChild(elem);

}

function onThreadList(data) {
    Object.values(data).sort((a, b) => a.last_message.when_written_time > b.last_message.when_written_time ? -1 : 1).forEach(listThread);
}

async function listThreads(sub) {
    const lm = new LoadingMessage();
    lm.start();
    const data = await v4_get(`./api/forum.ssjs?call=get-thread-list&sub=${sub}`);
    lm.stop();
    onThreadList(data);
}


// Sub list

function showNewestMessage(elem, msg) {
    elem.querySelector('strong[data-newest-message-subject]').innerHTML = msg.subject;
    elem.querySelector('span[data-newest-message-from]').innerHTML = msg.from;
    elem.querySelector('span[data-newest-message-date]').innerHTML = formatMessageDate(msg.date);
    elem.querySelector('span[data-newest-message-container]').removeAttribute('hidden');
}

async function onNewestSubMessage(sub, msg) {
    const elem = document.getElementById(`forum-sub-link-${sub}`);
    if (elem !== null) showNewestMessage(elem, msg);
}

function showSubUnreadCount(elem, s, u) { // sub link element, sub code, { total, scanned, newest }
    if (u.total - u.scanned > 0) elem.querySelector('span[data-unread-unscanned]').innerHTML = u.total - u.scanned;
    if (u.scanned > 0) elem.querySelector('span[data-unread-scanned]').innerHTML = u.scanned;
}

function onSubList(data) {
    data.sort((a, b) => a.index < b.index ? -1 : 1).forEach(e => {
        let elem;
        let append = false;
        const elemId = `forum-sub-link-${e.code}`;
        if ((elem = document.getElementById(elemId)) === null) {
            elem = document.getElementById('forum-sub-link-template').cloneNode(true);
            elem.id = elem.id.replace(/template$/, e.code);
            elem.setAttribute('href', `${elem.getAttribute('href')}&sub=${e.code}`);
            append = true; // Should see about slotting this into the document in the correct order instead, in the rare case that a new sub pops up while viewing the page I guess
        }
        elem.querySelector('strong[data-sub-name]').innerHTML = e.name;
        elem.querySelector('span[data-sub-description]').innerHTML = e.description;
        showNewestMessage(elem, e.newest);
        if (append) document.getElementById('forum-list-container').appendChild(elem);
    });
}

async function listSubs(group) {
    const lm = new LoadingMessage();
    lm.start();
    const data = await v4_get(`./api/forum.ssjs?call=list-subs&group=${group}`);
    lm.stop();
    onSubList(data);
}


// Group list

function showGroupUnreadCount(elem, u) {
    if (u.total - u.scanned > 0) elem.querySelector('span[data-unread-unscanned]').innerHTML = u.total - u.scanned;
    if (u.scanned > 0) elem.querySelector('span[data-unread-scanned]').innerHTML = u.scanned;
}

function listGroup(e) {
    let elem;
    let append = false;
    const elemId = `forum-group-link-${e.index}`;
    if ((elem = document.getElementById(elemId)) === null) {
        elem = document.getElementById('forum-group-link-template').cloneNode(true);
        elem.id = elem.id.replace(/template$/, e.index);
        elem.setAttribute('href', `${elem.getAttribute('href')}&group=${e.index}`);
        append = true;
    }
    elem.querySelector('strong[data-group-name]').innerHTML = e.name;
    elem.querySelector('span[data-unread-unscanned]').innerHTML = '';
    elem.querySelector('span[data-unread-scanned]').innerHTML = '';
    elem.querySelector('span[data-group-description]').innerHTML = e.description;
    elem.querySelector('span[data-group-sub-count]').innerHTML = e.sub_count;
    if (append) document.getElementById('forum-list-container').appendChild(elem);
}

async function listGroups() {
    const lm = new LoadingMessage();
    lm.start();
    const data = await v4_get('./api/forum.ssjs?call=list-groups');
    lm.stop();
    data.forEach(listGroup);
}
