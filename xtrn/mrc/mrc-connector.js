/* $Id: mrc-connector.js,v 1.3 2019/05/06 03:31:08 echicken Exp $ */

/**
 * Multi Relay Chat connector (multiplexer) service
 * echicken -at- bbs.electronicchicken.com
 */

load('sbbsdefs.js');
load('sockdefs.js');

if (!js.global.server) {
    server = {};
    server.socket = new Socket(SOCK_STREAM, 'MRC_PROXY');
    server.socket.bind(5000, '127.0.0.1');
    server.socket.listen();
}

js.branch_limit = 0;
server.socket.nonblocking = true;

var f = new File(js.exec_dir + 'mrc-connector.ini');
f.open('r');
const settings = f.iniGetObject();
const system_info = f.iniGetObject('info') || {};
f.close();
f = undefined;

const PROTOCOL_VERSION = '1.2.9';
const MAX_LINE = 256;
const FROM_SITE = system.qwk_id.toLowerCase();
const SYSTEM_NAME = system_info.system_name || system.name;

const clients = {};
var last_connect = 0;

// User / site name must be ASCII 33-125, no MCI, 30 chars max, underscores
function sanitize_name(str) {
    return str.replace(
        /\s/g, '_'
    ).replace(
        /[^\x21-\x7D]|(\|\w\w)/g, '' // Non-printable & MCI
    ).substr(
        0, 30
    );
}

// Room name must be ASCII 33-125, no MCI, 30 chars max
function sanitize_room(str) {
    return str.replace(/[^\x21-\x7D]|(\|\w\w)/g, '').substr(0, 30);
}

// Message text must be ASCII 32-125
function sanitize_message(str) {
    return str.replace(/[^\x20-\x7D]/g, '');
}

function strip_mci(str) {
    return str.replace(/\|\w\w/g, '');
}

function parse_message(line) {
    const msg = line.split('~');
    if (msg.length < 7) {
        log(LOG_ERR, 'Invalid MRC line: ' + line);
        return;
    }
    return {
        from_user: msg[0],
        from_site: msg[1],
        from_room: msg[2],
        to_user: msg[3],
        to_site: msg[4],
        to_room: msg[5],
        body: msg[6]
    };
}

function validate_message(msg) {
    return (
        typeof msg == 'object'
        && typeof msg.from_room == 'string'
        && typeof msg.to_user == 'string'
        && typeof msg.to_site == 'string'
        && typeof msg.to_room == 'string'
        && typeof msg.body == 'string'
    );
}

function client_receive(c, no_log) {
    const line = c.socket.recvline(1024, settings.timeout);
    if (!line || line == '') return;
    try {
        if (!no_log) log(LOG_DEBUG, 'From client: ' + line);
        const msg = JSON.parse(line);
        return msg;
    } catch (err) {
        log(LOG_ERR, 'Invalid message from client: ' + line);
    }
}

function client_close(sock, message) {
    sock.send(JSON.stringify(message) + '\n');
    sock.close();
}

function client_accept() {
    if (!server.socket.poll()) return;
    var c = server.socket.accept();
    if (!c) return;
    c.nonblocking = true;
    const msg = client_receive({ socket: c }, true);
    if (!msg) return;
    if (!msg.username || !msg.password) {
        log(LOG_ERR, 'Invalid handshake from client: ' + JSON.stringify(msg));
        client_close(c, { error: 'Invalid handshake' });
        return;
    }
    const un = system.matchuser(msg.username);
    if (!un) {
        log(LOG_ERR, 'Invalid username from client: ' + msg.username);
        client_close(c, { error: 'Invalid username' }); // Leaks user existence
        return;
    }
    var u = new User(un);
    if (msg.password != u.security.password) {
        log(LOG_ERR, 'Invalid password for: ' + msg.username);
        client_close(c, { error: 'Invalid password' });
        return;
    }
    clients[c.descriptor] = {
        username: sanitize_name(msg.username),
        socket : c,
        ping: 0,
        alias: msg.alias.replace(/\s/g, '_')
    };
    c.sendline(JSON.stringify({ error: null }));
}

// Forward a message to all applicable clients
function client_send(message, username) {
    Object.keys(clients).forEach(function (e) {
        if (message.to_user == 'NOTME' && message.from_user == clients[e].username) return;
        if (!username || clients[e].username == username) {
            log(LOG_DEBUG, 'Forwarding message to ' + clients[e].username);
            clients[e].socket.sendline(JSON.stringify(message));
        }
    });
}

