load(settings.web_lib + 'forum.js');

var last_subs;
var last_groups;
var last_threads;
var last_run = 0;
const is_real_user = is_user();
const frequency = (settings.refresh_interval || 60000) / 1000;

// Where 'a' is the previous data and 'b' is new
function shallow_diff(a, b) {
    const ret = {};
    Object.keys(a).forEach(function (e) {
        if (typeof b[e] == 'undefined') {
            ret[e] = b[e];
        } else if (a[e].scanned != b[e].scanned || a[e].total != b[e].total) {
            ret[e] = b[e];
        }
    });
    return Object.keys(ret).length ? ret : undefined;
}

function forum_emit(evt, data) {
    emit({
        event: 'forum',
        data: JSON.stringify({
            type: evt,
            data: data
        })
    });
}

function scan_groups() {
    const scan = getGroupUnreadCounts();
    if (!last_groups) {
        forum_emit('groups_unread', scan);
    } else {
        const diff = shallow_diff(last_groups, scan);
        if (diff) forum_emit('groups_unread', scan);
    }
    last_groups = scan;
}

function scan_subs(group) {
    const scan = getSubUnreadCounts(group);
    if (!last_subs) {
        forum_emit('subs_unread', scan);
    } else {
        const diff = shallow_diff(last_subs, scan);
        if (diff) forum_emit('subs_unread', scan);
    }
    last_subs = scan;
}

function scan_threads(sub) {
    const scan = getThreadStats(sub, !is_real_user);
    if (!last_threads) {
        forum_emit('threads', scan);
    } else {
        const ret = Object.keys(scan).reduce(function (a, c) {
            if (typeof last_threads[c] == 'undefined') {
                a[c] = scan[c];
            } else if (scan[c].total != last_threads[c].total) {
                a[c] = scan[c];
            } else if (scan[c].votes.total != last_threads[c].votes.total) {
                a[c] = scan[c];
            }
            return a;
        }, {});
        if (Object.keys(ret).length) forum_emit('threads', ret);
    }
    last_threads = scan;
}

function cycle() {
    if (time() - last_run <= frequency) return;
    last_run = time();
    if (is_real_user && Request.has_param('groups_unread')) scan_groups();
    if (is_real_user && Request.has_param('subs_unread')) scan_subs(Request.get_param('subs_unread'));
    if (Request.has_param('sub')) scan_threads(Request.get_param('sub'));
}

this;
