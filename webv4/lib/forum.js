require('sbbsdefs.js', 'MSG_DELETE');
require('xjs.js', 'xjs_compile');
load(settings.web_lib + 'mime-decode.js');
load(settings.web_lib + 'avatars.js');

var avatars = new Avatars();

function listGroups() {
    const response = [];
    msg_area.grp_list.forEach(function (grp) {
        if (grp.sub_list.length < 1) return;
        response.push({
            index: grp.index,
            name: grp.name,
            description: grp.description,
            sub_count: grp.sub_list.length
        });
    });
    return response;
}

// Returns an array of objects of "useful" information about subs
function listSubs(group) {
    return msg_area.grp_list[group].sub_list.map(function (sub) {
        return {
            index: sub.index,
            code: sub.code,
            grp_index: sub.grp_index,
            grp_name: sub.grp_name,
            name: sub.name,
            description: sub.description,
            qwk_name: sub.qwk_name,
            qwk_conf: sub.qwk_conf,
            qwk_tagline: sub.qwknet_tagline,
            newsgroup: sub.newsgroup,
            ars: sub.ars,
            read_ars: sub.read_ars,
            can_read: sub.can_read,
            post_ars: sub.post_ars,
            can_post: sub.can_post,
            operator_ars: sub.operator_ars,
            is_operator: sub.is_operator,
            moderated_ars: sub.moderated_ars,
            is_moderated: sub.is_moderated,
            scan_ptr: sub.scan_ptr,
            scan_cfg: sub.scan_cfg,
        };
    });
}

function getNewestMessageInSub(sub) {
    const mb = new MsgBase(sub.code);
    if (!mb.open()) return;
    var h;
    var ret;
    for (var m = mb.last_msg; m >= mb.first_msg; m--) {
        h = mb.get_msg_header(m);
        if (h === null) continue;
        ret = {
            from: h.from,
            subject: h.subject,
            date: h.when_written_time, // should just be a timestamp; all date formatting should be client-side in forum.xjs
        };
        break;
    }
    mb.close();
    return ret;
}

function getNewestMessagePerSub(grp) {
    grp = parseInt(grp, 10);
    if (isNaN(grp) || grp < 0 || !msg_area.grp_list[grp]) return [];
    return msg_area.grp_list[grp].sub_list.reduce(function (a, c) {
        const s = getNewestMessageInSub(c);
        if (s !== undefined) a[c.code] = s;
        return a;
    }, {});
}

function getSubUnreadCount(sub) {
    var ret = {
        scanned: 0,
        total: 0,
    };
    if (msg_area.sub[sub] === undefined) return ret;
    try {
        var sy = msg_area.sub[sub].scan_cfg&SCAN_CFG_YONLY;
        var sn = msg_area.sub[sub].scan_cfg&SCAN_CFG_NEW;
        var msgBase = new MsgBase(sub);
        msgBase.open();
        for (var m = msg_area.sub[sub].scan_ptr + 1; m <= msgBase.last_msg; m++) {
            var h = msgBase.get_msg_header(m);
            if (h === null || h.attr&MSG_DELETE || h.attr&MSG_NODISP) continue;
            if ((sy && (h.to_ext === user.number || h.to === user.alias || h.to === user.name)) || sn) ret.scanned++;
            ret.total++;
            ret.newest = {
                from: h.from,
                subject: h.subject,
                date: h.when_written_time
            };
        }
        msgBase.close();
    } catch (err) {
        log(LOG_ERR, err);
    }
    return ret;
}

function getSubUnreadCounts(group) {
    return msg_area.grp_list[group].sub_list.reduce(function (a, c) {
        a[c.code] = getSubUnreadCount(c.code);
        return a;
    }, {});
}

function getGroupUnreadCount(group) {
    var ret = {
        scanned : 0,
        total : 0
    };
    if (msg_area.grp_list[group] === undefined) return ret;
    msg_area.grp_list[group].sub_list.forEach(function (sub) {
        var count = getSubUnreadCount(sub.code);
        ret.scanned += count.scanned;
        ret.total += count.total;
    });
    return ret;
}

function getGroupUnreadCounts() {
    return msg_area.grp_list.reduce(function (a, c) {
        a[c.index] = getGroupUnreadCount(c.index);
        return a;
    }, {});
}

