load("sbbsdefs.js");
load("nodedefs.js");
const sbbsimsg = load({}, "sbbsimsg_lib.js");

var last_run = 0;
const frequency = 60;
const timeout = 2500;

function list() {
    const state = {};
    sbbsimsg.read_sys_list();
    var sent = sbbsimsg.request_active_users();
    sbbsimsg.poll_systems(sent, 0.25, timeout, function () { });
    for (var i in sbbsimsg.sys_list) {
        var sys = sbbsimsg.sys_list[i];
        if (sys.users.length) {
            state[sys.name] = { host: sys.host, users: sys.users };
        }
    }
    emit({ event: 'sbbsimsg', data: JSON.stringify(state) });
}

function cycle() {
    if (time() - last_run <= frequency) return;
    last_run = time();
    list();
}

this;
