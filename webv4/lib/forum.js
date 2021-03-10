require('sbbsdefs.js', 'MSG_DELETE');
require('xjs.js', 'xjs_compile');
load(settings.web_lib + 'mime-decode.js');

function validString(s) {
    return (typeof s === 'string' && s !== '');
}

function cleanSubject(subject) {
    return subject.replace(/^(re:\s*)*/ig, '');
}

function getNewestMessageInSub(mb) {
    var h;
    var ret;
    for (var m = mb.last_msg; m >= mb.first_msg; m--) {
        h = mb.get_msg_header(m);
        if (h === null) continue;
        ret = {
            from: h.from,
            subject: h.subject,
            when_written_time: h.when_written_time,
        };
        break;
    }
    return ret;
}

function getSubUnreadCount(sub) {

    var ret = {
        scanned: 0,
        total: 0,
    };

    var mb;
    if (sub instanceof MsgBase) {
        mb = sub;
    } else {
        if (msg_area.sub[sub] === undefined) return ret;
        mb = new MsgBase(sub);
        if (!mb.open()) throw new Error(mb.error);
    }

    var sy = msg_area.sub[mb.cfg.code].scan_cfg&SCAN_CFG_YONLY;
    var sn = msg_area.sub[mb.cfg.code].scan_cfg&SCAN_CFG_NEW;
    for (var m = msg_area.sub[mb.cfg.code].scan_ptr + 1; m <= mb.last_msg; m++) {
        var h = mb.get_msg_header(m);
        if (h === null || h.attr&MSG_DELETE || h.attr&MSG_NODISP) continue;
        if ((sy && (h.to_ext === user.number || h.to === user.alias || h.to === user.name)) || sn) ret.scanned++;
        ret.total++;
    }

    if (!(sub instanceof MsgBase)) mb.close();
    
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

var forum = {

    addTwit: function addTwit(str) {
        const f = new File(system.ctrl_dir + 'twitlist.cfg');
        if (!f.open('a')) {
            log(LOG_ERR, 'Failed to add ' + str + ' to twitlist');
            return;
        }
        f.writeln(str);
        f.close();
    },

    // get-signature
    // Returns the user's signature, or an empty String
    getSignature: function getSignature(un) {
        var f = new File(format('%s/user/%04d.sig', system.data_dir, un));
        if (!f.exists || !f.open('r')) return '';
        if (js.global.utf8_encode) {
            var signature = utf8_encode(f.read());
        } else {
            var signature = ascii_str(f.read());
        }
        f.close();
        return signature;
    },

    // list-groups
    listGroups: function listGroups() {
        return msg_area.grp_list.reduce(function (a, c) {
            if (c.sub_list.length < 1) return a;
            a.push({
                index: c.index,
                name: c.name,
                description: c.description,
                sub_count: c.sub_list.length,
            });
            return a;
        }, []);
    },

    // list-subs
    listSubs: function listSubs(group) {
        return msg_area.grp_list[group].sub_list.map(function (e) {
            const mb = new MsgBase(e.code);
            if (!mb.open()) throw new Error(mb.error);
            const ret = {
                index: e.index,
                code: e.code,
                grp_index: e.grp_index,
                grp_name: e.grp_name,
                name: e.name,
                description: e.description,
                can_post: e.can_post,
                is_operator: e.is_operator,
                is_moderated: e.is_moderated,
                scan_ptr: e.scan_ptr,
                scan_cfg: e.scan_cfg,
                newest: getNewestMessageInSub(mb),
            };
            mb.close();
            return ret;
        });
    },

    // get-newest-message-per-sub (getNewestMessagePerSub)
    getNewestMessagePerSub: function getNewestMessagePerSub(grp) {
        grp = parseInt(grp, 10);
        if (isNaN(grp) || grp < 0 || !msg_area.grp_list[grp]) return [];
        return msg_area.grp_list[grp].sub_list.reduce(function (a, c) {
            // const s = getNewestMessageInSub(c);
            // if (s !== undefined) a[c.code] = s;
            return a;
        }, {});
    },

    // set-scan-cfg (setScanCfg)
    setScanCfg: function setScanCfg(sub, cfg) {

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
    
    },

    // post
    // Post a new (non-reply) message to 'sub'
    postNew: function postNew(sub, to, subject, body) {
        if (!validString(sub) || !validString(to) || !validString(subject) || !validString(body)) return false;
        var header = {
            to: to,
            from: user.alias,
            from_ext: user.number,
            subject: subject
        };
        if (sub === 'mail') {
            header.to_ext = system.matchuser(to);
            if (header.to_ext === 0) header.to_net_addr = header.to;
            return postMail(header, body);
        } else {
            return postMessage(sub, header, body);
        }
    },

    // post-reply
    // Add a new message to 'sub' in reply to parent message 'pid'
    postReply: function postReply(sub, body, pid) {
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
    },

    // submit-poll
    postPoll: function postPoll(sub, subject, votes, results, answers, comments) {

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
    
    },

    // list-threads (getThreadList)
    getThreadList: function getThreadList(sub) {

        const threads = {};
        const subjects = {}; // Map of "clean" subjects to threads
        const messages = {}; // Map of messsage numbers to threads

        function addThread(h, s) {
            threads[h.thread_id] = {
                first_message: {
                    from: h.from,
                    from_net_addr: h.from_net_addr,
                    from_net_type: h.from_net_type,
                    tags: h.tags,
                    to: h.to,
                    to_net_addr: h.to_net_addr,
                    to_net_type: h.to_net_type,
                    subject: h.subject,
                    when_written_time: h.when_written_time,
                },
                id: h.thread_id,
                last_message: {
                    from: h.from,
                    when_written_time: h.when_written_time,
                },
                messages: 1,
                sub: sub,
                votes: {
                    parent: {
                        up: h.upvotes,
                        down: h.downvotes,
                    },
                    total: {
                        up: h.upvotes,
                        down: h.downvotes,
                    }
                }
            };
            subjects[s] = h.thread_id;
            messages[h.thread_id] = h.thread_id;
        }

        function addToThread(t, h, s, m) {
            threads[t].messages++;
            threads[t].last_message = {
                from: h.from,
                when_written_time: h.when_written_time,
            };
            threads[t].votes.total.up += h.upvotes;
            threads[t].votes.total.down += h.downvotes;
            if (subjects[s] === undefined) subjects[s] = t;
            messages[m] = t;
        }

        const mb = new MsgBase(sub);
        if (!mb.open()) return threads;
        const headers = mb.get_all_msg_headers();
        mb.close();

        var s; // "clean" subject of current message
        for (var h in headers) {
            if (headers[h].attr&MSG_DELETE) continue;
            s = cleanSubject(headers[h].subject);
            // If we don't yet have a thread for this message's thread_id:
            if (threads[headers[h].thread_id] === undefined) {
                // If this message's thread_id points to a message that belongs to another thread:
                if (messages[headers[h].thread_id] !== undefined) {
                    // The record in messages[] for that message tells us which thread it belongs to
                    addToThread(messages[headers[h].thread_id], headers[h], s, h);
                // If this message's "clean" subject has been seen before:
                } else if (subjects[s] !== undefined) {
                    // The record in subjects[] for this subject tells us which thread shares this subject
                    addToThread(subjects[s], headers[h], s, h);
                // This is the first message in a new thread
                } else {
                    addThread(headers[h], s);
                }
            // We have a thread for this message's thread_id:
            } else {
                addToThread(headers[h].thread_id, headers[h], s, h);
            }
        }

        return threads;

    },

    // get-thread (getThread)
    getThread: function getThread(sub, id, onMessage) {

        id = parseInt(id, 10);
        if (isNaN(id) || id < 0) return;

        const mb = new MsgBase(sub);
        if (!mb.open()) return;

        if (id < mb.first_msg || id > mb.last_msg) {
            mb.close();
            return;
        }
        
        var b;
        var s;
        const subjects = [];
        const messages = [];

        const headers = mb.get_all_msg_headers();
        
        for (var h in headers) {

            if (headers[h].attr&MSG_DELETE) continue;

            s = cleanSubject(headers[h].subject);
            if (headers[h].thread_id !== id && messages.indexOf(headers[h].thread_id) < 0 && subjects.indexOf(s) < 0) continue;
            messages.push(parseInt(h, 10));
            if (subjects.indexOf(s) < 0) subjects.push(s);

            b = mb.get_msg_body(parseInt(h, 10));
            if (b === null) continue; // Not sure if this is a holdover from early vote msg days. Is body ever null on a real message?

            onMessage({
                body: b, // Do we use formatMessage here, or let the browser handle that? Leaning toward client-side formatting.
                from: headers[h].from,
                from_net_addr: headers[h].from_net_addr,
                from_net_type: headers[h].from_net_type,
                number: parseInt(h, 10),
                subject: headers[h].subject,
                tags: headers[h].tags,
                thread_id: headers[h].thread_id, // for debug; remove this line at some point
                thread_back: headers[h].thread_back,
                thread_next: headers[h].thread_next,
                thread_first: headers[h].thread_first,
                to: headers[h].to,
                to_net_addr: headers[h].to_net_addr,
                to_net_type: headers[h].to_net_type,
                votes: {
                    up: headers[h].upvotes,
                    down: headers[h].downvotes,
                },
                when_written_time: headers[h].when_written_time,
            });

        }

        mb.close();

    },

};

forum;