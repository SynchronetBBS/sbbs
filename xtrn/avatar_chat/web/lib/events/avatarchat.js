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

function trimText(value) {
    return String(value || '').replace(/^\s+|\s+$/g, '');
}

function sanitizeChannel(raw, fallback) {
    var channel = String(raw || fallback || 'main').replace(/[^a-zA-Z0-9_-]/g, '');
    return channel.length ? channel : (fallback || 'main');
}

function sanitizeAlias(raw) {
    return trimText(String(raw || '')).replace(/[\x00-\x1f]/g, '').substr(0, 60);
}

function normalizeUpper(value) {
    return trimText(value).toUpperCase();
}

function normalizeNick(nick) {
    if (!nick || typeof nick !== 'object') {
        return null;
    }

    var name = sanitizeAlias(nick.name);
    if (!name.length) {
        return null;
    }

    return {
        name: name,
        host: trimText(nick.host),
        qwkid: normalizeUpper(nick.qwkid),
        avatar: trimText(nick.avatar)
    };
}

function isPrivateMessage(message) {
    return !!(
        message &&
        message.private &&
        message.private.to &&
        sanitizeAlias(message.private.to.name).length
    );
}

function resolvePrivatePeerNick(message, ownAlias) {
    var sender = normalizeNick(message ? message.nick : null);
    var recipient = normalizeNick(message && message.private ? message.private.to : null);

    if (!sender || !recipient) {
        return null;
    }

    if (normalizeUpper(sender.name) === normalizeUpper(ownAlias)) {
        return recipient;
    }

    return sender;
}

var config = loadAvatarChatConfig();
var channel = config.defaultChannel;
var includeMailbox = false;
var client = null;
var publicSubscribed = false;
var mailboxSubscribed = false;
var lastCycle = 0;
var frequency = 1;
var reconnectDelay = 5;
var lastReconnect = 0;
var channelPath = '';
var mailboxPath = '';

if (typeof http_request !== 'undefined' && http_request.query && http_request.query.channel) {
    channel = sanitizeChannel(http_request.query.channel[0], config.defaultChannel);
}

if (
    typeof http_request !== 'undefined' &&
    http_request.query &&
    http_request.query.mailbox &&
    user &&
    user.number > 0
) {
    includeMailbox = String(http_request.query.mailbox[0]) !== '0';
}

channelPath = 'channels.' + channel + '.messages';
mailboxPath = includeMailbox ? ('channels.' + user.alias + '.messages') : '';

function connect() {
    if (user.number < 1) {
        return false;
    }
    try {
        client = new JSONClient(config.host, config.port);
        client.subscribe('chat', channelPath);
        publicSubscribed = true;

        if (mailboxPath.length) {
            client.subscribe('chat', mailboxPath);
            mailboxSubscribed = true;
        }

        return true;
    } catch (e) {
        log(LOG_WARNING, 'avatarchat event: connect failed: ' + e);
        client = null;
        publicSubscribed = false;
        mailboxSubscribed = false;
        return false;
    }
}

function disconnect() {
    if (!client) {
        return;
    }

    try {
        if (publicSubscribed) {
            client.unsubscribe('chat', channelPath);
        }
    } catch (_unsubscribePublicError) {}

    try {
        if (mailboxSubscribed && mailboxPath.length) {
            client.unsubscribe('chat', mailboxPath);
        }
    } catch (_unsubscribeMailboxError) {}

    try { client.disconnect(); } catch (_disconnectError) {}
    client = null;
    publicSubscribed = false;
    mailboxSubscribed = false;
}

function processPublicUpdate(packet) {
    var oper = String(packet.oper).toUpperCase();
    var payload = packet.data || {};
    var nick = normalizeNick(payload.nick);
    var sender = nick && nick.name ? nick.name : '';
    var systemName = nick && nick.host ? nick.host : '';
    var userNumber = 0;
    var isSelf = false;

    if (sender.length) {
        try { userNumber = system.matchuser(sender) || 0; } catch (_matchError) {}
    }

    isSelf =
        user &&
        user.number > 0 &&
        normalizeUpper(sender) === normalizeUpper(user.alias) &&
        (!systemName.length || normalizeUpper(systemName) === normalizeUpper(system.name));

    if (oper === 'WRITE') {
        emit({
            event: 'avatarchat',
            data: JSON.stringify({
                type: 'message',
                channel: channel,
                sender: sender,
                system: systemName,
                text: payload.str || '',
                timestamp: payload.time || Date.now(),
                userNumber: userNumber,
                isSelf: isSelf,
                avatar: nick && nick.avatar ? String(nick.avatar) : undefined
            })
        });
        return;
    }

    if (oper === 'SUBSCRIBE' || oper === 'UNSUBSCRIBE') {
        emit({
            event: 'avatarchat',
            data: JSON.stringify({
                type: oper === 'SUBSCRIBE' ? 'join' : 'part',
                channel: channel,
                sender: sender,
                system: systemName,
                text: '',
                timestamp: Date.now()
            })
        });
    }
}

function processPrivateUpdate(packet) {
    var oper = String(packet.oper).toUpperCase();
    var payload = packet.data || {};
    var nick = normalizeNick(payload.nick);
    var sender = nick && nick.name ? nick.name : '';
    var systemName = nick && nick.host ? nick.host : '';
    var userNumber = 0;
    var peer = null;
    var isSelf = false;

    if (oper !== 'WRITE' || !isPrivateMessage(payload)) {
        return;
    }

    if (sender.length) {
        try { userNumber = system.matchuser(sender) || 0; } catch (_matchError) {}
    }

    isSelf =
        user &&
        user.number > 0 &&
        normalizeUpper(sender) === normalizeUpper(user.alias) &&
        (!systemName.length || normalizeUpper(systemName) === normalizeUpper(system.name));

    peer = resolvePrivatePeerNick(payload, user.alias);

    emit({
        event: 'avatarchat',
        data: JSON.stringify({
            type: 'private',
            sender: sender,
            system: systemName,
            text: payload.str || '',
            timestamp: payload.time || Date.now(),
            userNumber: userNumber,
            isSelf: isSelf,
            avatar: nick && nick.avatar ? String(nick.avatar) : undefined,
            peerName: peer && peer.name ? peer.name : sender,
            peerSystem: peer && peer.host ? peer.host : systemName,
            peerAvatar: peer && peer.avatar ? peer.avatar : (nick && nick.avatar ? String(nick.avatar) : undefined)
        })
    });
}

function processUpdate(packet) {
    if (!packet || !packet.location || !packet.oper) {
        return;
    }

    if (packet.location === channelPath) {
        processPublicUpdate(packet);
        return;
    }

    if (mailboxPath.length && packet.location === mailboxPath) {
        processPrivateUpdate(packet);
    }
}

function cycle() {
    var now = time();

    if (now - lastCycle < frequency) {
        return;
    }
    lastCycle = now;

    if (!client || !client.connected) {
        publicSubscribed = false;
        mailboxSubscribed = false;
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
