load('sbbsdefs.js');
load('modopts.js');
var settings = get_mod_options('web');

load(settings.web_directory + '/lib/init.js');
load(settings.web_lib + 'mime-decode.js');

function listGroups() {
    var response = [];
    msg_area.grp_list.forEach(
        function (grp) {
            if (grp.sub_list.length < 1) return;
            response.push(
                {   index : grp.index,
                    name : grp.name,
                    description : grp.description,
                    sub_count : grp.sub_list.length
                }
            );
        }
    );
    return response;
}

// Returns an array of objects of "useful" information about subs
function listSubs(group) {
    var response = [];
    msg_area.grp_list[group].sub_list.forEach(
        function (sub) {
            response.push(
                {   index : sub.index,
                    code : sub.code,
                    grp_index : sub.grp_index,
                    grp_name : sub.grp_name,
                    name : sub.name,
                    description : sub.description,
                    qwk_name : sub.qwk_name,
                    qwk_conf : sub.qwk_conf,
                    qwk_tagline : sub.qwknet_tagline,
                    newsgroup : sub.newsgroup,
                    ars : sub.ars,
                    read_ars : sub.read_ars,
                    can_read : sub.can_read,
                    post_ars : sub.post_ars,
                    can_post : sub.can_post,
                    operator_ars : sub.operator_ars,
                    is_operator : sub.is_operator,
                    moderated_ars : sub.moderated_ars,
                    is_moderated : sub.is_moderated,
                    scan_ptr : sub.scan_ptr,
                    scan_cfg : sub.scan_cfg
                }
            );
        }
    );
    return response;
}

function listThreads(sub, offset, count) {

    offset = parseInt(offset);
    if (isNaN(offset) || offset < 0) return false;
    count = parseInt(count);
    if (isNaN(count) || count < 1) return false;

    var threads = getMessageThreads(sub, settings.max_messages);
    if (offset >= threads.order.length) return false;

    var stop = Math.min(threads.order.length, offset + count);
    var ret = [];
    for (var n = offset; n < stop; n++) {
        var thread = threads.thread[threads.order[n]];
        var msgs = Object.keys(thread.messages);
        thread.first = thread.messages[msgs[0]];
        thread.last = thread.messages[msgs[msgs.length - 1]];
        delete thread.messages;
        ret.push(thread);
    }

    return ret;

}

function getSubUnreadCount(sub) {
    var ret = {
        scanned : 0,
        total : 0
    };
    if (typeof msg_area.sub[sub] === 'undefined') return ret;
    try {
        var msgBase = new MsgBase(sub);
        msgBase.open();
        for (var m = msg_area.sub[sub].scan_ptr + 1; m <= msgBase.last_msg; m++) {
            var i = msgBase.get_msg_index(m);
            if (i === null || i.attr&MSG_DELETE || i.attr&MSG_NODISP) continue;
            if ((   msg_area.sub[sub].scan_cfg&SCAN_CFG_YONLY &&
                    i.to === crc16_calc(user.alias.toLowerCase()) ||
                    i.to === crc16_calc(user.name.toLowerCase()) ||
                    (sub === 'mail' && i.to === crc16_calc(user.number))
                ) ||
                msg_area.sub[sub].scan_cfg&SCAN_CFG_NEW
            ) {
                ret.scanned++;
            }
            ret.total++;
        }
        msgBase.close();
    } catch (err) {
        log(LOG_ERR, err);
    }
    return ret;
}

function getGroupUnreadCount(group) {
    var ret = {
        scanned : 0,
        total : 0
    };
    if (typeof msg_area.grp_list[group] === 'undefined') return count;
    msg_area.grp_list[group].sub_list.forEach(
        function (sub) {
            var count = getSubUnreadCount(sub.code);
            ret.scanned += count.scanned;
            ret.total += count.total;
        }
    );
    return ret;
}

function getUnreadInThread(sub, thread) {
    if (typeof thread === 'number') {
        var threads = getMessageThreads(sub, settings.max_messages);
        if (typeof threads.thread[thread] === 'undefined') return 0;
        thread = threads.thread[thread];
    }
    var count = 0;
    Object.keys(thread.messages).forEach(
        function (m) {
            if (thread.messages[m].number > msg_area.sub[sub].scan_ptr) count++;
        }
    );
    return count;
}

