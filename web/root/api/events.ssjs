load('modopts.js');
const settings = get_mod_options('web');
load(settings.web_directory + '/lib/init.js');
load(settings.web_lib + 'auth.js');

http_reply.header['Cache-Control'] = 'no-cache';
http_reply.header['Content-type'] = 'text/event-stream';
http_reply.header['X-Accel-Buffering'] = 'no'; // probably not needed by everyone (nginx)

const keepalive = 15;
var last_send = 0;

function ping() {
    if (time() - last_send > keepalive) {
        write(': ping\n\n');
        last_send = time();
    }
}

function emit(obj) {
    Object.keys(obj).forEach(function (e) {
        write(e + ': ' + (typeof obj[e] == 'object' ? JSON.stringify(obj[e]) : obj[e]) + '\n');
    });
    write('\n');
    last_send = time();
}

const callbacks = [];
if (file_isdir(settings.web_lib + 'events')) {
    if (Array.isArray(http_request.query.subscribe)) {
        http_request.query.subscribe.forEach(function (e) {
            const base = file_getname(e).replace(file_getext(e), '');
            const script = settings.web_lib + 'events/' + base + '.js';
            if (file_exists(script)) callbacks.push(load({}, script));
        });
    }
}

ping();
while (client.socket.is_connected) {
    callbacks.forEach(function (e) {
        e.cycle();
    });
    yield();
    ping();
}
