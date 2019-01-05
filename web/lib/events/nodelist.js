var last_run = 0;
var frequency = 15;

function cycle() {
    if (time() - last_run <= frequency) return;
    last_run = time();
    log('running nodelist cycle');
    var usr = new User(1);
    reply = system.node_list.reduce(function (a, c, i) {
        if (c.status !== 3) return a;
        usr.number = c.useron;
        a.push({
            node : i,
            status : format(NodeStatus[c.status], c.aux, c.extaux),
            action : format(NodeAction[c.action], c.aux, c.extaux),
            user : usr.alias,
            connection : usr.connection
        });
        return a;
    }, []);
    for (var un = 1; un < system.lastuser; un++) {
        usr.number = un;
        if (usr.connection !== 'HTTP') continue;
        if (usr.alias === settings.guest) continue;
        if (usr.settings&USER_QUIET) continue;
        if (usr.logontime < time() - settings.inactivity) continue;
        var webAction = getSessionValue(usr.number, 'action');
        if (webAction === null) continue;
        reply.push({
            status : '',
            action : locale.strings.sidebar_node_list.label_status_column + ' ' + webAction,
            user : usr.alias,
            connection : 'W'
        });
    }
    usr = undefined;
    emit({ event: 'nodelist', data: reply });
}

this;