function getVotesInThread(sub, thread) {
    var ret = { t : { u : 0, d : 0 }, m : {} };
    if (typeof msg_area.sub[sub] === 'undefined') return ret;
    if (typeof thread === 'number') {
        var threads = getMessageThreads(sub, settings.max_messages);
        if (typeof threads.thread[thread] === 'undefined') return ret;
        thread = threads.thread[thread];
    }
    var msgBase = new MsgBase(sub);
    if (!msgBase.open()) return ret;
    Object.keys(thread.messages).forEach(
        function (m) {
            if (thread.messages[m].upvotes > 0 ||
                thread.messages[m].downvotes > 0
            ) {
                ret.t.up += thread.messages[m].upvotes;
                ret.t.down += thread.messages[m].downvotes;
                ret.m[thread.messages[m].number] = {
                    u : thread.messages[m].upvotes,
                    d : thread.messages[m].downvotes,
                    v : msgBase.how_user_voted(
                        thread.messages[m].number,
                        msgBase.cfg.settings&SUB_NAME ? user.name : user.alias
                    )
                };
            }
        }
    );
    msgBase.close();
    return ret;
}

function getVotesInThreads(sub) {
    var threads = getMessageThreads(sub, settings.max_messages);
    var ret = {};
    Object.keys(threads.thread).forEach(
        function(t) {
            Object.keys(threads.thread[t].messages).forEach(
                function (m, i) {
                    if (threads.thread[t].messages[m].upvotes < 1 &&
                        threads.thread[t].messages[m].downvotes < 1
                    ) {
                        return;
                    }
                    if (typeof ret[t] === 'undefined') {
                        ret[t] = { p : { u : 0, d : 0 }, t : { u : 0, d : 0 } };
                        if (i < 1) {
                            ret[t].p.u = threads.thread[t].messages[m].upvotes;
                            ret[t].p.d = threads.thread[t].messages[m].downvotes;
                        }
                    }
                    ret[t].t.u += threads.thread[t].messages[m].upvotes;
                    ret[t].t.d += threads.thread[t].messages[m].downvotes;
                }
            );
        }
    );
    return ret;
}

function getUserPollData(sub, id) {
    var ret = { answers : 0, tally : [], show_results : false };
    if (typeof msg_area.sub[sub] === 'undefined') return ret;
    id = parseInt(id);
    if (isNaN(id)) return ret;
    var msgBase = new MsgBase(sub);
    if (!msgBase.open()) return ret;
    // var header = msgBase.get_msg_header(id);
    // Temporary use of get_all_msg_headers() to get header.tally for polls
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
    ret.answers = msgBase.how_user_voted(
        header.number,
        msgBase.cfg.settings&SUB_NAME ? user.name : user.alias
    );
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
    if (typeof sent !== 'undefined' &&
        sent &&
        user.security.restrictions&UFLAG_K
    ) {
        return []; // They'll just see nothing.  Provide actual feedback?  Does anyone use REST K?
    }
    var headers = [];
    var msgBase = new MsgBase('mail');
    if (!msgBase.open()) return headers;
    for (var m = msgBase.first_msg; m <= msgBase.last_msg; m++) {
        var h = msgBase.get_msg_header(m);
        if (h === null || h.attr&MSG_DELETE) continue;
        if (    (typeof sent != 'undefined' && sent) &&
                h.from_ext != user.number
        ) {
            continue;
        } else if (
            (typeof sent == 'undefined' || !sent) &&
            h.to_ext != user.number
        ) {
            continue;
        }
        headers.push(h);
    }
    msgBase.close();
    if (typeof ascending === 'undefined' || !ascending) headers.reverse();
    return headers;
}

