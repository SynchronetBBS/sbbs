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

if (!settings.ssl)
	settings.ssl=false;

const PROTOCOL_VERSION = '1.3.5';
const MAX_LINE = 512;
const FROM_SITE = system.name.replace(/ /g, "_");
const SYSTEM_NAME = system_info.system_name || system.name;

const clients = {};
var last_connect = 0;
var latency_tracker = [];

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
        msg_ext: msg[4],
        to_room: msg[5],
        body: msg[6]
    };
}

function validate_message(msg) {
    return (
        typeof msg == 'object'
        && typeof msg.from_room == 'string'
        && typeof msg.to_user == 'string'
        && typeof msg.msg_ext == 'string'
        && typeof msg.to_room == 'string'
        && typeof msg.body == 'string'
    );
}

function request_stats(sock){
    mrc_send(sock, 'CLIENT', '', 'SERVER', '', '', 'STATS');
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
    client_send({ from_user: 'SERVER', to_user: clients[c.descriptor].username, to_room: "", body: 'PROTOCOLVERSION:' + PROTOCOL_VERSION });
}

// Forward a message to all applicable clients
function client_send(message, username) {
    Object.keys(clients).forEach(function (e) {
        if (message.to_user == 'NOTME' && message.from_user == clients[e].username) return;
        // Ignore case for users since MRC is not case sensitive in user context
        if (!username || clients[e].username.toUpperCase() == username.toUpperCase()) {
            log(LOG_DEBUG, 'Forwarding message to ' + clients[e].username);
            clients[e].socket.sendline(JSON.stringify(message));
        }
    });
}

function mrc_connect(host, port, ssl) {
    if (time() - last_connect < settings.reconnect_delay) return false;
    last_connect = time();
    const sock = new Socket();
    sock.nonblocking = true;
    log(LOG_INFO, 'Connecting to ' + host + ':' + port);
    if (!sock.connect(host, port, settings.timeout)) return false;
    if (ssl) 
        sock.ssl_session=true;

    if (ssl && port !== 5001)
        log(LOG_INFO, "If SSL is true then you probably want port 5001");
    if (!ssl && port !== 5000)
        log(LOG_INFO, "You probably want port 5000 if not using SSL");

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
    mrc_send(sock, 'CLIENT', '', 'SERVER', "", '', 'IMALIVE:' + SYSTEM_NAME);
    mrc_send(sock, 'CLIENT', '', 'SERVER', '', '', 'CAPABILITIES: MCI CTCP' + (ssl ? " SSL" : ""));
    return sock;
}

function mrc_send(sock, from_user, from_room, to_user, msg_ext, to_room, msg) {
    if (!sock.is_connected) throw new Error('Not connected.');
    const m = sanitize_message(msg);
    const line = [
        from_user,
        FROM_SITE,
        sanitize_room(from_room),
        sanitize_name(to_user || ''),
        sanitize_name(msg_ext || ''),
        sanitize_room(to_room || ''),
        m
    ].join('~') + '~';
    latency_tracker.push({"line": line.trim(), "time": Date.now()});
    log(LOG_DEBUG, 'To MRC: ' + line);
    return sock.send(line + '\n');
}

function mrc_send_info(sock, field, str) {
    mrc_send(sock, 'CLIENT', '', 'SERVER', '', '', 'INFO' + field + ':' + str);
}

function mrc_receive(sock) {
    var line, message;
    while (sock.data_waiting) {
        line = sock.recvline(MAX_LINE, settings.timeout);
        if (!line || line == '') break;
        latency_tracker.forEach(function(m) {
            if (m.line===line.trim() || (m.line.indexOf("~STATS") >= 0 && line.indexOf("~STATS") >= 0 ) ) {
                client_send({ from_user: "SERVER", to_user: 'CLIENT', to_room: "", body: 'LATENCY:' + (Date.now() - m.time) }); 
                log(LOG_DEBUG, 'Latency: ' +  (Date.now() - m.time) );
                latency_tracker = [];
            }
        });
        log(LOG_DEBUG, 'From MRC: ' + line);
        message = parse_message(line);
        if (!message) continue;
        if (message.from_user == 'SERVER' && message.body.toUpperCase() == 'PING') {
            mrc_send(sock, 'CLIENT', '', 'SERVER', "", '', 'IMALIVE:' + SYSTEM_NAME);
            return;
        }
        if (message.from_user == 'SERVER' && message.body.toUpperCase().substr(0,6) == 'STATS:') {                      
            var fMa = new File(js.exec_dir + "mrcstats.dat");
            fMa.open("w", false);
            fMa.write(message.body.substr(message.body.indexOf(':')+1).trim());
            fMa.close();
        }
        if (['', 'CLIENT', "", 'NOTME'].indexOf(message.to_user) > -1) {
            // Forward to all clients
            client_send(message);
        } else {
            // Send to this user
            client_send(message, message.to_user);
        }
        yield();
    }
}

function main() {

    var mrc_sock;
    var die = false;
    var last_stats = 0;
    while (!die && !js.terminated) {

        yield();
        if (!mrc_sock || !mrc_sock.is_connected) {
            mrc_sock = mrc_connect(settings.server, settings.port, settings.ssl);
            continue;
        }
        
        if (time() - last_stats > 20) { // TODO: consider moving to settings
            request_stats(mrc_sock);
            last_stats = time();
        }

        client_accept();

        Object.keys(clients).forEach(function (e, i) {
            if (!clients[e].socket.is_connected) {
                mrc_send(mrc_sock, clients[e].username, "", "NOTME", "", "", "|07- |12" + clients[e].username + " |04has left chat|08.");                
                mrc_send(mrc_sock, clients[e].username, '', 'SERVER', '', '', 'LOGOFF');
                client_send({ from_user: clients[e].username, to_user: 'SERVER', body: 'LOGOFF' }); // Notify local clients // is this even needed, since the SERVER handles the notify?
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
                        msg.msg_ext,
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
    mrc_send(mrc_sock, 'CLIENT', '', 'SERVER', "", '', 'SHUTDOWN');
    const stime = time();
    while (mrc_sock.is_connected && time() - stime > settings.timeout) {
        yield();
    }
    if (mrc_sock.is_connected) mrc_sock.close();
    log(LOG_INFO, 'Disconnected.  Exiting.');

}

main();