function getUnreadInThread(sub, thread, mkeys) {
    if (typeof thread == 'number') {
        var threads = getMessageThreads(sub, settings.max_messages);
        if (threads.thread[thread] === undefined) return 0;
        thread = threads.thread[thread];
    }
    var count = 0;
    if (!mkeys) mkeys = Object.keys(thread.messages);
    mkeys.forEach(function (m) {
        if (thread.messages[m].number > msg_area.sub[sub].scan_ptr) count++;
    });
    return count;
}

function getThreadVoteTotals(thread, mkeys) {
    if (!mkeys) mkeys = Object.keys(thread.messages); // Not sure why it doesn't just do this already - does anything else call getThreadVoteTotals?
    return mkeys.reduce(function (a, c, i) {
        if (thread.messages[c].upvotes > 0) {
            if (i == 0) a.up.p++;
            a.up.t++;
        }
        if (thread.messages[c].downvotes > 0) {
            if (i == 0) a.down.p++;
            a.down.t++;
        }
        a.total = a.up.t + a.down.t;
        return a;
    }, { up: { p: 0, t: 0 }, down: { p: 0, t: 0 }, total: 0 });
}

// Called from lib/events/forum.js to scan a sub for updates
// Very similar to listThreads, but the reply is smaller and there is no paging/offset
function getThreadStats(sub, guest) {
    const threads = getMessageThreads(sub, settings.max_messages);
    const ret = {
        sub: sub.code,
        scan_cfg: sub.scan_cfg,
    };
    threads.order.forEach(function (e) {
        const thread = threads.thread[e];
        const mkeys = Object.keys(thread.messages);
        ret[e] = {
            id: e,
            last: {
                from: thread.messages[mkeys[mkeys.length - 1]].from,
                when_written_time: thread.messages[mkeys[mkeys.length - 1]].when_written_time,
            },
            messages: mkeys.length,
            unread: guest ? 0 : getUnreadInThread(sub, thread, mkeys),
            votes: getThreadVoteTotals(thread, mkeys),
        };
    });
    return ret;
}

function listThreads(sub, count, after) {

    count = parseInt(count, 10);
    if (isNaN(count) || count < 1) return false;

    var threads = getMessageThreads(sub, settings.max_messages);
    var offset = 0;
    if (after) offset = threads.order.indexOf(after) + 1;

    var msgs;
    var thread;
    var stop = Math.min(threads.order.length, offset + count);
    var ret = { total: threads.order.length, threads : [] };
    if (sub.scan_cfg&SCAN_CFG_NEW) {
        ret.scan_cfg = 'new';
    } else if (sub.scan_cfg&SCAN_CFG_YONLY) {
        ret.scan_cfg = 'you_only';
    }
    for (var n = offset; n < stop; n++) {
        thread = threads.thread[threads.order[n]];
        msgs = Object.keys(thread.messages);
        ret.threads.push({
            id: thread.id,
            subject: thread.subject,
            first: thread.messages[msgs[0]],
            last: thread.messages[msgs[msgs.length - 1]],
            messages: msgs.length,
            unread: is_user() ? getUnreadInThread(sub, thread) : 0,
            votes: getThreadVoteTotals(thread),
        });
    }

    return ret;

}

function getVotesInThread(sub, thread) {
    var ret = { t : { u : 0, d : 0 }, m : {} };
    if (msg_area.sub[sub] === undefined) return ret;
    if (typeof thread === 'number') {
        var threads = getMessageThreads(sub, settings.max_messages);
        if (threads.thread[thread] === undefined) return ret;
        thread = threads.thread[thread];
    }
    var msgBase = new MsgBase(sub);
    if (!msgBase.open()) return ret;
    Object.keys(thread.messages).forEach(function (m) {
        if (thread.messages[m].upvotes > 0 || thread.messages[m].downvotes > 0) {
            ret.t.up += thread.messages[m].upvotes;
            ret.t.down += thread.messages[m].downvotes;
            ret.m[thread.messages[m].number] = {
                u: thread.messages[m].upvotes,
                d: thread.messages[m].downvotes,
                v: msgBase.how_user_voted(thread.messages[m].number, msgBase.cfg.settings&SUB_NAME ? user.name : user.alias),
            };
        }
    });
    msgBase.close();
    return ret;
}

function getVotesInThreads(sub) {
    var threads = getMessageThreads(sub, settings.max_messages);
    var ret = {};
    Object.keys(threads.thread).forEach(function (t) {
        Object.keys(threads.thread[t].messages).forEach(function (m, i) {
            if (threads.thread[t].messages[m].upvotes < 1 && threads.thread[t].messages[m].downvotes < 1) return;
            if (ret[t] === undefined) {
                ret[t] = { p: { u: 0, d: 0 }, t: { u: 0, d: 0 } };
                if (i < 1) {
                    ret[t].p.u = threads.thread[t].messages[m].upvotes;
                    ret[t].p.d = threads.thread[t].messages[m].downvotes;
                }
            }
            ret[t].t.u += threads.thread[t].messages[m].upvotes;
            ret[t].t.d += threads.thread[t].messages[m].downvotes;
        });
    });
    return ret;
}

