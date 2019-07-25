/*  This script is an interface between HTTP clients and the functions defined
    in web/lib/forum.js.  A basic check for an authenticated, non-guest user
    is done here; otherwise all permission checking is done at the function
    level. */

var settings = load('modopts.js', 'web');

require(settings.web_directory + '/lib/init.js', 'WEBV4_INIT');
const auth_lib = load({}, settings.web_lib + 'auth.js');
const forum_lib = load({}, settings.web_lib + 'forum.js');

var reply = {};

// There must be an API call, and the user must not be a guest or unknown
if ((http_request.method === 'GET' || http_request.method === 'POST') &&
    typeof http_request.query.call !== 'undefined'
) {

    var handled = false;

    // Authenticated calls
    if (user.number > 0 && user.alias !== settings.guest) {

        handled = true;

        switch(http_request.query.call[0].toLowerCase()) {

            case 'get-mail-unread-count':
                reply.count = user.stats.mail_waiting;
                break;

            case 'get-mail-body':
                if (typeof http_request.query.number !== 'undefined') {
                    reply = forum_lib.getMailBody(http_request.query.number[0]);
                }
                break;

            case 'get-signature':
                reply.signature = forum_lib.getSignature();
                break;

            case 'post-reply':
                if (typeof http_request.query.sub !== 'undefined' &&
                    typeof http_request.query.body !== 'undefined' &&
                    typeof http_request.query.pid !== 'undefined'
                ) {
                    reply.success = forum_lib.postReply(
                        http_request.query.sub[0],
                        http_request.query.body[0],
                        Number(http_request.query.pid[0])
                    );
                } else {
                    reply.success = false;
                }
                break;

            case 'post':
                if (typeof http_request.query.sub !== 'undefined' &&
                    typeof http_request.query.to !== 'undefined' &&
                    typeof http_request.query.subject !== 'undefined' &&
                    typeof http_request.query.body !== 'undefined'
                ) {
                    reply.success = forum_lib.postNew(
                        http_request.query.sub[0],
                        http_request.query.to[0],
                        http_request.query.subject[0],
                        http_request.query.body[0]
                    );
                } else {
                    reply.success = false;
                }
                break;

            case 'delete-message':
                if (typeof http_request.query.sub !== 'undefined' &&
                    typeof http_request.query.number !== 'undefined'
                ) {
                    reply.success = forum_lib.deleteMessage(
                        http_request.query.sub[0],
                        http_request.query.number[0]
                    );
                } else {
                    reply.success = false;
                }
                break;

            case 'delete-mail':
                if (typeof http_request.query.number !== 'undefined') {
                    reply.success = forum_lib.deleteMail(http_request.query.number);
                } else {
                    reply.success = false;
                }
                break;

            case 'set-scan-cfg':
                if (typeof http_request.query.sub !== 'undefined' &&
                    typeof http_request.query.cfg !== 'undefined'
                ) {
                    reply.success = forum_lib.setScanCfg(
                        http_request.query.sub[0],
                        http_request.query.cfg[0]
                    );
                } else {
                    reply.success = false;
                }
                break;

            case 'vote':
                if (typeof http_request.query.sub !== 'undefined' &&
                    typeof http_request.query.id !== 'undefined' &&
                    typeof http_request.query.up !== 'undefined' &&
                    !(user.security.restrictions&UFLAG_V)
                ) {
                    reply.success = forum_lib.voteMessage(
                        http_request.query.sub[0],
                        http_request.query.id[0],
                        http_request.query.up[0]
                    );
                } else {
                    reply.success = false;
                }
                break;

            case 'submit-poll-answers':
                if (typeof http_request.query.sub !== 'undefined' &&
                    typeof http_request.query.id !== 'undefined' &&
                    typeof http_request.query.answer !== 'undefined'
                ) {
                    reply.success = forum_lib.submitPollAnswers(
                        http_request.query.sub[0],
                        http_request.query.id[0],
                        http_request.query.answer
                    );
                }
                break;

            case 'submit-poll':
                if (typeof http_request.query.subject !== 'undefined' &&
                    typeof http_request.query.sub !== 'undefined' &&
                    typeof http_request.query.votes !== 'undefined' &&
                    typeof http_request.query.results !== 'undefined' &&
                    typeof http_request.query.answer !== 'undefined'
                ) {
                    reply.success = forum_lib.postPoll(
                        http_request.query.sub[0],
                        http_request.query.subject[0],
                        http_request.query.votes[0],
                        http_request.query.results[0],
                        http_request.query.answer,
                        http_request.query.comment || []
                    );
                }
                break;

            default:
                handled = false;
                break;

        }

    }

    // Unauthenticated calls
    if (!handled) {

        switch(http_request.query.call[0].toLowerCase()) {

            case 'get-thread-votes':
                if (typeof http_request.query.sub !== 'undefined' &&
                    typeof http_request.query.id !== 'undefined'
                ) {
                    var id = parseInt(http_request.query.id[0]);
                    if (!isNaN(id)) {
                        reply = forum_lib.getVotesInThread(
                            http_request.query.sub[0],
                            id
                        );
                    }
                }
                break;

            case 'get-sub-votes':
                if (typeof http_request.query.sub !== 'undefined') {
                    reply = forum_lib.getVotesInThreads(http_request.query.sub[0]);
                }
                break;

            case 'get-poll-results':
                if (typeof http_request.query.sub !== 'undefined' &&
                    typeof http_request.query.id !== 'undefined'
                ) {
                    reply = forum_lib.getUserPollData(
                        http_request.query.sub[0],
                        http_request.query.id[0]
                    );
                }
                break;

            case 'list-groups':
                reply = forum_lib.listGroups();
                break;

            case 'list-subs':
                if (typeof http_request.query.group !== 'undefined') {
                    reply = forum_lib.listSubs(http_request.query.group[0]);
                }
                break;

            case 'list-threads':
                if (typeof http_request.query.sub !== 'undefined' &&
                    typeof http_request.query.offset !== 'undefined'
                ) {
                    if (typeof http_request.query.count !== 'undefined') {
                        var count = http_request.query.count[0];
                    }
                    reply = forum_lib.listThreads(
                        http_request.query.sub[0],
                        http_request.query.offset[0],
                        count || settings.page_size
                    ).threads;
                }
                break;

            case 'get-group-unread-count':
                if (typeof http_request.query.group !== 'undefined') {
                    http_request.query.group.forEach(
                        function(group) {
                            reply[group] = forum_lib.getGroupUnreadCount(group);
                        }
                    );
                }
                break;

            case 'get-sub-unread-count':
                if (typeof http_request.query.sub !== 'undefined') {
                    http_request.query.sub.forEach(
                        function(sub) {
                            reply[sub] = forum_lib.getSubUnreadCount(sub);
                        }
                    );
                }
                break;

            default:
                break;

        }

    }

}

reply = JSON.stringify(reply);
http_reply.header['Content-Type'] = 'application/json';
http_reply.header['Content-Length'] = reply.length;
write(reply);
