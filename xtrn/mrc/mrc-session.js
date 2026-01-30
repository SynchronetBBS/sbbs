/* $Id: mrc-session.js,v 1.2 2019/05/04 04:59:06 echicken Exp $ */

// Passes traffic between an mrc-connector.js server and a client application
// See mrc-client.js for a bad example.
function MRC_Session(host, port, user, pass, alias) {
    
    const MRC_VER = "Multi Relay Chat JS v%s 2025-12-10 [cf]"; // update date and initials [xx] with each modification
    const CTCP_ROOM = "ctcp_echo_channel";
    
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
        last_private_msg_from: "",
        show_ctcp_req: false,
        protocol_version: ""
    };

    const callbacks = {
        error: []
    };

    function send(to_user, msg_ext, to_room, body) {
        if (body == '' || body == state.alias) return;
        state.output_buffer.push({
            from_room: state.room,
            to_user: to_user,
            msg_ext: msg_ext,
            to_room: to_room,
            body: body
        });
    }

    function send_command(command) {
        send('SERVER', '', state.room, command);
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
    
    function send_ctcp(to, p, s) {       
        state.output_buffer.push({
            from_room: CTCP_ROOM,
            to_user: to,
            msg_ext: "",
            to_room: CTCP_ROOM,
            body: p + " " + user + " " + s
        });        
        mswait(20);
    }
    
    function ctcp_time(d) {
        return format("%02d/%02d/%02d %02d:%02d", d.getMonth()+1, d.getDate(), d.getFullYear().toString().substr(-2), d.getHours(), d.getMinutes());        
    }
    
    function ctcp_reply(cmd, data) {
        // Future ctcp commands can be added easily by adding another
        // string to this array, and then adding the response logic 
        // to the switch/case structure below.
        const CTCP_CMDS = [
            /* 0 */ "VERSION",    
            /* 1 */ "TIME", 
            /* 2 */ "PING", 
            /* 3 */ "CLIENTINFO"
        ];
        var reply = "";
        switch (CTCP_CMDS.indexOf(cmd)) {
            case 0:
                reply = format(MRC_VER, state.protocol_version);
                break;
            case 1:
                reply = ctcp_time(new Date());
                break;
            case 2:
                reply = data || "PONG"; // return the same data as was given, or "PONG" if nothing
                break;
            case 3:
                reply = CTCP_CMDS.join(" ");
                break;    
            default:
                reply = "Unsupported ctcp command";
                break;                    
        }
        return reply.trim();
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
        if (msg.from_user == 'SERVER' && ['', state.room].indexOf(msg.to_room) > -1) {

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
                case "PROTOCOLVERSION":
                    state.protocol_version = params;
                    break;
                default:
                    emit('message', msg);
                    break;
            }
            // refresh the user list when the SERVER announces joins and leaves
            if (msg.body.search(/\**(Joining|Leaving|Timeout|Rename|Linked|Unlink)/) > -1) {
                send_command('USERLIST');
            }
        } else if (msg.to_user == 'SERVER') {
            if (msg.body == 'LOGOFF') {
                uidx = state.nicks.indexOf(msg.from_user);
                if (uidx > -1) {
                    state.nicks.splice(uidx, 1);
                    emit('nicks', state.nicks);
                }
            } 
        } else if (msg.to_user == '' || user.toLowerCase().replace(/\s/g, '_') == msg.to_user.toLowerCase()) {            
            if (user.toLowerCase().replace(/\s/g, '_') == msg.to_user.toLowerCase()) { // fix for incoming DirectMsg it user's name contains a space which the client replaced with an underscore
                state.last_private_msg_from = msg.from_user;
            }            
            if (msg.to_room == '' || msg.to_room == state.room) {
                emit('message', msg);
            }
            if (msg.to_room == state.room && state.nicks.indexOf(msg.from_user) < 0) {
                send_command('USERLIST');
            }
        } else if (msg.to_user == 'NOTME') {            
            // refresh the user list for cases (e.g.: room changes) *not* announced by the SERVER  
            if (['', state.room].indexOf(msg.from_room) > -1 && ['', state.room].indexOf(msg.to_room) > -1) {
                emit('message', msg);
                send_command('USERLIST');
            }                       
        }
        
        if (msg.to_room === CTCP_ROOM) {             
            log ("msg.body: " + msg.body);
        
            const ctcp_data = msg.body.split(' ');
            if (ctcp_data[0] === "[CTCP]" && ctcp_data.length >= 4) {
                
                if (state.show_ctcp_req) {
                    emit('ctcp-msg', '* |14[CTCP-REQUEST] |15' + ctcp_data[3] + ' |07on |15' + ctcp_data[2] + ' |07from |10' + msg.from_user);
                }
                if (ctcp_data[2] === "*" || (ctcp_data[2].toUpperCase()===user.toUpperCase()) || (ctcp_data[2].toUpperCase()==="#"+state.room.toUpperCase()) ) {
                    send_ctcp(ctcp_data[1], "[CTCP-REPLY]", ctcp_data[3].toUpperCase() + " " + ctcp_reply(ctcp_data[3].toUpperCase(), ctcp_data.length >= 5 ? ctcp_data.slice(4).join(' ').trim() : "") );
                }
                
            } else if (ctcp_data[0] === "[CTCP-REPLY]" && ctcp_data.length >= 3) {
                if (msg.to_user.toUpperCase()===user.toUpperCase()) {
                    emit('ctcp-msg', '* |14[CTCP-REPLY] |10' + ctcp_data[1] + ' |15' + ctcp_data.slice(2).join(' ').trim());
                }
            }
        }        
    }

    this.send_room_message = function (msg) {
        send_message('', state.room, msg);
    }
    
    this.send_notme = function (msg) {
        send("NOTME", "", "", msg);
    }
    
    this.send_action = function (msg) {
        send("", "", state.room, msg);
    }

    this.send_private_messsage = function (user, msg) {
        msg = '|11(|03DirectMSG|11)|07 ' + msg;
        send_message(user, state.room, msg);
        emit('sent_privmsg', user, msg);
    }

    this.send_command = function (command, msg_ext) {
        send_command(command, msg_ext || '');
    }

    this.connect = function () {
        handle.connect(host || 'localhost', port || 50000);
        handle.send(JSON.stringify({
            username: user,
            password: pass,
            alias: state.alias
        }) + '\r\n');

        this.send_command('IAMHERE');
    }

    this.disconnect = function () {
        handle.close();
    }
    
    this.is_connected = function () {
        return handle.is_connected();
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
        if (!handle.is_connected) {
            log(LOG_INFO, 'Connection to MRC server lost for ' + user + ".");
            emit('error', "\x01r ** Connection to MRC server lost **");
            mswait(3000); // pause for 3 seconds before returning control to the client
            emit('reconnect');
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
        // (e.g. /motd) and/ or are local command shortcuts 
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
                this.send_command('USERLIST');
                this.send_command('STATS');
            }
        },
        j: { // shorthand for join 
            // TODO: is there an easier way to duplicate command functionality?
            //help: 'Move to a new room: /join room_name',
            callback: function (str) { // validate valid room name?
                str = str.replace(/^#/, '');                
                this.send_command(format('NEWROOM:%s:%s', state.room, str));
                state.room = str;
                state.nicks = [];
                this.send_command('USERLIST');
                this.send_command('STATS');
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
        t: { // shorthand for msg
            // TODO: is there an easier way to duplicate command functionality?
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
        q: { // shorthand for quit
            // TODO: is there an easier way to duplicate command functionality?
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
        },
        me: {
            //me: 'Send an action to the server',
            callback: function (str) {
                this.send_action('|15* |13' + user + ' ' + str);
            }
        },
        ctcp: {
            // outgoing ctcp request            
            callback: function (str) {                
                const cmd = str.split(' ');
                var u = cmd[0];
                if (u) {
                    if (u === "*" || u.indexOf("#") === 0) {
                        u = "";
                    }
                    send_ctcp(u, '[CTCP]', str.trim().toUpperCase());
                } 
            }     
        },
        toggle_ctcp: {
            callback: function() {
                state.show_ctcp_req = !state.show_ctcp_req;
                emit('ctcp-msg', '* |14Incoming CTCP requests are now ' + (state.show_ctcp_req ? "|15shown" : "|12hidden") + '.');
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
