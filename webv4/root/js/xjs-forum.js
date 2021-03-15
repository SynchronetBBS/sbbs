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
    let x = 0;
    let y = 0;
    let _x = 0;
    let _y = 0;
    let fg = 7;
    let bg = 0;
    let high = 0;
    let match;
    let opts;
    const lifo = [];
    const data = [[]];
    const re = /^((?<ansi>\u001b\[((?:[\x30-\x3f]{0,2};?)*)[\x20-\x2f]*([\x40-\x7c]))|(\x01(?<ctrl_a>.))|(@(?<pcboard_bg>[a-fA-F0-9])(?<pcboard_fg>[a-fA-F0-9])@{0,1})|(\|(?<pipe>\d\d))|(\x03(?<wwiv>[0-9]))|(\|(?<celerity>[kbgcrmywdBGCRMYWS])))/;

    const ANSI_Colors = [
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

    const Codes = {
        ctrl_a: {
            K: { fg: 0 },
            R: { fg: 1 },
            G: { fg: 2 },
            Y: { fg: 3 },
            B: { fg: 4 },
            M: { fg: 5 },
            C: { fg: 6 },
            W: { fg: 7 },
            0: { bg: 0 },
            1: { bg: 1 },
            2: { bg: 2 },
            3: { bg: 3 },
            4: { bg: 4 },
            5: { bg: 5 },
            6: { bg: 6 },
            7: { bg: 7 },
            H: { high: 1 },
            N: { high: 0 },
            '-': { // optimized normal
                handler: () => {
                    if (lifo.length) {
                        const attr = lifo.pop();
                        fg = attr.fg;
                        bg = attr.bg;
                        high = attr.high;
                    } else {
                        high = 0;
                    }
                }
            },
            L: { // Clear the screen
                handler: () => data = [[]],
            },
            J: { // Clear to end of screen but keep cursor in place
                handler: () => {
                    for (let yy = y; yy < data.length; yy++) {
                        if (data[yy] === undefined) continue;
                        for (let xx = x + 1; xx < data[yy].length; xx++) {
                            if (data[yy][xx] === undefined) continue;
                            data[yy][xx] = { c: ' ', fg: fg + (high ? 8 : 0), bg }; // Could just make these undefined I guess
                        }
                    }
                },
            },
            '>': { // Clear to end of line but keep cursor in place
                handler: () => {
                    for (let xx = x; xx < data[y].length; xx++) {
                        if (data[y][xx] === undefined) continue;
                        data[y][xx] = { c: ' ', fg: fg + (high ? 8 : 0), bg }; // Could just make these undefined I guess
                    }
                },
            },
            '<': { // Cursor left
                handler: () => {
                    if (x > 0) {
                        x--;
                    } else if (y > 0) { // Not sure if this is what would happen on terminal side; must test.
                        x = 79;
                        y--;
                    }
                },
            },
            "'": {
                handler: () => {
                    x = 0;
                    y = 0;
                },
            },
            '[': { handler: () => x = 0 }, // CR
            ']': { // LF
                handler: () => {
                    y++;
                    if (data[y] === undefined) data[y] = [];
                },
            }, 
            '/': { // Conditional newline - Send a new-line sequence (CRLF) only when the cursor is not already in the first column
                handler: () => {
                    if (x < 1) return;
                    this['['].handler();
                    this[']'].handler();
                },
            }, 
            '+': { handler: () => lifo.push({ fg, bg, high }) },
            // '_': {}, // optimized normal, but only if blinking or (high?) background is set
            // I: {}, // blink
            // E: {}, // bright bg (ice)
            // f: {}, // blink font
            // F: {}, // high blink font
            // D: { // current date in mm/dd/yy or dd/mm/yy
            //     handler: () => {},
            // },
            // T: { // current system time in hh:mm am/pm or hh:mm:ss format
            //     handler: () => {},
            // },
            // '"': {}, // Display a file
        },
        pcboard_bg: {
            0: { bg: 0 },
            1: { bg: 4 },
            2: { bg: 2 },
            3: { bg: 6 },
            4: { bg: 1 },
            5: { bg: 5 },
            6: { bg: 3 },
            7: { bg: 7 },
            // 8-F are blinking fg
        },
        pcboard_fg: {
            0: {
                fg: 0,
                high: 0,
            },
            1: {
                fg: 4,
                high: 0,
            },
            2: {
                fg: 2,
                high: 0,
            },
            3: {
                fg: 6,
                high: 0,
            },
            4: {
                fg: 1,
                high: 0,
            },
            5: {
                fg: 5,
                high: 0,
            },
            6: {
                fg: 3,
                high: 0,
            },
            7: {
                fg: 7,
                high: 0,
            },
            8: {
                fg: 0,
                high: 1,
            },
            9: {
                fg: 4,
                high: 1,
            },
            A: {
                fg: 2,
                high: 1,
            },
            B: {
                fg: 6,
                high: 1,
            },
            C: {
                fg: 1,
                high: 1,
            },
            D: {
                fg: 5,
                high: 1,
            },
            E: {
                fg: 3,
                high: 1,
            },
            F: {
                fg: 7,
                high: 1,
            },
        },
        pipe: {
            00: {
                fg: 0,
                high: 0,
            },
            01: {
                fg: 4,
                high: 0,
            },
            02: {
                fg: 2,
                high: 0,
            },
            03: {
                fg: 6,
                high: 0,
            },
            04: {
                fg: 1,
                high: 0,
            },
            05: {
                fg: 5,
                high: 0,
            },
            06: {
                fg: 3,
                high: 0,
            },
            07: {
                fg: 7,
                high: 0,
            },
            08: {
                fg: 0,
                high: 1,
            },
            09: {
                fg: 4,
                high: 1,
            },
            10: {
                fg: 2,
                high: 1,
            },
            11: {
                fg: 6,
                high: 1,
            },
            12: {
                fg: 1,
                high: 1,
            },
            13: {
                fg: 5,
                high: 1,
            },
            14: {
                fg: 3,
                high: 1,
            },
            15: {
                fg: 7,
                high: 1,
            },
            16: {
                bg: 0,
                high: 0,
            },
            17: {
                bg: 4,
                high: 0,
            },
            18: {
                bg: 2,
                high: 0,
            },
            19: {
                bg: 6,
                high: 0,
            },
            20: {
                bg: 1,
                high: 0,
            },
            21: {
                bg: 5,
                high: 0,
            },
            22: {
                bg: 3,
                high: 0,
            },
            23: {
                bg: 7,
                high: 0,
            },
            // 24 - 31 are ice bg or blinking fg
        },
        wwiv: {
            0: {
                fg: 7,
                bg: 0,
                high: 0,
            },
            1: {
                fg: 6,
                bg: 0,
                high: 1,
            },
            2: {
                fg: 3,
                bg: 0,
                high: 1,
            },
            3: {
                fg: 5,
                bg: 0,
                high: 0,
            },
            4: {
                fg: 7,
                bg: 4,
                high: 1,
            },
            5: {
                fg: 2,
                bg: 0,
                high: 0,
            },
            6: { // Supposed to blink, but whatever.
                fg: 2,
                bg: 0,
                high: 1,
            },
            7: {
                fg: 4,
                bg: 0,
                high: 1,
            },
            8: {
                fg: 4,
                bg: 0,
                high: 0,
            },
            9: {
                fg: 6,
                bg: 0,
                high: 0,
            },
        },
        celerity: {
            k: {
                fg: 0,
                high: 0,
            },
            b: {
                fg: 4,
                high: 0,
            },
            g: {
                fg: 2,
                high: 0,
            },
            c: {
                fg: 6,
                high: 0,
            },
            r: {
                fg: 1,
                high: 0,
            },
            m: {
                fg: 5,
                high: 0,
            },
            y: {
                fg: 3,
                high: 0,
            },
            w: {
                fg: 7,
                high: 0,
            },
            d: {
                fg: 0,
                high: 1,
            },
            B: {
                fg: 4,
                high: 1,
            },
            G: {
                fg: 2,
                high: 1,
            },
            C: {
                fg: 6,
                high: 1,
            },
            R: {
                fg: 1,
                high: 1,
            },
            M: {
                fg: 5,
                high: 1,
            },
            Y: {
                fg: 3,
                high: 1,
            },
            W: {
                fg: 7,
                high: 1,
            },
            S: {
                handler: () => {
                    const _fg = fg;
                    fg = bg;
                    bg = _fg;
                },
            },
        },
    };
    Object.values(Codes).forEach(v => {
        Object.keys(v).forEach(k => {
            if (v[k.toUpperCase()] === undefined) v[k.toUpperCase()] = v[k];
            if (v[k.toLowerCase()] === undefined) v[k.toLowerCase()] = v[k];
        });
    });

    while (body.length) {
        match = re.exec(body);
        if (match !== null) {
            body = body.substr(match[0].length);
            if (match.groups.ansi !== undefined) {
                opts = match[3].split(';').map(e => parseInt(e, 10));
                switch (match[4]) {
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
                            if (isNaN(o)) continue;
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
            } else {
                Object.keys(match.groups).forEach(e => {
                    if (match.groups[e] === undefined) return;
                    if (Codes[e] === undefined || Codes[e][match.groups[e]] === undefined) return;
                    if (Codes[e][match.groups[e]].fg !== undefined) fg = Codes[e][match.groups[e]].fg;
                    if (Codes[e][match.groups[e]].bg !== undefined) bg = Codes[e][match.groups[e]].bg;
                    if (Codes[e][match.groups[e]].high !== undefined) high = Codes[e][match.groups[e]].high;
                    if (Codes[e][match.groups[e]].handler !== undefined) Codes[e][match.groups[e]].handler();
                });
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
        if (data[y] === undefined) continue;
        for (let x = 0; x < data[y].length; x++) {
            if (data[y][x]) {
                if (!span || data[y][x].fg != ofg || data[y][x].bg != obg) {
                    ofg = data[y][x].fg;
                    obg = data[y][x].bg;
                    span = document.createElement('span');
                    span.style.setProperty('color', ANSI_Colors[data[y][x].fg]);
                    span.style.setProperty('background-color', ANSI_Colors[data[y][x].bg]);
                    pre.appendChild(span);
                }
                span.innerText += data[y][x].c;
            } else {
                if (!span || ofg !== 7 || obg !== 0) {
                    span = document.createElement('span');
                    span.style.setProperty('color', ANSI_Colors[7]);
                    span.style.setProperty('background-color', ANSI_Colors[0]);
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
        if (e.newest) showNewestMessage(elem, e.newest);
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
