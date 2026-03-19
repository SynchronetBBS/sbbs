/* avatarchat.ssjs - portable REST API for Avatar Chat web page
 *
 * Endpoints:
 *   GET  ?action=history&channel=main   - last N messages
 *   GET  ?action=who&channel=main       - current subscribers
 *   GET  ?action=channels               - available channels (minimal)
 *   POST ?action=send                   - send a message (auth required)
 */

var settings = load('modopts.js', 'web') || { web_directory: '../webv4' };
load(settings.web_directory + '/lib/init.js');
load(settings.web_lib + 'auth.js');
var request = require({}, settings.web_lib + 'request.js', 'request');
load('json-client.js');

http_reply.header['Content-Type'] = 'application/json';

function loadAvatarChatConfig() {
    var path = js.exec_dir + '../../../xtrn/avatar_chat/avatar_chat.ini';
    var file = new File(path);
    var config = {
        host: '127.0.0.1',
        port: 10088,
        defaultChannel: 'main',
        maxHistory: 200
    };

    if (!file.open('r')) {
        return config;
    }

    config.host = String(file.iniGetValue(null, 'host', config.host) || config.host);
    config.port = parseInt(String(file.iniGetValue(null, 'port', config.port)), 10) || config.port;
    config.defaultChannel = String(file.iniGetValue(null, 'default_channel', config.defaultChannel) || config.defaultChannel);
    config.maxHistory = parseInt(String(file.iniGetValue(null, 'max_history', config.maxHistory)), 10) || config.maxHistory;
    file.close();

    return config;
}

function sanitizeChannel(raw, fallback) {
    var channel = String(raw || fallback || 'main').replace(/[^a-zA-Z0-9_-]/g, '');
    return channel.length ? channel : (fallback || 'main');
}

function getChannel(config) {
    var raw = request.has_param('channel') ? http_request.query.channel[0] : config.defaultChannel;
    return sanitizeChannel(raw, config.defaultChannel);
}

function withClient(config, fn) {
    var client = null;
    try {
        client = new JSONClient(config.host, config.port);
        var result = fn(client);
        client.disconnect();
        return result;
    } catch (e) {
        log(LOG_ERR, 'avatarchat.ssjs: ' + e);
        if (client) {
            try { client.disconnect(); } catch (_disconnectError) {}
        }
        return { error: 'avatar chat service error' };
    }
}

var config = loadAvatarChatConfig();
var action = request.has_param('action') ? String(http_request.query.action[0]) : '';
var reply = { error: 'invalid request' };

switch (action) {
    case 'history':
        var historyChannel = getChannel(config);
        var count = 60;

        if (request.has_param('count')) {
            var requestedCount = parseInt(String(http_request.query.count[0]), 10);
            if (!isNaN(requestedCount) && requestedCount > 0 && requestedCount <= Math.max(60, config.maxHistory)) {
                count = requestedCount;
            }
        }

        reply = withClient(config, function (client) {
            var history = client.slice('chat', 'channels.' + historyChannel + '.history', -count, undefined, 1);
            var messages = [];
            var index = 0;

            if (!Array.isArray(history)) {
                history = [];
            }

            for (index = 0; index < history.length; index += 1) {
                var message = history[index];
                var nick = message && message.nick && typeof message.nick === 'object' ? message.nick : null;
                var sender = nick && nick.name ? String(nick.name) : '';
                var userNumber = 0;

                if (sender.length) {
                    try { userNumber = system.matchuser(sender) || 0; } catch (_matchError) {}
                }

                messages.push({
                    sender: sender,
                    system: nick && nick.host ? String(nick.host) : '',
                    text: message && message.str ? String(message.str) : '',
                    timestamp: message && message.time ? message.time : 0,
                    userNumber: userNumber,
                    avatar: nick && nick.avatar ? String(nick.avatar) : undefined
                });
            }

            return {
                channel: historyChannel,
                messages: messages
            };
        });
        break;

    case 'who':
        var whoChannel = getChannel(config);

        reply = withClient(config, function (client) {
            var users = [];
            var whoResult = client.who('chat', 'channels.' + whoChannel + '.messages') || {};
            var key;

            for (key in whoResult) {
                if (!Object.prototype.hasOwnProperty.call(whoResult, key)) {
                    continue;
                }

                var entry = whoResult[key];
                var nickObj = entry && entry.nick && typeof entry.nick === 'object' ? entry.nick : null;
                var nickName = nickObj && nickObj.name ? String(nickObj.name) : String(entry && entry.nick ? entry.nick : key);
                var systemName = nickObj && nickObj.host ? String(nickObj.host) : String(entry && entry.system ? entry.system : '');
                var userNumber = 0;

                if (nickName.length) {
                    try { userNumber = system.matchuser(nickName) || 0; } catch (_matchUserError) {}
                }

                users.push({
                    nick: nickName,
                    system: systemName,
                    userNumber: userNumber
                });
            }

            return {
                channel: whoChannel,
                users: users
            };
        });
        break;

    case 'channels':
        reply = {
            channels: [sanitizeChannel(config.defaultChannel, 'main')]
        };
        break;

    case 'send':
        if (http_request.method !== 'POST') {
            reply = { error: 'POST required' };
            break;
        }

        if (user.number < 1 || (settings.guest && user.alias === settings.guest)) {
            reply = { error: 'authentication required' };
            break;
        }

        var sendChannel = getChannel(config);
        var messageText = request.has_param('message') ? String(http_request.query.message[0]) : '';

        if (!messageText.length) {
            reply = { error: 'empty message' };
            break;
        }

        if (messageText.length > 1000) {
            messageText = messageText.substr(0, 1000);
        }
        messageText = messageText.replace(/[\x00-\x08\x0b\x0c\x0e-\x1f]/g, '');

        reply = withClient(config, function (client) {
            var avatarLib = load({}, 'avatar_lib.js');
            var avatarObj = avatarLib.read_localuser(user.number) || {};
            var nick = {
                name: user.alias,
                host: system.name,
                ip: user.ip_address || '0.0.0.0',
                qwkid: system.qwk_id,
                avatar: avatarObj && avatarObj.data ? String(avatarObj.data) : undefined
            };
            var packet = {
                nick: nick,
                str: messageText,
                time: Date.now()
            };
            var basePath = 'channels.' + sendChannel;

            client.write('chat', basePath + '.messages', packet, 2);
            client.push('chat', basePath + '.history', packet, 2);

            return {
                success: true,
                channel: sendChannel,
                timestamp: packet.time
            };
        });
        break;

    default:
        reply = { error: 'unknown action: ' + action };
        break;
}

writeln(JSON.stringify(reply));
