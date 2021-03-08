require('sbbsdefs.js', 'SYS_CLOSED');
var settings = load('modopts.js', 'web') || { web_directory: '../webv4' };
load(settings.web_directory + '/lib/init.js');
load(settings.web_lib + 'auth.js');
load(settings.web_lib + 'request.js');

js.time_limit = 0;

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
        writeln(e + ': ' + (typeof obj[e] == 'object' ? JSON.stringify(obj[e]) : obj[e]));
    });
    write('\n');
    last_send = time();
}

const callbacks = {};
if (file_isdir(settings.web_lib + 'events')) {
    if (Array.isArray(http_request.query.subscribe)) {
        http_request.query.subscribe.forEach(function (e) {
            const base = file_getname(e).replace(file_getext(e), '');
            const script = settings.web_lib + 'events/' + base + '.js';
            try {
                if (file_exists(script)) callbacks[e] = load({}, script);
            } catch (err) {
                log(LOG_ERR, 'Failed to load event module ' + e + ': ' + err);
            }
        });
    }
}

ping();
while (client.socket.is_connected) {
    Object.keys(callbacks).forEach(function (e) {
        try {
            callbacks[e].cycle();
        } catch (err) {
            log(LOG_ERR, 'Callback ' + e + ' failed: ' + err);
            delete callbacks[e];
        }
    });
    mswait(1000);
    ping();
}