function mrc_connect(host, port) {
    if (time() - last_connect < settings.reconnect_delay) return false;
    last_connect = time();
    const sock = new Socket();
    sock.nonblocking = true;
    log(LOG_INFO, 'Connecting to ' + host + ':' + port);
    if (!sock.connect(host, port, settings.timeout)) return false;
    const platform = format(
        'SYNCHRONET/%s_%s/%s',
        system.platform, system.architecture, PROTOCOL_VERSION
    ).replace(/\s/g, '_');
    const line = SYSTEM_NAME + '~' + platform;
    log(LOG_DEBUG, 'To MRC: ' + line);
    sock.send(line + '\n');
    while (sock.is_connected) {
        yield();
        // Just waiting for the HELLO line; we could verify, but meh
        if (!sock.data_waiting) continue;
        log(LOG_DEBUG, 'From MRC: ' + sock.recvline(512, settings.timeout));
        break;
    }
    // Could check if HTTPS is available and if SSH is enabled (sbbs.ini)
    mrc_send_info(sock, 'WEB', system_info.web || 'http://' + system.inet_addr + '/');
    mrc_send_info(sock, 'TEL', system_info.telnet || system.inet_addr);
    mrc_send_info(sock, 'SSH', system_info.ssh || system.inet_addr);
    mrc_send_info(sock, 'SYS', system_info.sysop || system.operator);
    mrc_send_info(sock, 'DSC', system_info.description || system.name);
    return sock;
}

function mrc_send(sock, from_user, from_room, to_user, to_site, to_room, msg) {
    if (!sock.is_connected) throw new Error('Not connected.');
    const m = sanitize_message(msg);
    const line = [
        from_user,
        FROM_SITE,
        sanitize_room(from_room),
        sanitize_name(to_user || ''),
        sanitize_name(to_site || ''),
        sanitize_room(to_room || ''),
        m
    ].join('~') + '~';
    log(LOG_DEBUG, 'To MRC: ' + line);
    return sock.send(line + '\n');
}

function mrc_send_info(sock, field, str) {
    mrc_send(sock, 'CLIENT', '', 'SERVER', 'ALL', '', 'INFO' + field + ':' + str);
}

function mrc_receive(sock) {
    var line, message;
    while (sock.data_waiting) {
        line = sock.recvline(MAX_LINE, settings.timeout);
        if (!line || line == '') break;
        log(LOG_DEBUG, 'From MRC: ' + line);
        message = parse_message(line);
        if (!message) continue;
        if (message.from_user == 'SERVER' && message.body.toUpperCase() == 'PING') {
            mrc_send(sock, 'CLIENT', '', 'SERVER', 'ALL', '', 'IMALIVE:' + SYSTEM_NAME);
            return;
        }
        if (['', 'ALL', FROM_SITE].indexOf(message.to_site) > -1) {
            if (['', 'CLIENT', 'ALL', 'NOTME'].indexOf(message.to_user) > -1) {
                // Forward to all clients
                client_send(message);
            } else {
                // Send to this user
                client_send(message, message.to_user);
            }
        }
        yield();
    }
}

function main() {

    var mrc_sock;
    var die = false;
    while (!die && !js.terminated) {

        yield();
        if (!mrc_sock || !mrc_sock.is_connected) {
            mrc_sock = mrc_connect(settings.server, settings.port);
            continue;
        }

        client_accept();

        Object.keys(clients).forEach(function (e, i) {
            if (!clients[e].socket.is_connected) {
                mrc_send(mrc_sock, clients[e].username, '', 'SERVER', '', '', 'LOGOFF');
                client_send({ from_user: clients[e].username, to_user: 'SERVER', body: 'LOGOFF' }); // Notify local clients
                delete clients[e];
            } else {
                var msg;
                while (clients[e].socket.data_waiting) {
                    msg = client_receive(clients[e]);
                    if (!msg) break;
                    if (!validate_message(msg)) break; // Log
                    mrc_send(
                        mrc_sock,
                        clients[e].username,
                        msg.from_room,
                        msg.to_user,
                        msg.to_site,
                        msg.to_room,
                        msg.body
                    );
                    yield();
                }
            }
        });

        mrc_receive(mrc_sock);

    }

    log(LOG_INFO, 'Disconnecting from MRC');
    mrc_send(mrc_sock, 'CLIENT', '', 'SERVER', 'ALL', '', 'SHUTDOWN');
    const stime = time();
    while (mrc_sock.is_connected && time() - stime > settings.timeout) {
        yield();
    }
    if (mrc_sock.is_connected) mrc_sock.close();
    log(LOG_INFO, 'Disconnected.  Exiting.');

}

main();