function mimeDecode(header, body, code) {
    var ret = {
        type : '',
        body : [],
        inlines : [],
        attachments : []
    };
    var msg = mime_decode(header, body, code);
    if (typeof msg.inlines !== 'undefined') {
        msg.inlines.forEach(
            function (inline) {
                ret.inlines.push(
                    format(
                        '<a href="./api/attachments.ssjs?sub=%s&amp;msg=%s&amp;cid=%s" target="_blank">%s</a>',
                        code, header.number, inline, inline
                    )
                );
            }
        );
    }
    if (typeof msg.attachments !== 'undefined') {
        msg.attachments.forEach(
            function (attachment) {
                ret.attachments.push(
                    format(
                        '<a href="./api/attachments.ssjs?sub=%s&amp;msg=%s&amp;filename=%s" target="_blank">%s</a>',
                        code, header.number, attachment, attachment
                    )
                );
            }
        );
    }
    ret.type = msg.type;
    ret.body = msg.body;
    return ret;
}

function getMailBody(number) {

    var ret = {
        type : '',
        body : '',
        inlines : [],
        attachments : []
    };

    number = Number(number);
    if (isNaN(number) || number < 0) return ret;

    var msgBase = new MsgBase('mail');
    if (!msgBase.open()) return ret;
    var header = msgBase.get_msg_header(false, number, false);
    if (header !== null &&
        (   header.to_ext == user.number ||
            header.from_ext == user.number
        )
    ) {
        var body = msgBase.get_msg_body(false, number, header);
        if (header.to_ext == user.number && (header.attr^MSG_READ)) {
            header.attr|=MSG_READ;
            msgBase.put_msg_header(false, number, header);
        }
    }
    msgBase.close();
    if (typeof body === 'undefined' || body === null) return ret;

    var decoded = mimeDecode(header, body, 'mail');
    ret.type = decoded.type;
//    if (ret.type == 'html') {
//      ret.body = html_decode(decoded.body);
//    } else {
      ret.body = formatMessage(decoded.body);
//    }
    ret.inlines = decoded.inlines;
    ret.attachments = decoded.attachments;

    return ret;
}

// Returns the user's signature, or an empty String
function getSignature() {
    var fn = format('%s/user/%04d.sig', system.data_dir, user.number);
    if (!file_exists(fn)) return '';
    var f = new File(fn);
    f.open('r');
    var signature = f.read();
    f.close();
    return signature;
}

// Post a messge to 'sub'
// Called by postNew/postReply, not directly
function postMessage(sub, header, body) {
    var ret = false;
    if (user.alias === settings.guest ||
        typeof msg_area.sub[sub] === 'undefined' ||
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
        msgBase.open();
        ret = msgBase.save_msg(header, word_wrap(body));
        msgBase.close();
    } catch (err) {
        log(err);
    }
    return ret;
}

// Post a message to the mail sub, if this user can do so
// Called by postNew/postReply, not directly
function postMail(header, body) {
    // Lazy ARS checks; we could check the *type* of email being sent, I guess.
    if (user.security.restrictions&UFLAG_E ||
        user.security.restrictions&UFLAG_M
    ) {
        return false;
    }
    if (typeof header.to !== 'string' ||
        typeof header.subject !== 'string' ||
        typeof body !== 'string'
    ) {
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
        ret = msgBase.save_msg(header, lfexpand(body));
        msgBase.close();
    }
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
        header.to_net_addr = header.to;
        return postMail(header, body);
    } else {
        return postMessage(sub, header, body);
    }
}

