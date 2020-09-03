/* $Id: mrc-session.js,v 1.2 2019/05/04 04:59:06 echicken Exp $ */

// Passes traffic between an mrc-connector.js server and a client application
// See mrc-client.js for a bad example.
function MRC_Session(host, port, user, pass, alias) {

    const handle = new Socket();

    const state = {
        room: '',
        room_topic: '',
        nicks: [],
        output_buffer: [],
        last_ping: 0,
        last_send: 0,
        alias: alias || user
    };

    const callbacks = {
        error: []
    };

    function send(to_user, to_site, to_room, body) {
        if (body == '' || body == state.alias) return;
        state.output_buffer.push({
            from_room: state.room,
            to_user: to_user,
            to_site: to_site,
            to_room: to_room,
            body: body
        });
    }

    function send_command(command, to_site) {
        send('SERVER', to_site || '', state.room, command);
    }

    function send_message(to_user, to_room, body) {
        if (body.length + state.alias.length + 1 > 140) {
            word_wrap(body, 140 - 1 - state.alias.length).split(/\n/).forEach(function (e) {
                send(to_user, '', to_room, state.alias + ' ' + e);
            });
        } else {
            send(to_user, '', to_room, state.alias + ' ' + body);
        }
    }

    function emit() {
        if (!Array.isArray(callbacks[arguments[0]])) return;
        const rest = Array.prototype.slice.call(arguments, 1);
        callbacks[arguments[0]].forEach(function (e) {
            if (typeof e != 'function') return;
            e.apply(null, rest);
        });
    }

    function handle_message(msg) {
        if (msg.from_user == 'SERVER') {
            const params = msg.body.split(':');
            switch (params[0]) {
                case 'BANNER':
                    emit('banner', params[1].replace(/^\s+/, ''));
                    break;
                case 'ROOMTOPIC':
                    emit('topic', params[1], params.slice(2).join(' '));
                    break;
                case 'USERLIST':
                    state.nicks = params[1].split(',');
                    emit('nicks', state.nicks);
                    break;
                default:
                    emit('message', msg);
                    break;
            }
        } else if (msg.to_user == 'SERVER') {
            if (msg.body == 'LOGOFF') {
                const uidx = state.nicks.indexOf(msg.from_user);
                if (uidx > -1) {
                    state.nicks.splice(uidx, 1);
                    emit('nicks', state.nicks);
                }
            }
        } else if (msg.to_user == '' || user.toLowerCase() == msg.to_user.toLowerCase()) {
            if (msg.to_room == '' || msg.to_room == state.room) {
                emit('message', msg);
            }
            if (msg.to_room == state.room
                && state.nicks.indexOf(msg.from_user) < 0
            ) {
                send_command('USERLIST', 'ALL');
            }
        } else if (msg.to_user == 'NOTME') {
            if (msg.body.search(/left\ the\ (room|server)\.*$/ > -1)) {
                const udix = state.nicks.indexOf(msg.from_user);
                if (uidx > -1) {
                    state.nicks.splice(uidx, 1);
                    emit('nicks', state.nicks);
                }
            } else if (msg.body.search(/just joined room/) > -1) {
                send_command('USERLIST', 'ALL');
            }
            emit('message', msg);
        }
    }

    this.send_room_message = function (msg) {
        send_message('', state.room, msg);
    }

    this.send_private_messsage = function (user, msg) {
        msg = '|11(|03Private|11)|07 ' + msg;
        send_message(user, state.room, msg);
        emit('sent_privmsg', user, msg);
    }

    this.send_command = function (command, to_site) {
        send_command(command, to_site || '');
    }

    this.connect = function () {
        handle.connect(host || 'localhost', port || 50000);
        handle.send(JSON.stringify({
            username: user,
            password: pass,
            alias: state.alias
        }) + '\r\n');
        this.send_command('USERLIST', 'ALL');
    }

    this.disconnect = function () {
        handle.close();
    }

    this.on = function (evt, callback) {
        if (typeof callbacks[evt] == 'undefined') callbacks[evt] = [];
        callbacks[evt].push(callback);
        return callbacks[evt].length;
    }

    this.off = function (evt, id) {
        if (Array.isArray(callbacks[evt])) callbacks[evt][id] = null;
    }

    this.cycle = function () {
        var msg;
        while (handle.data_waiting) {
            try {
                msg = JSON.parse(handle.recvline(), 1024, 10);
                if (msg.body) handle_message(msg);
            } catch (err) {
                callbacks.error.forEach(function (e) {
                    e(err);
                });
            }
        }
        if (time() - state.last_ping >= 60) {
            this.send_command('IAMHERE');
            state.last_ping = time();
        }
        if (state.output_buffer.length && time() - state.last_send >= 1) {
            handle.send(JSON.stringify(state.output_buffer.shift()) + '\r\n');
            state.last_send = time();
        }
    }

    const commands = {
        banners: {
            help: 'List of banners from server'
        },
        chatters: {
            help: 'List current users'
        },
        connected: {
            help: 'List connected BBSs'
        },
        help: {
            help: 'Display this help message',
            callback: function (str) {
                emit('help', 'List of available commands:');
                for (var c in commands) {
                    emit('help', c, commands[c].help, commands[c].ars);
                }
                emit('local_help');
            }
        },
        info: {
            help: 'View information about a BBS (/info #)',
            callback: function (str) {
                this.send_command('INFO ' + str);
            }
        },
        join: {
            help: 'Move to a new room: /join room_name',
            callback: function (str) { // validate valid room name?
                str = str.replace(/^#/, '');
                this.send_command(format('NEWROOM:%s:%s', state.room, str));
                state.room = str;
                state.nicks = [];
                this.send_command('USERLIST', 'ALL');
            }
        },
        meetups: {
            help: 'Display information about upcoming meetups'
        },
        motd: {
            help: 'Display the Message of the Day'
        },
        msg: {
            help: 'Send a private message: /msg nick message goes here',
            callback: function (str) {
                const cmd = str.split(' ');
                if (cmd.length > 1 && state.nicks.indexOf(cmd[0]) > -1) {
                    this.send_private_messsage(cmd[0], cmd.slice(1).join(' '));
                }
            }
        },
        notify: {
            help: 'Send a notification message to the server (what?)',
            callback: function (str) {
                this.send_command('NOTIFY:' + str, 'ALL');
            }
        },
        quit: {
            help: 'Quit the program',
            callback: function () {
                handle.close();
                emit('disconnect');
            }
        },
        rooms: {
            help: 'List available rooms',
            command: 'LIST'
        },
        stats: {
            help: 'Return anonymous server stats'
        },
        topic: {
            help: 'Change the topic of the current room',
            callback: function (str) {
                this.send_command(format('NEWTOPIC:%s:%s', state.room, str));
            }
        },
        users: {
            help: 'Display list of users'
        },
        whoon: {
            help: 'Display list of users and BBSs'
        }
    };

    Object.keys(commands).forEach(function (e) {
        if (commands[e].callback) {
            this[e] = commands[e].callback;
        } else if (commands[e].command) {
            this[e] = function () {
                this.send_command(commands[e].command.toUpperCase());
            }
        } else {
            this[e] = function () {
                this.send_command(e.toUpperCase());
            }
        }
    }, this);

    Object.keys(state).forEach(function (e) {
        Object.defineProperty(this, e, {
            get: function () {
                return state[e];
            },
            set: function (v) {
                state[e] = v;
            }
        });
    }, this);

}