function getUserPollData(sub, id) {
    var ret = {
        answers: 0,
        tally: [],
        show_results: false,
    };
    if (msg_area.sub[sub] === undefined) return ret;
    id = parseInt(id);
    if (isNaN(id)) return ret;
    var msgBase = new MsgBase(sub);
    if (!msgBase.open()) return ret;
    // var header = msgBase.get_msg_header(id);
    // Temporary use of get_all_msg_headers() to get header.tally for polls -- lol, "temporary"
    var headers = msgBase.get_all_msg_headers();
    var header = null;
    for (var h in headers) {
        if (headers[h].number !== id) continue;
        header = headers[h];
        break;
    }
    // End of temporary shitfest
    if (header === null || !(header.attr&MSG_POLL)) {
        msgBase.close();
        return ret;
    }
    if (header.tally && Array.isArray(header.tally)) ret.tally = header.tally;
    ret.answers = msgBase.how_user_voted(header.number, msgBase.cfg.settings&SUB_NAME ? user.name : user.alias);
    msgBase.close();
    var pollAttr = header.auxattr&POLL_RESULTS_MASK;
    if (header.from === user.alias || header.from === user.name) {
        ret.show_results = true;
    } else if (pollAttr === POLL_RESULTS_CLOSED && header.auxattr&POLL_CLOSED) {
        ret.show_results = true;
    } else if (pollAttr === POLL_RESULTS_OPEN) {
        ret.show_results = true;
    } else if (pollAttr === POLL_RESULTS_VOTERS && ret.answers > 0) {
        ret.show_results = true;
    }
    return ret;
}

function getMailHeaders(sent, ascending) {
    if (sent !== undefined && sent && user.security.restrictions&UFLAG_K) return []; // They'll just see nothing.  Provide actual feedback?  Does anyone use REST K?
    var headers = [];
    var msgBase = new MsgBase('mail');
    if (!msgBase.open()) return headers;
    for (var m = msgBase.first_msg; m <= msgBase.last_msg; m++) {
        var h = msgBase.get_msg_header(m);
        if (h === null || h.attr&MSG_DELETE) continue;
        if ((sent !== undefined && sent) && h.from_ext != user.number) continue;
        if ((sent === undefined || !sent) && h.to_ext != user.number) continue;
        headers.push(h);
    }
    msgBase.close();
    if (ascending === undefined || !ascending) headers.reverse(); // not sure why the double !checks re: ascending and sent
    return headers;
}

function is_spam(header) {
    return (header.attr&MSG_SPAM || (header.subject.search(/^SPAM:/) > -1));
}

function get_mail_headers(filter, ascending) {
    const ret = {
        headers: [],
        sent: { read: 0, unread: 0 },
        spam: { read: 0, unread: 0 },
        inbox: { read: 0, unread: 0 },
    };
    if (filter == 'sent' && user.security.restrictions&UFLAG_K) return ret; // I don't remember what this is for.
    const msg_base = new MsgBase('mail');
    if (!msg_base.open()) return ret;
    for (var n = msg_base.first_msg; n <= msg_base.last_msg; n++) {
        var h = msg_base.get_msg_header(n);
        if (h === null || h.attr&MSG_DELETE) continue;
        if (h.from_ext == user.number) {
            h.attr&MSG_READ ? ret.sent.read++ : (ret.sent.unread++);
            if (filter == 'sent') ret.headers.push(h);
        }
    	if (h.to_ext == user.number) {
            if (is_spam(h)) {
                h.attr&MSG_READ ? ret.spam.read++ : (ret.spam.unread++);
                if (filter == 'spam') ret.headers.push(h);
            } else {
                h.attr&MSG_READ ? ret.inbox.read++ : (ret.inbox.unread++);
                if (filter == 'inbox') ret.headers.push(h);
            }
        }
    }
    msg_base.close();
    if (ascending) ret.headers.reverse();
    return ret;
}

