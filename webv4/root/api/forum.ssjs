/*  This script is an interface between HTTP clients and the functions defined
    in web/lib/forum.js.  A basic check for an authenticated, non-guest user
    is done here; otherwise all permission checking is done at the function
    level. */

var settings = load('modopts.js', 'web') || { web_directory: '../webv4' };

load(settings.web_directory + '/lib/init.js');
load(settings.web_lib + 'auth.js');
load(settings.web_lib + 'forum.js');
var request = require({}, settings.web_lib + 'request.js', 'request');

var reply = {};
var replied = false;

// There must be an API call, and the user must not be a guest or unknown
if (request.has_param('call') && (http_request.method === 'GET' || http_request.method === 'POST')) {

    var handled = false;

    // Authenticated calls
    if (user.number > 0 && user.alias !== settings.guest) {

        handled = true;

        switch (http_request.query.call[0].toLowerCase()) {

            // Unread message counts for every sub in a group
            case 'get-sub-unread-counts':
                if (request.has_param('group') && msg_area.grp_list[http_request.query.group[0]]) {
                    reply = getSubUnreadCounts(http_request.query.group[0]);
                }
                break;
            
            // Unread message counts for all groups user has access to
            case 'get-group-unread-counts':
                reply = getGroupUnreadCounts();
                break;

            case 'get-mail-unread-count':
                reply.count = user.stats.mail_waiting;
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
                if (request.has_params(['sub', 'body', 'pid'])) {
                    reply.success = postReply(http_request.query.sub[0], http_request.query.body[0], Number(http_request.query.pid[0]));
                } else {
                    reply.success = false;
                }
                break;

            case 'post':
                if (request.has_params(['sub', 'to', 'subject', 'body'])) {
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
                if (request.has_params(['sub', 'number'])) {
                    reply.success = deleteMessage(http_request.query.sub[0], http_request.query.number[0]);
                } else {
                    reply.success = false;
                }
                break;

            case 'delete-mail':
                if (request.has_param('number')) {
                    reply.success = deleteMail(http_request.query.number);
                } else {
                    reply.success = false;
                }
                break;

            case 'set-scan-cfg':
                if (request.has_params(['sub', 'cfg'])) {
                    reply.success = setScanCfg(http_request.query.sub[0], http_request.query.cfg[0]);
                } else {
                    reply.success = false;
                }
                break;

            case 'vote':
                if (request.has_params(['sub', 'id', 'up']) && !(user.security.restrictions&UFLAG_V)) {
                    reply.success = voteMessage(http_request.query.sub[0], http_request.query.id[0], http_request.query.up[0]);
                } else {
                    reply.success = false;
                }
                break;

            case 'submit-poll-answers':
                if (request.has_params(['sub', 'id', 'answer'])) {
                    reply.success = submitPollAnswers(http_request.query.sub[0], http_request.query.id[0], http_request.query.answer[0]);
                }
                break;

            case 'submit-poll':
                if (request.has_params(['subject', 'sub', 'votes', 'results', 'answer'])) {
                    reply.success = postPoll(
                        http_request.query.sub[0],
                        http_request.query.subject[0],
                        http_request.query.votes[0],
                        http_request.query.results[0],
                        http_request.query.answer,
                        http_request.query.comment || []
                    );
                }
                break;

            case 'block-sender':
                if (user.is_sysop) {
                    if (http_request.query.from) addTwit(decodeURIComponent(http_request.query.from[0]));
                    if (http_request.query.from_net) addTwit(decodeURIComponent(http_request.query.from_net[0]));
                    reply.err = null;
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
                if (request.has_params(['sub', 'id'])) {
                    var id = parseInt(request.get_param('id'), 10);
                    if (!isNaN(id)) reply = getVotesInThread(request.get_param('sub'), id);
                }
                break;

            case 'get-sub-votes':
                if (request.has_param('sub')) reply = getVotesInThreads(request.get_param('sub'));
                break;

            case 'get-poll-results':
                if (request.has_params(['sub', 'id'])) {
                    reply = getUserPollData(request.get_param('sub'), request.get_param('id'));
                }
                break;

            case 'get-thread':
                if (request.has_params(['sub', 'thread'])) {
                    http_reply.header['Content-Type'] = 'text/plain; charset=utf8';
                    getThread(request.get_param('sub'), request.get_param('thread'), function (m) {
                        writeln(JSON.stringify(m));
                    });
                    replied = true;
                }
                break;

            case 'list-groups':
                reply = listGroups();
                break;

            case 'list-subs':
                if (request.has_param('group')) reply = listSubs(request.get_param('group'));
                break;

            case 'list-threads':
                if (request.has_param('sub')) {
                    if (request.has_param('count')) var count = request.get_param('count');
                    reply = listThreads(request.get_param('sub'), count || settings.page_size, request.get_param('after'));
                }
                break;

            case 'get-newest-message-per-sub':
                if (request.has_param('group')) reply = getNewestMessagePerSub(request.get_param('group'));
                break;

            case 'get-thread-list':
                if (request.has_param('sub')) reply = getThreadList(request.get_param('sub'));
                break;

            default:
                break;

        }

    }

}

if (!replied) {
    reply = JSON.stringify(reply);
    http_reply.header['Content-Type'] = 'application/json';
    http_reply.header['Content-Length'] = reply.length;
    write(reply);
}

reply = undefined;