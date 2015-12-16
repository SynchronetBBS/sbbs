load('sbbsdefs.js');
load(system.exec_dir + '../web/lib/init.js');
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

function getSubUnreadCount(sub) {
    var ret = {
        scanned : 0,
        total : 0
    };
    if (typeof msg_area.sub[sub] === 'undefined') return ret;
    try {
        var msgBase = new MsgBase(sub);
        msgBase.open();
        for (var m = msg_area.sub[sub].scan_ptr; m < msgBase.last_msg; m++) {
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

function getMailUnreadCount() {
    var count = 0;
    var msgBase = new MsgBase('mail');
    msgBase.open();
    for (var m = msgBase.first_msg; m <= msgBase.last_msg; m++) {
        var index = msgBase.get_msg_header(m);
        if (index === null) continue;
        if (index.to_ext !== user.number) continue;
        if (index.attr&MSG_READ) continue;
        if (index.attr&MSG_DELETE) continue;
        count++;
    }
    msgBase.close();
    return count;
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
    var header = msgBase.get_msg_header(number);
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
    ret.body = formatMessage(decoded.body);
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
    var na = netaddr_type(header.to);
    header.to_net_type = na;
    if (na > 0) header.to_net_addr = header.to;
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
            'to_net_addr' : pHeader.from_net_addr,
            'from' : user.alias,
            'subject' : pHeader.subject,
            'thread_back' : pHeader.number
        };
        if (sub === 'mail') {
            ret = postMail(header, body);
        } else {
            ret = postMessage(sub, header, body);
        }
    } catch (err) {
        log(err);
    }
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
	log(JSON.stringify(header));
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
function formatMessage(body, ansi) {

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
        body = html_encode(body, true, false, false, false);
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

var getMessageThreads = function(sub, max) {

    var threads = { thread : {}, order : [] };
    var subjects = {};

    function addToThread(thread_id, header, subject) {
        if (typeof subject !== 'undefined') subjects[subject] = thread_id;
        threads.thread[thread_id].newest = header.when_written_time;
        threads.thread[thread_id].messages[header.number] = {
            number : header.number,
            from : header.from,
            from_net_addr : header.from_net_addr,
            to : header.to,
            when_written_time : header.when_written_time
        };
    }

    function getSomeMessageHeaders(msgBase, start, count) {
        if (start < 0 || start > msgBase.last_msg) return {};
        if (start + count - 1 > msgBase.last_msg) {
            count = msgBase.last_msg - start;
        }
        var headers = {};
        for (var m = 0; m < count; m++) {
            var header = msgBase.get_msg_header(start + m);
            if (header === null || header.attr&MSG_DELETE) continue;
            headers[header.number] = header;
        }
        return headers;
    }

    var msgBase = new MsgBase(sub);
    if (!msgBase.open()) return threads;
    if ((typeof max === 'number' && max > 0) ||
        typeof msgBase.get_all_msg_headers !== 'function'
    ) {
        var headers = getSomeMessageHeaders(msgBase, 0, max);
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
                        messages : {}
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
                            messages : {}
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
                    messages : {}
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