function mimeDecode(header, body, code) {
    const ret = {
        type: '',
        body: [],
    };
    const msg = mime_decode(header, body, code);
    if (msg.inlines) {
        ret.inlines = msg.inlines.map(function (e) {
            return format(
                '<a href="./api/attachments.ssjs?sub=%s&amp;msg=%s&amp;cid=%s" target="_blank">%s</a>',
                code, header.number, e, e
            );
        });
    }
    if (msg.attachments) {
        ret.attachments = msg.attachments.map(function (e) {
            return format(
                '<a href="./api/attachments.ssjs?sub=%s&amp;msg=%s&amp;filename=%s" target="_blank">%s</a>',
                code, header.number, e, e
            );
        });
    }
    ret.type = msg.type;
    ret.body = msg.body;
    return ret;
}

function getMailBody(number) {

    var ret = {
        type: '',
        body: ''
    };

    number = Number(number);
    if (isNaN(number) || number < 0) return ret;

    var msgBase = new MsgBase('mail');
    if (!msgBase.open()) return ret;
    var header = msgBase.get_msg_header(false, number, false);
    if (header !== null && (header.to_ext == user.number || header.from_ext == user.number)) {
        const body = msgBase.get_msg_body(false, header);
        const pt_body = msgBase.get_msg_body(false, header, false, false, true, true);
        if (header.to_ext == user.number && (header.attr^MSG_READ)) {
            header.attr|=MSG_READ;
            msgBase.put_msg_header(false, number, header);
        }
    }
    msgBase.close();
    if (!body) return ret;

    var decoded = mimeDecode(header, body, 'mail');
    ret.type = decoded.type;
    ret.body = formatMessage(pt_body == body ? decoded.body : pt_body); // See above re: pt_body
    ret.inlines = decoded.inlines;
    ret.attachments = decoded.attachments;
    if (user.is_sysop) {
        ret.buttons = [
            format(xjs_eval(settings.web_components + 'twit-button.xjs', true), number, number, header.from, header.from_net_addr),
        ];
    }

    return ret;
}

function addTwit(str) {
    const f = new File(system.ctrl_dir + 'twitlist.cfg');
    if (!f.open('a')) {
        log(LOG_ERR, 'Failed to add ' + str + ' to twitlist');
        return;
    }
    f.writeln(str);
    f.close();
}

// Returns the user's signature, or an empty String
function getSignature() {
    var fn = format('%s/user/%04d.sig', system.data_dir, user.number);
    if (!file_exists(fn)) return '';
    var f = new File(fn);
    f.open('r');
    if (js.global.utf8_encode) {
    	var signature = utf8_encode(f.read());
    } else {
        var signature = ascii_str(f.read());
    }
    f.close();
    return signature;
}

// Post a messge to 'sub'
// Called by postNew/postReply, not directly
function postMessage(sub, header, body) {
    var ret = false;
    if (user.alias === settings.guest ||
        msg_area.sub[sub] === undefined ||
        !msg_area.sub[sub].can_post ||
        typeof header.to !== 'string' ||
        header.to === '' ||
        typeof header.from !== 'string' ||
        typeof header.subject !== 'string' ||
        typeof body !== 'string' ||
        body === ''
    ) {
        return ret;
    }
    try {
        if (msg_area.sub[sub].settings&SUB_NAME) {
            if (user.name === '') return ret;
            header.from = user.name;
        }
        body = lfexpand(body);
        var msgBase = new MsgBase(sub);
        if(msgBase.open()) {
			header.ftn_charset = "UTF-8 4";
			header.auxattr = MSG_HFIELDS_UTF8;
			ret = msgBase.save_msg(header, word_wrap(body));
			msgBase.close();
		}
    } catch (err) {
        log(err);
    }
    if (ret) user.posted_message();
    return ret;
}

// Post a message to the mail sub, if this user can do so
// Called by postNew/postReply, not directly
function postMail(header, body) {
    // Lazy ARS checks; we could check the *type* of email being sent, I guess.
    if (user.security.restrictions&UFLAG_E || user.security.restrictions&UFLAG_M) {
        return false;
    }
    if (typeof header.to !== 'string' || typeof header.subject !== 'string' || typeof body !== 'string') {
        return false;
    }
    var ret = false;
    if (user.number < 1 || user.alias === settings.guest) return ret;
    var na = netaddr_type(header.to_net_addr);
    header.to_net_type = na;
    if (na === NET_NONE) {
        var un = system.matchuser(header.to);
        if (un === 0) return false; // Should actually inform about this
        header.to_ext = un;
    }
    var msgBase = new MsgBase('mail');
    if (msgBase.open()) {
		header.ftn_charset = "UTF-8 4";
		header.auxattr = MSG_HFIELDS_UTF8;
        ret = msgBase.save_msg(header, lfexpand(body));
        msgBase.close();
    }
    if (ret) user.sent_email();
    return ret;
}

