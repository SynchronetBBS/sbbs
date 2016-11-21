/*  This script is an interface between HTTP clients and the functions defined
    in web/lib/forum.js.  A basic check for an authenticated, non-guest user
    is done here; otherwise all permission checking is done at the function
    level. */

load(system.exec_dir + '../web/lib/init.js');
load(settings.web_lib + 'auth.js');
load(settings.web_lib + 'forum.js');

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

            case 'list-groups':
                reply = listGroups();
                break;
            
            case 'list-subs':
                if (typeof http_request.query.group !== 'undefined') {
                    reply = listSubs(http_request.query.group[0]);
                }
                break;
            
            case 'list-threads':
                if (typeof http_request.query.sub !== 'undefined') {
                    reply = listThreads(http_request.query.sub[0]);
                }
                break;
            
            case 'get-group-unread-count':
                if (typeof http_request.query.group !== 'undefined') {
                    http_request.query.group.forEach(
                        function(group) {
                            reply[group] = getGroupUnreadCount(group);
                        }
                    );
                }
                break;

            case 'get-sub-unread-count':
                if (typeof http_request.query.sub !== 'undefined') {
                    http_request.query.sub.forEach(
                        function(sub) {
                            reply[sub] = getSubUnreadCount(sub);
                        }
                    );
                }
                break;

            case 'get-mail-unread-count':
                reply.count = getMailUnreadCount();
                break;
            
            case 'get-mail-body':
                if (typeof http_request.query.number !== 'undefined') {
                    reply = getMailBody(http_request.query.number[0]);
                }
                break;
            
            case 'get-signature':
                reply.signature = getSignature();
                break;
            
            case 'post-reply':
                if (typeof http_request.query.sub !== 'undefined' &&
                    typeof http_request.query.body !== 'undefined' &&
                    typeof http_request.query.pid !== 'undefined'
                ) {
                    reply.success = postReply(
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
                    reply.success = postNew(
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
                    reply.success = deleteMessage(
                        http_request.query.sub[0],
                        http_request.query.number[0]
                    );
                } else {
                    reply.success = false;
                }
                break;

            case 'set-scan-cfg':
                if (typeof http_request.query.sub !== 'undefined' &&
                    typeof http_request.query.cfg !== 'undefined'
                ) {
                    reply.success = setScanCfg(
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
                    reply.success = voteMessage(
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
                    reply.success = submitPollAnswers(
                        http_request.query.sub[0],
                        http_request.query.id[0],
                        http_request.query.answer
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
                        reply = getVotesInThread(
                            http_request.query.sub[0],
                            id
                        );
                    }
                }
                break;
            case 'get-sub-votes':
                if (typeof http_request.query.sub !== 'undefined') {
                    reply = getVotesInThreads(http_request.query.sub[0]);
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