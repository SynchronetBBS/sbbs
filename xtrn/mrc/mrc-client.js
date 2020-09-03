/* $Id: mrc-client.js,v 1.5 2019/05/05 04:52:40 echicken Exp $ */

/**
 * Multi Relay Chat Client Module
 * echicken -at- bbs.electronicchicken.com
 *
 * I started out with good intentions.  At least it works.
 */

load('sbbsdefs.js');
load('frame.js');
load('scrollbar.js');
load('inputline.js');
load(js.startup_dir + 'mrc-session.js');

var input_state = 'chat';

var f = new File(js.startup_dir + 'mrc-client.ini');
f.open('r');
const settings = {
    root: f.iniGetObject(),
    startup: f.iniGetObject('startup'),
    aliases: f.iniGetObject('aliases') || {}
};
f.close();
f = undefined;

const NICK_COLOURS = [
    '\1h\1r',
    '\1h\1g',
    '\1h\1y',
    '\1h\1b',
    '\1h\1m',
    '\1h\1c',
    '\1h\1w',
    // Low colours with reasonable contrast
    '\1n\1r',
    '\1n\1g',
    '\1n\1y',
    '\1n\1m',
    '\1n\1c',
    '\1n\1w'
];

const PIPE_COLOURS = [2, 3, 4, 5, 6, 9, 10, 11, 12, 13, 14, 15];

function pipe_to_ctrl_a(str) {
	str = str.replace(/\|00/g, "\1N\1K");
	str = str.replace(/\|01/g, "\1N\1B");
	str = str.replace(/\|02/g, "\1N\1G");
	str = str.replace(/\|03/g, "\1N\1C");
	str = str.replace(/\|04/g, "\1N\1R");
	str = str.replace(/\|05/g, "\1N\1M");
	str = str.replace(/\|06/g, "\1N\1Y");
	str = str.replace(/\|07/g, "\1N\1W");
	str = str.replace(/\|08/g, "\1H\1K");
	str = str.replace(/\|09/g, "\1H\1B");
	str = str.replace(/\|10/g, "\1H\1G");
	str = str.replace(/\|11/g, "\1H\1C");
	str = str.replace(/\|12/g, "\1H\1R");
	str = str.replace(/\|13/g, "\1H\1M");
	str = str.replace(/\|14/g, "\1H\1Y");
	str = str.replace(/\|15/g, "\1H\1W");
	str = str.replace(/\|16/g, "\001" + 0);
	str = str.replace(/\|17/g, "\001" + 4);
	str = str.replace(/\|18/g, "\001" + 2);
	str = str.replace(/\|19/g, "\001" + 6);
	str = str.replace(/\|20/g, "\001" + 1);
	str = str.replace(/\|21/g, "\001" + 5);
	str = str.replace(/\|22/g, "\001" + 3);
	str = str.replace(/\|23/g, "\001" + 7);
	return str;
}

function resize_nicklist(frames, nicks) {
    const maxlen = Math.max(1, nicks.reduce(function (a, c) {
        return c.length > a ? c.length : a;
    }, 0));
    frames.nicklist.moveTo(frames.top.x + frames.top.width - 1 - maxlen - 1, 2);
    frames.nicklist_divider.moveTo(frames.nicklist.x, 2);
    frames.nicks.moveTo(frames.nicklist.x + 1, 2);
    frames.nicklist.width = maxlen + 2;
    frames.nicks.width = maxlen + 1;
    frames.output.width = frames.top.width - frames.nicklist.width;
}

function redraw_nicklist(frames, nicks, colours) {
    frames.nicks.clear();
    nicks.forEach(function (e, i) {
        frames.nicks.gotoxy(1, i + 1);
        frames.nicks.putmsg(colours[e] + e + '\1n\1w');
    });
}

function init_display() {
    const w = console.screen_columns;
    const h = console.screen_rows;
    const f = { top: new Frame(1, 1, w, h, BG_BLACK|LIGHTGRAY) };
    f.title = new Frame(1, 1, w, 1, BG_BLUE|WHITE, f.top);
    f.output = new Frame(1, 2, 1, h - 3, BG_BLACK|LIGHTGRAY, f.top);
    f.divider = new Frame(1, h - 1, w, 1, BG_BLUE|WHITE, f.top);
    f.nicklist = new Frame(w - 2, 2, 2, h - 3, BG_BLACK|LIGHTGRAY, f.top);
    f.nicklist_divider = new Frame(w - 2, 2, 1, h - 3, BG_BLACK|LIGHTGRAY, f.nicklist);
    f.nicks = new Frame(w - 1, 2, 1, h - 3, BG_BLACK|LIGHTGRAY, f.nicklist);
    f.input = new Frame(1, h, w, 1, BG_BLACK|WHITE, f.top);
    for (var n = 0; n < f.nicklist_divider.height; n++) {
        f.nicklist_divider.gotoxy(1, n + 1);
        f.nicklist_divider.putmsg(ascii(179));
    }
    f.output_scroll = new ScrollBar(f.output, { autohide: true });
    f.nick_scroll = new ScrollBar(f.nicks, { autohide: true });
    f.output.word_wrap = true;
    f.divider.gotoxy(f.divider.width - 5, 1);
    f.divider.putmsg('/help');
    f.top.open();
    return f;
}