// Post a new (non-reply) message to 'sub'
function postNew(sub, to, subject, body) {
    if (typeof sub !== 'string' ||
        typeof to !== 'string' ||
        to === '' ||
        typeof subject !== 'string' ||
        subject === '' ||
        typeof body !== 'string' ||
        body === ''
    ) {
        return false;
    }
    var header = {
        to : to,
        from : user.alias,
        from_ext : user.number,
        subject : subject
    };
    if (sub === 'mail') {
	header.to_ext = system.matchuser(to);
	if (header.to_ext === 0)
		header.to_net_addr = header.to;
        return postMail(header, body);
    } else {
        return postMessage(sub, header, body);
    }
}

// Add a new message to 'sub' in reply to parent message 'pid'
function postReply(sub, body, pid) {
    var ret = false;
    if (typeof sub !== 'string' || typeof body !== 'string' || typeof pid !== 'number') return ret;
    try {
        var msgBase = new MsgBase(sub);
        msgBase.open();
        var pHeader = msgBase.get_msg_header(pid);
        msgBase.close();
        if (pHeader === null) return ret;
        var header = {
            to: pHeader.from == user.alias ? pHeader.to : pHeader.from,
            from: user.alias,
            from_ext: user.number,
            subject: pHeader.subject,
            thread_id: pHeader.thread_id === undefined ? pHeader.number : pHeader.thread_id,
            thread_back: pHeader.number,
        };
        if (sub === 'mail') {
            if (typeof pHeader.from_net_addr !== 'undefined') header.to_net_addr = pHeader.from_net_addr;
            ret = postMail(header, body);
        } else {
            ret = postMessage(sub, header, body);
        }
    } catch (err) {
        log(err);
    }
    return ret;
}

function postPoll(sub, subject, votes, results, answers, comments) {

    if (user.alias == settings.guest || user.security.restrictions&UFLAG_V) return false;
    if (typeof msg_area.sub[sub] === 'undefined' || !msg_area.sub[sub].can_post) return false;
    if (typeof subject !== 'string' || subject.length < 1) return false;
    if (!Array.isArray(answers) || answers.length < 2) return false;

    votes = parseInt(votes);
    if (isNaN(votes) || votes < 1 || votes > 15) return false;
    if (votes > answers) votes = answers;

    results = parseInt(results);
    if (isNaN(results) || results < 0 || results > 3) return false;

    var header = {
        attr: MSG_POLL,
        subject: subject.substr(0, LEN_TITLE),
        from: msg_area.sub[sub].settings&SUB_AONLY ? 'Anonymous' : (msg_area.sub[sub].settings&SUB_NAME ? user.name : user.alias),
        from_ext: user.number,
        to: 'All',
        field_list: [],
        auxattr: (results<<POLL_RESULTS_SHIFT) | MSG_HFIELDS_UTF8,
        votes: votes
    };

    if (Array.isArray(comments)) {
        comments.forEach(function (e) {
            header.field_list.push({
                type: SMB_COMMENT,
                data: e.substr(0, LEN_TITLE),
            });
        });
    }

    answers.forEach(function (e) {
        header.field_list.push({
            type: SMB_POLL_ANSWER,
            data: e.substr(0, LEN_TITLE),
        });
    });

    var msgBase = new MsgBase(sub);
    if (!msgBase.open()) return false;
    var ret = msgBase.add_poll(header);
    msgBase.close();

    if (ret) user.posted_message();
    return ret;

}

// Delete a message if
// - This is the mail sub, and the message was sent by or to this user
// - This is another sub on which the user is an operator
function deleteMessage(sub, number) {
    number = parseInt(number);
    if (msg_area.sub[sub] === undefined && sub !== 'mail') return false;
    var msgBase = new MsgBase(sub);
    if (!msgBase.open()) return false;
    var header = msgBase.get_msg_header(number);
    if (header === null) return false;
    if (sub === 'mail' && (header.to_ext == user.number || header.from_ext == user.number)) {
        var ret = msgBase.remove_msg(number);
    } else if (sub !== 'mail' && msg_area.sub[sub].is_operator) {
        var ret = msgBase.remove_msg(number);
    } else {
        var ret = false;
    }
    msgBase.close();
    return ret;
}

