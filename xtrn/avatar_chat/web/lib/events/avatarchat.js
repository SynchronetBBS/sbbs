/* avatarchat.js - portable SSE bridge for Avatar Chat web page */

load('json-client.js');

function loadAvatarChatConfig() {
    var path = js.exec_dir + '../../../xtrn/avatar_chat/avatar_chat.ini';
    var file = new File(path);
    var config = {
        host: '127.0.0.1',
        port: 10088,
        defaultChannel: 'main'
    };

    if (!file.open('r')) {
        return config;
    }

    config.host = String(file.iniGetValue(null, 'host', config.host) || config.host);
    config.port = parseInt(String(file.iniGetValue(null, 'port', config.port)), 10) || config.port;
    config.defaultChannel = String(file.iniGetValue(null, 'default_channel', config.defaultChannel) || config.defaultChannel);
    file.close();

    return config;
}

function sanitizeChannel(raw, fallback) {
    var channel = String(raw || fallback || 'main').replace(/[^a-zA-Z0-9_-]/g, '');
    return channel.length ? channel : (fallback || 'main');
}

var config = loadAvatarChatConfig();
var channel = config.defaultChannel;
var client = null;
var subscribed = false;
var lastCycle = 0;
var frequency = 1;
var reconnectDelay = 5;
var lastReconnect = 0;

if (typeof http_request !== 'undefined' && http_request.query && http_request.query.channel) {
    channel = sanitizeChannel(http_request.query.channel[0], config.defaultChannel);
}

var channelPath = 'channels.' + channel + '.messages';

function connect() {
    try {
        client = new JSONClient(config.host, config.port);
        client.subscribe('chat', channelPath);
        subscribed = true;
        return true;
    } catch (e) {
        log(LOG_WARNING, 'avatarchat event: connect failed: ' + e);
        client = null;
        return false;
    }
}

function disconnect() {
    if (!client) {
        return;
    }

    try {
        if (subscribed) {
            client.unsubscribe('chat', channelPath);
        }
    } catch (_unsubscribeError) {}

    try { client.disconnect(); } catch (_disconnectError) {}
    client = null;
    subscribed = false;
}

function processUpdate(packet) {
    if (!packet || !packet.location || !packet.oper) {
        return;
    }

    var oper = String(packet.oper).toUpperCase();
    var payload = null;
    var nick = null;
    var sender = '';
    var systemName = '';
    var userNumber = 0;

    if (oper === 'WRITE') {
        payload = packet.data || {};
        nick = payload.nick && typeof payload.nick === 'object' ? payload.nick : null;
        sender = nick && nick.name ? String(nick.name) : '';
        systemName = nick && nick.host ? String(nick.host) : '';

        if (sender.length) {
            try { userNumber = system.matchuser(sender) || 0; } catch (_matchError) {}
        }

        emit({
            event: 'avatarchat',
            data: JSON.stringify({
                type: 'message',
                sender: sender,
                system: systemName,
                text: payload.str || '',
                timestamp: payload.time || Date.now(),
                userNumber: userNumber,
                avatar: nick && nick.avatar ? String(nick.avatar) : undefined
            })
        });
        return;
    }

    if (oper === 'SUBSCRIBE' || oper === 'UNSUBSCRIBE') {
        payload = packet.data || {};
        nick = payload.nick && typeof payload.nick === 'object' ? payload.nick : null;

        emit({
            event: 'avatarchat',
            data: JSON.stringify({
                type: oper === 'SUBSCRIBE' ? 'join' : 'part',
                sender: nick && nick.name ? String(nick.name) : String(payload.nick || ''),
                system: nick && nick.host ? String(nick.host) : String(payload.system || ''),
                text: '',
                timestamp: Date.now()
            })
        });
    }
}

function cycle() {
    var now = time();

    if (now - lastCycle < frequency) {
        return;
    }
    lastCycle = now;

    if (!client || !client.connected) {
        subscribed = false;
        if (now - lastReconnect < reconnectDelay) {
            return;
        }
        lastReconnect = now;
        if (!connect()) {
            return;
        }
    }

    try {
        client.cycle();
        while (client.updates.length) {
            processUpdate(client.updates.shift());
        }
    } catch (e) {
        log(LOG_WARNING, 'avatarchat event: cycle error: ' + e);
        disconnect();
    }
}

js.on_exit('try { if (typeof disconnect === "function") disconnect(); } catch(e) {}');

this;