function append_message(frames, msg) {
    const top = frames.output.offset.y;
    if (frames.output.data_height > frames.output.height) {
        while (frames.output.down()) {
            yield();
        }
        frames.output.gotoxy(1, frames.output.height);
    }
    frames.output.putmsg(msg + '\r\n');
    frames.output.scroll(0, -1);
    if (input_state == 'scroll') frames.output.scrollTo(0, top);
}

function display_message(frames, msg, colour) {
    const body = pipe_to_ctrl_a(truncsp(msg.body) || '').split(' ');
    append_message(frames, body[0] + '\1n\1w: ' + body.slice(1).join(' ') + '\1n\1w');
}

function display_server_message(frames, msg) {
    append_message(frames, '\1h\1w' + pipe_to_ctrl_a(truncsp(msg) || ''));
}

function display_title(frames, room, title) {
    frames.title.clear();
    frames.title.putmsg('MRC - #' + room + ' - ' + title);
}

function set_alias(alias) {
    const f = new File(js.startup_dir + 'mrc-client.ini');
    f.open('r+');
    f.iniSetValue('aliases', user.alias, alias);
    f.close();
}

function new_alias() {
    const alias = format(
        '|%02d%s',
        PIPE_COLOURS[Math.floor(Math.random() * PIPE_COLOURS.length)],
        user.alias.replace(/\s/g, '_')
    );
    set_alias(alias);
    settings.aliases[user.alias] = alias;
}