function deleteMail(numbers) {
    if (numbers === undefined || !Array.isArray(numbers)) return false;
    var msgBase = new MsgBase('mail');
    if (!msgBase.open()) return false;
    numbers.forEach(function (e) {
        e = parseInt(e);
        if (isNaN(e) || e < msgBase.first_msg || e > msgBase.last_msg) return;
        var header = msgBase.get_msg_header(e);
        if (header === null) return;
        if (header.to_ext == user.number || header.from_ext == user.number) {
            msgBase.remove_msg(e);
        }
    });
    msgBase.close();
    return true;
}

function voteMessage(sub, number, up) {
    if (typeof msg_area.sub[sub] === 'undefined' && sub !== 'mail') return false;
    if (user.alias == settings.guest || user.security.restrictions&UFLAG_V) return false;
    if (msg_area.sub[sub].settings&SUB_NOVOTING) return false;
    number = parseInt(number);
    if (isNaN(number)) return false;
    up = parseInt(up);
    if (isNaN(up) || up < 0 || up > 1) return false;
    var msgBase = new MsgBase(sub);
    if (!msgBase.open()) return false;
    var header = msgBase.get_msg_header(number);
    if (header === null || header.attr&MSG_POLL) {
        msgBase.close();
        return false;
    }
    var uv = msgBase.how_user_voted(header.number, msgBase.cfg.settings&SUB_NAME ? user.name : user.alias);
    if (uv === 0) {
        var vh = {
            from: msgBase.cfg.settings&SUB_NAME ? user.name : user.alias,
            from_ext: user.number,
            from_net_type: NET_NONE,
            thread_back: header.number,
            attr: up ? MSG_UPVOTE : MSG_DOWNVOTE,
        };
        var ret = msgBase.vote_msg(vh);
    }
    msgBase.close();
    return ret;
}

