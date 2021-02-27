/*  This script is an interface between HTTP clients and the functions defined
    in web/lib/forum.js.  A basic check for an authenticated, non-guest user
    is done here; otherwise all permission checking is done at the function
    level. */

var settings = load('modopts.js', 'web') || { web_directory: '../webv4' };

load(settings.web_directory + '/lib/init.js');
load(settings.web_lib + 'auth.js');
load(settings.web_lib + 'forum.js');
load(settings.web_lib + 'request.js');

var reply = {};

// There must be an API call, and the user must not be a guest or unknown
if (Request.has_param('call') && (http_request.method === 'GET' || http_request.method === 'POST')) {

    var handled = false;

    // Authenticated calls
    if (user.number > 0 && user.alias !== settings.guest) {

        handled = true;

        switch (http_request.query.call[0].toLowerCase()) {

            // Unread message counts for every sub in a group
            case 'get-sub-unread-counts':
                if (Request.has_param('group') && msg_area.grp_list[http_request.query.group[0]]) {
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
                if (Request.has_params(['sub', 'body', 'pid'])) {
                    reply.success = postReply(http_request.query.sub[0], http_request.query.body[0], Number(http_request.query.pid[0]));
                } else {
                    reply.success = false;
                }
                break;

            case 'post':
                if (Request.has_params(['sub', 'to', 'subject', 'body'])) {
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
                if (Request.has_params(['sub', 'number'])) {
                    reply.success = deleteMessage(http_request.query.sub[0], http_request.query.number[0]);
                } else {
                    reply.success = false;
                }
                break;

            case 'delete-mail':
                if (Request.has_param('number')) {
                    reply.success = deleteMail(http_request.query.number);
                } else {
                    reply.success = false;
                }
                break;

            case 'set-scan-cfg':
                if (Request.has_params(['sub', 'cfg'])) {
                    reply.success = setScanCfg(http_request.query.sub[0], http_request.query.cfg[0]);
                } else {
                    reply.success = false;
                }
                break;

            case 'vote':
                if (Request.has_params(['sub', 'id', 'up']) && !(user.security.restrictions&UFLAG_V)) {
                    reply.success = voteMessage(http_request.query.sub[0], http_request.query.id[0], http_request.query.up[0]);
                } else {
                    reply.success = false;
                }
                break;

            case 'submit-poll-answers':
                if (Request.has_params(['sub', 'id', 'answer'])) {
                    reply.success = submitPollAnswers(http_request.query.sub[0], http_request.query.id[0], http_request.query.answer[0]);
                }
                break;

            case 'submit-poll':
                if (Request.has_params(['subject', 'sub', 'votes', 'results', 'answer'])) {
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
                if (Request.has_params(['sub', 'id'])) {
                    var id = parseInt(http_request.query.id[0]);
                    if (!isNaN(id)) reply = getVotesInThread(http_request.query.sub[0], id);
                }
                break;

            case 'get-sub-votes':
                if (Request.has_param('sub')) reply = getVotesInThreads(http_request.query.sub[0]);
                break;

            case 'get-poll-results':
                if (Request.has_params(['sub', 'id'])) {
                    reply = getUserPollData(http_request.query.sub[0], http_request.query.id[0]);
                }
                break;

            case 'get-thread':
                if (Request.has_params(['sub', 'thread'])) {
                    if (Request.has_param('count')) var count = Request.get_param('count');
                    reply = getMessageThread(Request.get_param('sub'), Request.get_param('thread'), count || settings.page_size, Request.get_param('after'));
                }
                break;

            case 'list-groups':
                reply = listGroups();
                break;

            case 'list-subs':
                if (Request.has_param('group')) reply = listSubs(http_request.query.group[0]);
                break;

            case 'list-threads':
                if (Request.has_param('sub')) {
                    if (Request.has_param('count')) var count = Request.get_param(count);
                    reply = listThreads(Request.get_param('sub'), count || settings.page_size, Request.get_param('after'));
                }
                break;

            case 'get-newest-message-per-sub':
                if (Request.has_param('group')) reply = getNewestMessagePerSub(Request.get_param('group'));
                break;

            default:
                break;

        }

    }

}

if (typeof reply === 'function') { // generator
    var r;
    http_reply.header['Content-Type'] = 'application/json';
    while ((r = reply()) !== null) {
        writeln(JSON.stringify(r));
    }
} else { // Normal reply
    reply = JSON.stringify(reply);
    http_reply.header['Content-Type'] = 'application/json';
    http_reply.header['Content-Length'] = reply.length;
    write(reply);
}