function main() {

    var msg;
    var break_loop = false;
    const nick_colours = {};

    const session = new MRC_Session(
        settings.root.server,
        settings.root.port,
        user.alias,
        user.security.password,
        settings.aliases[user.alias] || new_alias(user.alias)
    );

    const frames = init_display();
    const inputline = new InputLine(frames.input);
    inputline.show_cursor = true;
    inputline.max_buffer = 140 - settings.aliases[user.alias].length - 1;

    resize_nicklist(frames, []);
    redraw_nicklist(frames, []);

    session.connect();
    session.on('banner', function (msg) {
        display_server_message(frames, msg);
    });
    session.on('disconnect', function () {
        break_loop = true;
    });
    session.on('error', function (err) {
        display_message(frames, { from_user: 'ERROR', body: err });
    });
    session.on('help', function (cmd, help, ars) {
        if (!ars || user.compare_ars(ars)) {
            display_server_message(frames, format('\1h\1w/\1h\1c%s \1h\1w- \1n\1w%s', cmd, help));
        }
    });
    session.on('local_help', function (msg) {
        display_server_message(frames, '\1h\1w/\1h\1cscroll \1h\1w- \1n\1wScroll the output area');
        display_server_message(frames, '\1h\1w/\1h\1cscroll_nicks \1h\1w- \1n\1wScroll the nicklist');
        display_server_message(frames, '\1h\1w/\1h\1cnick_prefix \1h\1w- \1n\1wSet a single-character prefix for your handle, eg. /nick_prefix @');
        display_server_message(
            frames,
            '\1h\1w/\1h\1cnick_color \1h\1w- \1n\1wSet your nick color to one of '
            + PIPE_COLOURS.reduce(function (a, c) {
                a += format('|%02d%s ', c, c);
                return a;
            }, '')
            + ', eg. /nick_color 11'
        );
        display_server_message(frames, '\1h\1w/\1h\1cnick_suffix \1h\1w- \1n\1wSet an eight-character suffix for your handle, eg. /nick_suffix <poop>');
        display_server_message(frames, '\1h\1w/\1h\1cquit \1n\1w- \1h\1wExit the program');
    });
    session.on('message', function (msg) {
        if (msg.from_user == 'SERVER') {
            display_server_message(frames, msg.body);
        } else {
            display_message(frames, msg, nick_colours[msg.from_user] || '');
        }
    });
    session.on('nicks', function (nicks) {
        nicks.forEach(function (e, i) {
            nick_colours[e] = NICK_COLOURS[i % NICK_COLOURS.length];
        });
        resize_nicklist(frames, nicks);
        redraw_nicklist(frames, nicks, nick_colours);
    });
    session.on('sent_privmsg', function (user, msg) {
        display_message(frames, { body: '--> ' + (nick_colours[user] || '') + user + ' ' + msg });
    });
    session.on('topic', function (room, topic) {
        display_title(frames, room, topic);
    });

    if (settings.startup.motd) session.motd();
    if (settings.startup.banners) session.banners();
    if (settings.startup.room) session.join(settings.startup.room);

    var cmd, line, user_input;
    while (!js.terminated && !break_loop) {
        session.cycle();
        user_input = inputline.getkey();
        if (typeof user_input == 'string') {
            if (input_state == 'chat') {
                if (user_input.substring(0, 1) == '/') { // It's a command
                    cmd = user_input.split(' ');
                    cmd[0] = cmd[0].substr(1).toLowerCase();
                    switch (cmd[0]) {
                        case 'rainbow':
                            line = user_input.replace(/^\/rainbow\s/, '').split('').reduce(function (a, c) {
                                var cc = PIPE_COLOURS[Math.floor(Math.random() * PIPE_COLOURS.length)];
                                a += format('|%02d%s', cc, c);
                                return a;
                            }, '').substr(0, inputline.max_buffer).replace(/\\s\|\d*$/, '');
                            session.send_room_message(line);
                            break;
                        case 'scroll':
                            input_state = 'scroll';
                            frames.divider.clear();
                            frames.divider.putmsg('UP/DOWN to scroll, ENTER to return');
                            break;
                        case 'scroll_nicks':
                            input_state = 'scroll_nicks';
                            frames.divider.clear();
                            frames.divider.putmsg('UP/DOWN to scroll nicklist, ENTER to return');
                            break;
                        case 'nick_prefix':
                            log(LOG_DEBUG, 'nick prefix ' + JSON.stringify(cmd));
                            if (cmd.length == 2) {
                                if (cmd[1].length == 1) {
                                    settings.aliases[user.alias] = settings.aliases[user.alias].replace(/^(\|\d\d)*\S/, cmd[1]);
                                    set_alias(settings.aliases[user.alias]);
                                    session.alias = settings.aliases[user.alias];
                                } else if (cmd[1].search(/^\|\d\d\S$/) == 0) {
                                    settings.aliases[user.alias] = settings.aliases[user.alias].replace(/^(\|\d\d)*\S/, cmd[1]);
                                    set_alias(settings.aliases[user.alias]);
                                    session.alias = settings.aliases[user.alias];
                                }
                            }
                            break;
                        case 'nick_color':
                        case 'nick_colour':
                            if (cmd.length == 2) {
                                if (PIPE_COLOURS.indexOf(parseInt(cmd[1])) >= 0) {
                                    var re = new RegExp('(\|\d\d)*' + user.alias, 'i');
                                    settings.aliases[user.alias] = settings.aliases[user.alias].replace(re, format('|%02d%s', cmd[1], user.alias));
                                    set_alias(settings.aliases[user.alias]);
                                    session.alias = settings.aliases[user.alias];
                                }
                            }
                            break;
                        case 'nick_suffix':
                            if (cmd.length == 2) {
                                if (cmd[1].replace(/(\|\d\d)/g, '').length <= 8) {
                                    var re = new RegExp(user.alias + '.*$', 'i');
                                    settings.aliases[user.alias] = settings.aliases[user.alias].replace(re, user.alias + cmd[1]);
                                    set_alias(settings.aliases[user.alias]);
                                    session.alias = settings.aliases[user.alias];
                                }
                            } else { // Clearing a nick suffix
                                var re = new RegExp(user.alias + '.*$', 'i');
                                settings.aliases[user.alias] = settings.aliases[user.alias].replace(re, user.alias);
                                set_alias(settings.aliases[user.alias]);
                                session.alias = settings.aliases[user.alias];
                            }
                            break;
                        default:
                            if (typeof session[cmd[0]] == 'function') {
                                session[cmd[0]](cmd.slice(1).join(' '));
                            }
                            break;
                    }
                } else if (user_input == '\t') { // Nick completion
                    cmd = inputline.buffer.split(' ').pop().toLowerCase();
                    var nick = '';
                    session.nicks.some(function (e) {
                        if (e.substring(0, cmd.length).toLowerCase() != cmd) return false;
                        nick = e;
                        return true;
                    });
                    if (nick != '') {
                        // lol this is horrible
                        // to do: add some methods to inputline
                        for (var n = 0; n < cmd.length; n++) {
                            console.ungetstr('\b');
                            inputline.getkey();
                        }
                        for (var n = 0; n < nick.length; n++) {
                            console.ungetstr(nick[n]);
                            inputline.getkey();
                        }
                    }
                } else if ( // Regular message
                    typeof user_input == 'string' // Could be bool at this point
                    && user_input != ''
                    && [KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT].indexOf(user_input) < 0
                ) {
                    session.send_room_message(user_input);
                }
            } else if (input_state == 'scroll' || input_state == 'scroll_nicks') {
                var sframe = input_state == 'scroll' ? frames.output : frames.nicks;
                // to do: page up, page down
                if (user_input == KEY_UP && sframe.offset.y > 0) {
                    sframe.scroll(0, -1);
                } else if (user_input == KEY_DOWN && sframe.offset.y + sframe.height < sframe.data_height) {
                    sframe.scroll(0, 1);
                } else if (user_input == '' || user_input == 'q') {
                    frames.output.scrollTo(1, frames.output.data_height - frames.output.height);
                    frames.output_scroll.cycle();
                    frames.divider.clear();
                    frames.divider.gotoxy(frames.divider.width - 5, 1);
                    frames.divider.putmsg('/help');
                    input_state = 'chat';
                }
            }
        }
        if (frames.top.cycle()) {
            frames.output_scroll.cycle();
            frames.nick_scroll.cycle();
            console.gotoxy(inputline.frame.__relations__.child[0].x, inputline.frame.y);
        }
        yield();
    }

}

main();