function submitPollAnswers(sub, number, answers) {
    if (typeof msg_area.sub[sub] === 'undefined') return false;
    if (msg_area.sub[sub].settings&SUB_NOVOTING) return false;
    if (user.alias == settings.guest || user.security.restrictions&UFLAG_V) return false;
    number = parseInt(number);
    if (isNaN(number)) return false;
    var msgBase = new MsgBase(sub);
    if (!msgBase.open()) return false;
    var ret = false;
    var header = msgBase.get_msg_header(number);
    if (header !== null && header.attr&MSG_POLL && !(header.auxattr&POLL_CLOSED) && answers.length > 0 && (answers.length <= header.votes || (answers.length == 1 && header.votes == 0))) {
        var uv = msgBase.how_user_voted(number, msgBase.cfg.settings&SUB_NAME ? user.name : user.alias);
        if (uv === 0) {
            var a = 0;
            answers.forEach(function (e) {
                e = parseInt(e);
                if (isNaN(e) || e < 0 || e > 15) return;
                a|=(1<<e);
            });
            ret = msgBase.vote_msg({
                from: msgBase.cfg.settings&SUB_NAME ? user.name : user.alias,
                from_ext: user.number,
                from_net_type: NET_NONE,
                thread_back: number,
                attr: MSG_VOTE,
                votes: a,
            });
        }
    }
    msgBase.close();
    return ret;
}

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

    var blockquote_start = '<blockquote>';
    var blockquote_end = '</blockquote>';

    var quote_depth=0;
    var prefixes = [];

    const ret = body.split(/\r?\n/).reduce(function (a, c) {
        var line = '';
        var line_prefix = '';
        var m = c.match(/^((?:\s?[^\s]{0,3}&gt;\s?)+)/);
        if (m !== null) {
            var p;
            var broken = false;            
            var new_prefixes = m[1].match(/\s?[^\s]{0,3}&gt;\s?/g);
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
                    for (var o = p; o < prefixes.length && o < new_prefixes.length; o++) {
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
function formatMessage(body, ansi, exascii) {

    // Workaround for html_encode(body, true, false, false, false);
    // which causes a crash if body is empty
    if (body === '') return body;

    if (typeof ansi === 'boolean' && ansi) {

        body = html_encode(body, true, false, true, true);
        body = body.replace(/\r?\n+(<\/span>)?$/,'$1');
        body = linkify(body);

        // Get the last line
        var body_m = body.match(/\n([^\n]*)$/);
        if (body_m !== null) {
            body = '<pre>'+body;
            body_m[1] = body_m[1].replace(/&[^;]*;/g,".");
            body_m[1] = body_m[1].replace(/<[^>]*>/g,"");
            var lenremain = 80 - body_m[1].length;
            while (lenremain > 0) {
                body += '&nbsp;';
                lenremain--;
            }
            body += '</pre>';
        } else {
            /* If we couldn't get the last line, add a line of 80 columns */
            var line = "";
            for (n = 0; n < 80; n++) {
                line += '&nbsp;';
            }
            body = '<pre>' + body + line + "</pre>";
        }

    } else {

        // Strip CTRL-A
        body = body.replace(/\1./g,'');
        // Strip ANSI
        body = body.replace(/\x1b\[[\x30-\x3f]*[\x20-\x2f]*[\x40-\x7e]/g,'');
        body = body.replace(/\x1b[\x40-\x7e]/g,'');
        // Strip unprintable control chars (NULL, BEL, DEL, ESC)
        body = body.replace(/[\x00\x07\x1b\x7f]/g,'');

        // Format for the web
        body = word_wrap(body, body.length);
        body = html_encode(body, exascii, false, false, false);
        body = quotify(body);
        body = linkify(body);
        body = body.replace(/\r\n$/,'');
        body = body.replace(/(\r?\n)/g, "<br>$1");

    }

    return body;

}

function setScanCfg(sub, cfg) {

    var opts = [
        0,
        SCAN_CFG_NEW,
        SCAN_CFG_YONLY
    ];

    if (msg_area.sub[sub] === undefined) return false;

    cfg = parseInt(cfg);
    if (isNaN(cfg) || cfg < 0 || cfg > 2) return false;

    if (cfg === 2) opts[cfg]|=SCAN_CFG_NEW;

    msg_area.sub[sub].scan_cfg = opts[cfg];
    return true;

}

function getMessageThreads(sub, max) {

    var threads = {
        thread: {},
        order: [],
    };
    var subjects = {};

    if (msg_area.sub[sub] === undefined) return threads;
    if (!msg_area.sub[sub].can_read) return threads;

    function addToThread(thread_id, header, subject) {
        if (subject !== undefined) subjects[subject] = thread_id;
        if (header.when_written_time > threads.thread[thread_id].newest) {
            threads.thread[thread_id].newest = header.when_written_time;
        }
        if (is_user() && header.number > msg_area.sub[sub].scan_ptr) {
            threads.thread[thread_id].unread++;
        }
        threads.thread[thread_id].messages[header.number] = {
            attr: header.attr,
            auxattr: header.auxattr,
            number: header.number,
            from: (header.attr&MSG_ANONYMOUS) ? "Anonymous" : (header.is_utf8 ? header.from : utf8_encode(header.from)),
            from_ext: header.from_ext,
            from_net_addr: header.from_net_addr,
            to: header.is_utf8 ? header.to : utf8_encode(header.to),
            when_written_time: header.when_written_time,
            upvotes: (header.attr&MSG_POLL ? 0 : (header.upvotes || 0)),
            downvotes: (header.attr&MSG_POLL ? 0 : (header.downvotes || 0)),
            is_utf8: header.is_utf8
        };
        if (header.attr&MSG_POLL) {
            header.field_list.sort(function (a, b) {
                if (a.type === 0x62) return -1;
                if (b.type === 0x62) return 1;
                return 0;
            });
            threads.thread[thread_id].messages[header.number].poll_comments = [];
            threads.thread[thread_id].messages[header.number].poll_answers = [];
            header.field_list.forEach(function (e) {
                if (e.type === SMB_COMMENT) {
                    threads.thread[thread_id].messages[header.number].poll_comments.push(e);
                } else if (e.type === SMB_POLL_ANSWER) {
                    threads.thread[thread_id].messages[header.number].poll_answers.push(e);
                }
            });
            threads.thread[thread_id].messages[header.number].votes = header.votes;
            threads.thread[thread_id].messages[header.number].tally = header.tally || [];
            threads.thread[thread_id].messages[header.number].subject = header.subject;
        } else {
            threads.thread[thread_id].votes.up += (header.upvotes || 0);
            threads.thread[thread_id].votes.down += (header.downvotes || 0);
        }
    }

    function getSomeMessageHeaders(msgBase, count) {
        var start = msgBase.last_msg - count;
        if (start < msgBase.first_msg) start = msgBase.first_msg;
        var headers = {};
        var c = 0;
        for (var m = start; m <= msgBase.last_msg; m++) {
            var header = msgBase.get_msg_header(m);
            if (header === null || header.attr&MSG_DELETE) continue;
            if (settings.forum_no_spam && is_spam(header)) continue;
            headers[header.number] = header;
            c++;
            if (c >= count) break;
        }
        return headers;
    }

    var msgBase = new MsgBase(sub);
    if (!msgBase.open()) return threads;
    if ((typeof max == 'number' && max > 0) || typeof msgBase.get_all_msg_headers != 'function') {
        var headers = getSomeMessageHeaders(msgBase, max);
    } else {
        var headers = msgBase.get_all_msg_headers();
    }
    msgBase.close();
    if (!headers) return threads;

    Object.keys(headers).forEach(function (h) {

        if (headers[h] === null || headers[h].attr&MSG_DELETE) {
            delete headers[h];
            return;
        }

        if (settings.forum_no_spam && is_spam(header)) {
            delete headers[h];
            return;
        }

        if (sub === 'mail' &&
            headers[h].to !== user.alias &&
            headers[h].to !== user.name &&
            headers[h].to_ext !== user.number &&
            headers[h].from !== user.alias &&
            headers[h].from !== user.name &&
            headers[h].from_ext !== user.number
        ) {
            delete headers[h];
            return;
        }

        var subject = headers[h].subject.replace(/^(re:\s*)*/ig, '');

        if (subjects[subject] !== undefined) {
            addToThread(subjects[subject], headers[h]);
        } else if (headers[h].thread_id !== 0) {
            if (threads.thread[headers[h].thread_id] === undefined) {
                threads.thread[headers[h].thread_id] = {
                    id: headers[h].thread_id,
                    newest: 0,
                    subject: headers[h].subject,
                    messages: {},
                    votes: {
                        up: 0,
                        down: 0
                    },
                    unread: 0
                };
            }
            addToThread(headers[h].thread_id, headers[h], subject);
        } else if (headers[h].thread_back !== 0) {
            if (threads.thread[headers[h].thread_back] !== undefined) {
                addToThread(headers[h].thread_back, headers[h], subject);
            } else {
                var threaded = false;
                for (var t in threads.thread) {
                    if (threads.thread[t].messages[headers[h].thread_back] !== undefined) {
                        addToThread(t, headers[h], subject);
                        threaded = true;
                        break;
                    }
                }
                if (!threaded) {
                    threads.thread[headers[h].thread_back] = {
                        id: headers[h].thread_back,
                        newest: 0,
                        subject: headers[h].subject,
                        messages: {},
                        votes: {
                            up: 0,
                            down: 0
                        },
                        unread: 0
                    };
                    addToThread(headers[h].thread_back, headers[h], subject);
                }
            }
        } else {
            threads.thread[headers[h].number] = {
                id: headers[h].number,
                newest: 0,
                subject: headers[h].subject,
                messages: {},
                votes: {
                    up: 0,
                    down: 0
                },
                unread: 0
            };
            addToThread(headers[h].number, headers[h], subject);
        }

        delete headers[h];

    });

    threads.order = Object.keys(threads.thread).sort(function (a, b) {
        return threads.thread[b].newest - threads.thread[a].newest;
    });

    return threads;

}

function getMessageThread(sub, thread, count, after) {

    thread = parseInt(thread, 10);
    if (isNaN(thread)) return [];
    count = parseInt(count, 10);
    if (isNaN(count)) return [];

    const t = getMessageThreads(sub, settings.max_messages).thread[thread];
    const mkeys = Object.keys(t.messages);
    var m; // Current message
    var r = 0; // Messages returned
    var n = 0; // Index into t.messages
    if (after) n = mkeys.indexOf(after) + 1;

    const msgBase = new MsgBase(sub);

    return function threadIterator() {
        if (r >= count || n >= mkeys.length) {
            if (msgBase.is_open) msgBase.close();
            return null; // Done
        }
        if (!msgBase.is_open && !msgBase.open()) {
            throw new Error('Failed to open ' + sub);
        }
        m = t.messages[mkeys[n]];
        const body = msgBase.get_msg_body(m.number);
        if (body === null) {
            n++;
            return threadIterator();
        }
        if (r == 0) m.subject = t.subject;
        m.body = formatMessage(body);
        n++;
        r++;
        return m;
    }

}

function isValidRequest() {
    if (Request.has_param('group')) {
        const grp = Request.get_param('group');
        if (msg_area.grp_list[grp] === undefined) return false;
        if (!user.compare_ars(msg_area.grp_list[grp].ars)) return false;
    }
    if (Request.has_param('sub')) {
        const sub = Request.get_param('sub');
        if (msg_area.sub[sub] === undefined) return false;
    }
    return true;
}
