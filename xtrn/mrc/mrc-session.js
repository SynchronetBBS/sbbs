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
        alias: alias || user,
        stats: ['-','-','-','0'],
        mention_count: 0,
        latency: '-',
        msg_color: 7,
        twit_list: [],
        last_private_msg_from: ""
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
        if (body.length + state.alias.length + 1 > 250) {
            word_wrap(body, 250 - 1 - state.alias.length).split(/\n/).forEach(function (e) {
                send(to_user, '', to_room, state.alias + ' ' + format("|%02d", state.msg_color) + e);
            });
        } else {
            send(to_user, '', to_room, state.alias + ' ' + format("|%02d", state.msg_color) + body);
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
        var uidx;
        if (msg.from_user == 'SERVER') {

            const cmd = msg.body.substr(0, msg.body.indexOf(':'));          // cmd is everything left of the first colon (:)
            const params = msg.body.substr(msg.body.indexOf(':')+1).trim(); // params are everything right of the first colon (:), including any additional colons to follow

            switch (cmd) {
                case 'BANNER':
                    emit('banner', params.replace(/^\s+/, ''));
                    break;
                case 'ROOMTOPIC':                    
                    const room = params.substr(0, params.indexOf(':'));        // room is everything left of the first colon (:)
                    const topic = params.substr(params.indexOf(':')+1).trim(); // topic is everything right of the first colon (:), including any additional colons to follow                    
                    state.room_topic = topic;
                    emit('topic', room, topic);
                    break;
                case 'USERLIST':
                    state.nicks = params.split(',');
                    emit('nicks', state.nicks);
                    break;
                case 'STATS':
                    state.stats = params.split(' ');
                    emit('stats'); 
                    break;
                case 'LATENCY':
                    state.latency = params;
                    emit('latency');
                    break;
                default:
                    emit('message', msg);
                    break;
            }
            if (msg.body.search(/just joined room/) > -1 ||
                msg.body.search(/session has timed-out/) > -1) {
                send_command('USERLIST', 'ALL');
            }
        } else if (msg.to_user == 'SERVER') {
            if (msg.body == 'LOGOFF') {
                uidx = state.nicks.indexOf(msg.from_user);
                if (uidx > -1) {
                    state.nicks.splice(uidx, 1);
                    emit('nicks', state.nicks);
                }
            } 
        } else if (msg.to_user == '' || user.toLowerCase() == msg.to_user.toLowerCase()) {
            
            if (user.toLowerCase() == msg.to_user.toLowerCase()) {
                state.last_private_msg_from = msg.from_user;
            }
            
            if (msg.to_room == '' || msg.to_room == state.room) {
                emit('message', msg);
            }
            if (msg.to_room == state.room && state.nicks.indexOf(msg.from_user) < 0) {
                send_command('USERLIST', 'ALL');
            }
        } else if (msg.to_user == 'NOTME') {
            if (msg.body.search(/left\ the\ (room|server)\.*$/ > -1)) {
                uidx = state.nicks.indexOf(msg.from_user);
                if (uidx > -1) {
                    state.nicks.splice(uidx, 1);
                    emit('nicks', state.nicks);
                }
            } /*else if (msg.body.search(/just joined room/) > -1) {
                send_command('USERLIST', 'ALL'); // moved above to msg.from_user == 'SERVER'
            }*/
            emit('message', msg);
        }
    }

    this.send_room_message = function (msg) {
        send_message('', state.room, msg);
    }
    
    this.send_notme = function (msg) {
        send("NOTME", "", "", msg);
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

        this.send_command('IAMHERE');
        mswait(20);
        this.send_command('USERLIST', 'ALL');
        mswait(20);
        this.send_command('STATS', 'ALL');
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
        
        // This section has been cleaned up significantly.
        //         
        // Server commands no longer need to be strictly defined 
        // here.
        //
        // Some commands are still defined if they have specific 
        // function calls attached to them in mrc-client.js 
        // (e.g. /motd) and or are local command shortcuts 
        // (e.g. /t and /r).
        //
        // Any command not defined here (or in mrc-client.js) will
        // be assumed to be a server command. The server will 
        // inform the user if a command is invalid.
        //
        // The "help" properties are no longer needed, since 
        // help is contained in external .msg file. They have been
        // left commended out to provide reference.
        // 
        
        help: {
            //help: 'Display this help message',
            callback: function (str) {
                emit('local_help', str);
            }
        },
        info: {
            //help: 'View information about a BBS (/info #)',
            callback: function (str) {
                this.send_command('INFO ' + str);
            }
        },
        join: {            
            //help: 'Move to a new room: /join room_name',
            callback: function (str) { // validate valid room name?
                str = str.replace(/^#/, '');                
                this.send_command(format('NEWROOM:%s:%s', state.room, str));
                state.room = str;
                state.nicks = [];
                this.send_command('USERLIST', 'ALL');
            }
        },
        motd: {
            //help: 'Display the Message of the Day'
        },
        msg: { // This is largely overtaken by /t, but we'll still handle it.
            //help: 'Send a private message: /msg nick message goes here',
            callback: function (str) {
                const cmd = str.split(' ');
                if (cmd.length > 1 && state.nicks.indexOf(cmd[0]) > -1) {
                    this.send_private_messsage(cmd[0], cmd.slice(1).join(' '));
                }
            }
        },
        t: {
            //help: 'Send a private message: /t nick message goes here',
            callback: function (str) {
                const cmd = str.split(' ');
                if (cmd.length > 1 && state.nicks.indexOf(cmd[0]) > -1) {
                    this.send_private_messsage(cmd[0], cmd.slice(1).join(' '));
                }
            }
        },
        r: {
            //help: 'Reply to last private message: /r message goes here',
            callback: function (str) {
                if (state.last_private_msg_from) {
                    this.send_private_messsage(state.last_private_msg_from, str);
                }
            }
        },        
        quote: {
            //help: 'Send a raw command to the server',
            callback: function (str) {
                this.send_command(str);
            }
        },
        quit: {
            //help: 'Quit the program',
            callback: function () {
                emit('disconnect');
                handle.close();
            }
        },
        rooms: {
            //help: 'List available rooms',
            command: 'LIST'
        },
        stats: {
            //help: 'Return anonymous server stats',
            command: 'statistics'
        },
        topic: {
            //help: 'Change the topic of the current room',
            callback: function (str) {
                this.send_command(format('NEWTOPIC:%s:%s', state.room, str));
            }
        }          
    };

    Object.keys(commands).forEach(function (e) {
        if (commands[e].callback) {
            this[e] = commands[e].callback;
        } else if (commands[e].command) {
            this[e] = function () {
                this.send_command(commands[e].command);
            }
        } else {
            this[e] = function () {
                this.send_command(e);
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
