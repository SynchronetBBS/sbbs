const forum_lib = load({}, settings.web_lib + 'forum.js');

var last_subs;
var last_groups;
var last_run = 0;
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
    emit({ event: 'forum', data: JSON.stringify({ type: evt, data: data }) });
}

function scan_groups() {
    const scan = forum_lib.getGroupUnreadCounts();
    if (!last_groups) {
        forum_emit('groups_unread', scan);
        emit({ event: 'forum', data: JSON.stringify(scan) });
    } else {
        const diff = shallow_diff(last_groups, scan);
        if (diff) forum_emit('groups_unread', scan);
    }
    last_groups = scan;
}

function scan_subs() {
    const scan = forum_lib.getSubUnreadCounts(Req.get_param('subs_unread'));
    if (!last_subs) {
        forum_emit('subs_unread', scan);
    } else {
        const diff = shallow_diff(last_subs, scan);
        if (diff) forum_emit('subs_unread', scan);
    }
    last_subs = scan;
}

function cycle() {
    if (!auth_lib.is_user()) return;
    if (time() - last_run <= frequency) return;
    last_run = time();
    if (Req.has_param('groups_unread')) scan_groups();
    if (Req.has_param('subs_unread')) scan_subs();
}

this;
