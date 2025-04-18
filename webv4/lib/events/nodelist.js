require("presence_lib.js", 'node_status');

var last_run = 0;
var frequency = 15;

const node_state = system.node_list.map(function (e, i) {
    return {
        status: -1,
        action: -1,
        aux: -1,
        extaux: -1,
        useron: -1,
        misc: -1,
        connection : -1
    };
});

const web_state = {};

function scan_nodes() {
    var change = false;
    system.node_list.forEach(function (e, i) {
        const n = system.node_list[i];
        if (n.status != node_state[i].status
            || n.misc != node_state[i].misc
            || n.action != node_state[i].action
            || n.useron != node_state[i].useron
        ) {
            change = true;
            const obj = {
                status: n.status,
		vstatus: strip_ctrl(n.vstatus),
                action: n.action,
		activity: strip_ctrl(n.activity),
                aux: n.aux,
                misc: n.misc,
                extaux: n.extaux,
                useron: n.useron,
                connection: n.connection
            };
            node_state[i] = obj;
        }
    });
    return change;
}

function scan_web() {
    var change = false;
    const sessions = directory(system.data_dir + 'user/*.web');
    sessions.forEach(function (e) {
        const base = file_getname(e).replace(file_getext(e), '');
        const un = parseInt(base, 10);
        if (isNaN(un) || un < 1 || un > system.lastuser) return;
        if (time() - file_date(e) >= settings.inactivity) {
            if (web_state[base]) {
                delete web_state[base];
                change = true;
            }
        } else {
            const action = getSessionValue(un, 'action');
            if (web_state[base] != action) {
                web_state[base] = action;
                change = true;
            }
        }
    });
    return change;
}

function scan() {
    var out = [];
    var usr = new User(1);
    const node_change = scan_nodes();
    const web_change = scan_web();
    if (node_change) {
        out = node_state.map(function (e, i) {
            if (e.status != NODE_INUSE) {
                return {
                    node: i + 1,
                    status: locale.strings.sidebar_node_list.label_waiting_for_call,
                    action: null,
                    user: null,
                    connection: ''
                };
            } else {
                usr.number = e.useron;

                return {
                    node: i + 1,
                    status: format(e.vstatus || NodeStatus[e.status], e.aux, e.extaux),
                    action: node_status(e, user.is_sysop, {exclude_username: true, exclude_connection: true}, i),
                    user: (e.misc & NODE_ANON) && !user.is_sysop ? "Anonymous" : usr.alias,
                    connection : NodeConnectionProper[e.connection] ? NodeConnectionProper[e.connection] : (e.connection + ' bps')
                };
            }
        });
    }
    if (node_change || web_change) {
        out = out.concat(Object.keys(web_state).map(function (e) {
            usr.number = parseInt(e, 10);
            return {
                node: 'W',
                action: locale.strings.sidebar_node_list.label_status_web + ' ' + web_state[e],
                user: usr.alias,
                connection: 'Web'
            };
        }));
    }
    usr = undefined;
    return out;
}

function cycle() {
    if (time() - last_run <= frequency) return;
    last_run = time();
    const data = scan();
    if (data.length) emit({ event: 'nodelist', data: JSON.stringify(data) });
}

this;