// Add a new message to 'sub' in reply to parent message 'pid'
function postReply(sub, body, pid) {
    var ret = false;
    if (    typeof sub !== 'string' ||
            typeof body !== 'string' ||
            typeof pid !== 'number'
    ) {
        return ret;
    }
    try {
        var msgBase = new MsgBase(sub);
        msgBase.open();
        var pHeader = msgBase.get_msg_header(pid);
        msgBase.close();
        if (pHeader === null) return ret;
        var header = {
            'to' : pHeader.from,
            'from' : user.alias,
            'subject' : pHeader.subject,
            'thread_id' : (
                typeof pHeader.thread_id === 'undefined'
                ? pHeader.number
                : pHeader.thread_id
            ),
            'thread_back' : pHeader.number
        };
        if (sub === 'mail') {
            if (typeof pHeader.from_net_addr !== 'undefined') {
                header.to_net_addr = pHeader.from_net_addr;
            }
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

    if (user.alias == settings.guest || user.security.restrictions&UFLAG_V) {
        return false;
    }

    if (typeof msg_area.sub[sub] === 'undefined' || !msg_area.sub[sub].can_post) {
        return false;
    }

    if (typeof subject !== 'string' || subject.length < 1) return false;

    if (!Array.isArray(answers) || answers.length < 2) return false;

    votes = parseInt(votes);
    if (isNaN(votes) || votes < 1 || votes > 15) return false;
    if (votes > answers) votes = answers;

    results = parseInt(results);
    if (isNaN(results) || results < 0 || results > 3) {
        return false;
    }

    var header = {
        attr : MSG_POLL,
        subject : subject.substr(0, LEN_TITLE),
        from : msg_area.sub[sub].settings&SUB_NAME ? user.name : user.alias,
        from_ext : user.number,
        to : 'All',
        field_list : [],
        auxattr : (results<<POLL_RESULTS_SHIFT),
        votes : votes
    };

    if (Array.isArray(comments)) {
        comments.forEach(
            function (e) {
                header.field_list.push(
                    { type : SMB_COMMENT, data : e.substr(0, LEN_TITLE) }
                );
            }
        );
    }

    answers.forEach(
        function (e) {
            header.field_list.push(
                { type : SMB_POLL_ANSWER, data : e.substr(0, LEN_TITLE) }
            );
        }
    );

    var msgBase = new MsgBase(sub);
    if (!msgBase.open()) return false;
    var ret = msgBase.add_poll(header);
    msgBase.close();

    return ret;

}

// Delete a message if
// - This is the mail sub, and the message was sent by or to this user
// - This is another sub on which the user is an operator
function deleteMessage(sub, number) {
    number = parseInt(number);
    if (typeof msg_area.sub[sub] === 'undefined' && sub !== 'mail') {
        return false;
    }
    var msgBase = new MsgBase(sub);
    if (!msgBase.open()) return false;
    var header = msgBase.get_msg_header(number);
    if (header === null) return false;
    if (sub === 'mail' &&
        (header.to_ext == user.number || header.from_ext == user.number)
    ) {
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
    if (typeof numbers === 'undefined' || !Array.isArray(numbers)) return false;
    var msgBase = new MsgBase('mail');
    if (!msgBase.open()) return false;
    numbers.forEach(
        function (e) {
            e = parseInt(e);
            if (isNaN(e) || e < msgBase.first_msg || e > msgBase.last_msg) return;
            var header = msgBase.get_msg_header(e);
            if (header === null) return;
            if (header.to_ext == user.number || header.from_ext == user.number) {
                msgBase.remove_msg(e);
            }
        }
    );
    msgBase.close();
    return true;
}

function voteMessage(sub, number, up) {
    if (typeof msg_area.sub[sub] === 'undefined' && sub !== 'mail') {
        return false;
    }
    if (user.alias == settings.guest || user.security.restrictions&UFLAG_V) {
        return false;
    }
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
    var uv = msgBase.how_user_voted(
        header.number, msgBase.cfg.settings&SUB_NAME ? user.name : user.alias
    );
    if (uv === 0) {
        var vh = {
            'from' : msgBase.cfg.settings&SUB_NAME ? user.name : user.alias,
            'from_ext' : user.number,
            'from_net_type' : NET_NONE,
            'thread_back' : header.number,
            'attr' : up ? MSG_UPVOTE : MSG_DOWNVOTE
        };
        var ret = msgBase.vote_msg(vh);
    }
    msgBase.close();
    return ret;
}

function submitPollAnswers(sub, number, answers) {
    if (typeof msg_area.sub[sub] === 'undefined') return false;
    if (msg_area.sub[sub].settings&SUB_NOVOTING) return false;
    if (user.alias == settings.guest || user.security.restrictions&UFLAG_V) {
        return false;
    }
    number = parseInt(number);
    if (isNaN(number)) return false;
    var msgBase = new MsgBase(sub);
    if (!msgBase.open()) return false;
    var ret = false;
    var header = msgBase.get_msg_header(number);
    if (header !== null &&
        header.attr&MSG_POLL &&
        !(header.auxattr&POLL_CLOSED) &&
        answers.length > 0 &&
        (   answers.length <= header.votes ||
            (answers.length == 1 && header.votes == 0)
        )
    ) {
        var uv = msgBase.how_user_voted(
            number, msgBase.cfg.settings&SUB_NAME ? user.name : user.alias
        );
        if (uv === 0) {
            var a = 0;
            answers.forEach(
                function (e) {
                    e = parseInt(e);
                    if (isNaN(e) || e < 0 || e > 15) return;
                    a|=(1<<e);
                }
            );
            ret = msgBase.vote_msg(
                {   'from' : msgBase.cfg.settings&SUB_NAME ? user.name : user.alias,
                    'from_ext' : user.number,
                    'from_net_type' : NET_NONE,
                    'thread_back' : number,
                    'attr' : MSG_VOTE,
                    'votes' : a
                }
            );
        }
    }
    msgBase.close();
    return ret;
}

// Deuce's URL-ifier
function linkify(body) {
    urlRE = /(?:https?|ftp|telnet|ssh|gopher|rlogin|news):\/\/[^\s'"'<>()]*|[-\w.+]+@(?:[-\w]+\.)+[\w]{2,6}/gi;
    body = body.replace(
        urlRE,
        function (str) {
            var ret=''
            var p=0;
            var link=str.replace(/\.*$/, '');
            var linktext=link;
            if (link.indexOf('://') === -1) link = 'mailto:' + link;
            return ('<a class="ulLink" href="' + link + '">' + linktext + '</a>' + str.substr(linktext.length));
        }
    );
    return (body);
}

// Somewhat modified version of Deuce's "magical quoting stuff" from v3
function quotify(body) {

    var blockquote_start = '<blockquote>';
    var blockquote_end = '</blockquote>';

    var lines = body.split(/\r?\n/);
    body = '';

    var quote_depth=0;
    var prefixes = [];

    for (l in lines) {

        var line_prefix = '';
        var m = lines[l].match(/^((?:\s?[^\s]{0,3}&gt;\s?)+)/);

        if (m !== null) {

            var new_prefixes = m[1].match(/\s?[^\s]{0,3}&gt;\s?/g);
            var p;
            var broken = false;

            line = lines[l];

            // If the new length is smaller than the old one, close the extras
            for (p = new_prefixes.length; p < prefixes.length; p++) {
                if (quote_depth < 1) continue;
                line_prefix = line_prefix + blockquote_end;
                quote_depth--;
            }

            for (p in new_prefixes) {
                // Remove prefix from start of line
                line = line.substr(new_prefixes[p].length);

                if (typeof prefixes[p] === "undefined") {
                    /* New depth */
                    line_prefix = line_prefix + blockquote_start;
                    quote_depth++;
                } else if (broken) {
                    line_prefix = line_prefix + blockquote_start;
                    quote_depth++;
                } else if (prefixes[p].replace(/^\s*(.*?)\s*$/,"$1") != new_prefixes[p].replace(/^\s*(.*?)\s*$/,"$1")) {
                    // Close all remaining old prefixes and start one new one
                    var o;
                    for (o = p; o < prefixes.length && o < new_prefixes.length; o++) {
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
            line = line_prefix + lines[l];

        }

        body = body + line + "\r\n";

    }

    if (quote_depth !== 0) {
        for (;quote_depth > 0; quote_depth--) {
            body += blockquote_end;
        }
    }

    return body.replace(/\<\/blockquote\>\r\n<blockquote\>/g, "\r\n");

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

    if (typeof msg_area.sub[sub] === 'undefined') return false;

    cfg = parseInt(cfg);
    if (isNaN(cfg) || cfg < 0 || cfg > 2) return false;

    if (cfg === 2) opts[cfg]|=SCAN_CFG_NEW;

    msg_area.sub[sub].scan_cfg = opts[cfg];
    return true;

}

function getMessageThreads(sub, max) {

    var threads = { thread : {}, order : [] };
    var subjects = {};

    if (typeof msg_area.sub[sub] === 'undefined') return threads;
    if (!msg_area.sub[sub].can_read) return threads;

    function addToThread(thread_id, header, subject) {
        if (typeof subject !== 'undefined') subjects[subject] = thread_id;
        if (header.when_written_time > threads.thread[thread_id].newest) {
            threads.thread[thread_id].newest = header.when_written_time;
        }
        threads.thread[thread_id].messages[header.number] = {
            attr : header.attr,
            auxattr : header.auxattr,
            number : header.number,
            from : header.from,
            from_net_addr : header.from_net_addr,
            to : header.to,
            when_written_time : header.when_written_time,
            upvotes : (header.attr&MSG_POLL ? 0 : (header.upvotes || 0)),
            downvotes : (header.attr&MSG_POLL ? 0 : (header.downvotes || 0))
        };
        if (header.attr&MSG_POLL) {
            header.field_list.sort(
                function (a, b) {
                    if (a.type === 0x62) return -1;
                    if (b.type === 0x62) return 1;
                    return 0;
                }
            );
            threads.thread[thread_id].messages[header.number].poll_comments = [];
            threads.thread[thread_id].messages[header.number].poll_answers = [];
            header.field_list.forEach(
                function (e) {
                    switch (e.type) {
                        case SMB_COMMENT:
                            threads.thread[thread_id].messages[header.number].poll_comments.push(e);
                            break;
                        case SMB_POLL_ANSWER:
                            threads.thread[thread_id].messages[header.number].poll_answers.push(e);
                            break;
                        default:
                            break;
                    }
                }
            );
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
            headers[header.number] = header;
            c++;
            if (c >= count) break;
        }
        return headers;
    }

    var msgBase = new MsgBase(sub);
    if (!msgBase.open()) return threads;
    if ((typeof max === 'number' && max > 0) ||
        typeof msgBase.get_all_msg_headers !== 'function'
    ) {
        var headers = getSomeMessageHeaders(msgBase, max);
    } else {
        var headers = msgBase.get_all_msg_headers();
    }
    msgBase.close();

    Object.keys(headers).forEach(

        function(h) {

            if (headers[h] === null || headers[h].attr&MSG_DELETE) {
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

            if (typeof subjects[subject] !== 'undefined') {

                addToThread(subjects[subject], headers[h]);

            } else if (headers[h].thread_id !== 0) {

                if (typeof threads.thread[headers[h].thread_id]
                    !== 'undefined'
                ) {
                    addToThread(headers[h].thread_id, headers[h], subject);
                } else {
                    threads.thread[headers[h].thread_id] = {
                        id : headers[h].thread_id,
                        newest : 0,
                        subject : headers[h].subject,
                        messages : {},
                        votes : {
                            up : 0,
                            down : 0
                        }
                    };
                    addToThread(headers[h].thread_id, headers[h], subject);
                }

            } else if (headers[h].thread_back !== 0) {

                if (typeof threads.thread[headers[h].thread_back]
                    !== 'undefined'
                ) {
                    addToThread(headers[h].thread_back, headers[h], subject);
                } else {
                    var threaded = false;
                    for (var t in threads.thread) {
                        if (typeof
                            threads.thread[t].messages[headers[h].thread_back]
                            !== 'undefined'
                        ) {
                            addToThread(t, headers[h], subject);
                            threaded = true;
                            break;
                        }
                    }
                    if (!threaded) {
                        threads.thread[headers[h].thread_back] = {
                            id : headers[h].thread_back,
                            newest : 0,
                            subject : headers[h].subject,
                            messages : {},
                            votes : {
                                up : 0,
                                down : 0
                            }
                        };
                        addToThread(
                            headers[h].thread_back,
                            headers[h],
                            subject
                        );
                    }
                }

            } else {

                threads.thread[headers[h].number] = {
                    id : headers[h].number,
                    newest : 0,
                    subject : headers[h].subject,
                    messages : {},
                    votes : {
                        up : 0,
                        down : 0
                    }
                };
                addToThread(headers[h].number, headers[h], subject);

            }

            delete headers[h];

        }

    );

    threads.order = Object.keys(threads.thread).sort(
        function (a, b) {
            return threads.thread[b].newest - threads.thread[a].newest;
        }
    );

    return threads;

}
