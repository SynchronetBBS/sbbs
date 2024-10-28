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
load("funclib.js"); // for getColor() function
load(js.startup_dir + 'mrc-session.js');

js.on_exit("js.counter = 0");
js.time_limit=0;

var input_state = 'chat';
var show_nicks = false;
var stat_mode = 0; // 0 = Local Session stats (Chatters, Latency, & Mentions); 1 = Global MRC Stats

const MENTION_MARKER = ascii(15);
const INDENT_MARKER = ascii(28);

// default theme
var theme_bg_color = BG_BLUE;
var theme_fg_color = "\x01w\x01h";
var theme_2nd_color = "\x01b\x01h";
var theme_mrc_title = "\x01w\x01hMRC";

var f = new File(js.startup_dir + 'mrc-client.ini');
f.open('r');
const settings = {
    root: f.iniGetObject(),
    startup: f.iniGetObject('startup'),
    aliases: f.iniGetObject('aliases') || {},
    client: f.iniGetObject('client') || {},
    show_nicks: f.iniGetObject('show_nicks') || {},
    theme: f.iniGetObject('theme') || {}
};

f.close();
f = undefined;


const NICK_COLOURS = [
    '\x01h\x01r',
    '\x01h\x01g',
    '\x01h\x01y',
    '\x01h\x01b',
    '\x01h\x01m',
    '\x01h\x01c',
    '\x01h\x01w',
    // Low colours with reasonable contrast
    '\x01n\x01r',
    '\x01n\x01g',
    '\x01n\x01y',
    '\x01n\x01m',
    '\x01n\x01c',
    '\x01n\x01w'
];

const PIPE_COLOURS = [2, 3, 4, 5, 6, 9, 10, 11, 12, 13, 14, 15];

const ACT_LVL = [
    /* 0: NUL */ "\x01H\x01KNUL",
    /* 1: LOW */ "\x01H\x01YLOW",
    /* 2: MED */ "\x01H\x01GMED",
    /* 3: HI  */ "\x01H\x01RHI"
];

function pipe_to_ctrl_a(str) { // could use the pipeToCtrlA function in funclib.js instead
	str = str.replace(/\|00/g, "\x01N\x01K");
	str = str.replace(/\|01/g, "\x01N\x01B");
	str = str.replace(/\|02/g, "\x01N\x01G");
	str = str.replace(/\|03/g, "\x01N\x01C");
	str = str.replace(/\|04/g, "\x01N\x01R");
	str = str.replace(/\|05/g, "\x01N\x01M");
	str = str.replace(/\|06/g, "\x01N\x01Y");
	str = str.replace(/\|07/g, "\x01N\x01W");
	str = str.replace(/\|08/g, "\x01H\x01K");
	str = str.replace(/\|09/g, "\x01H\x01B");
	str = str.replace(/\|10/g, "\x01H\x01G");
	str = str.replace(/\|11/g, "\x01H\x01C");
	str = str.replace(/\|12/g, "\x01H\x01R");
	str = str.replace(/\|13/g, "\x01H\x01M");
	str = str.replace(/\|14/g, "\x01H\x01Y");
	str = str.replace(/\|15/g, "\x01H\x01W");
	str = str.replace(/\|16/g, "\x01" + 0);
	str = str.replace(/\|17/g, "\x01" + 4);
	str = str.replace(/\|18/g, "\x01" + 2);
	str = str.replace(/\|19/g, "\x01" + 6);
	str = str.replace(/\|20/g, "\x01" + 1);
	str = str.replace(/\|21/g, "\x01" + 5);
	str = str.replace(/\|22/g, "\x01" + 3);
	str = str.replace(/\|23/g, "\x01" + 7);
	return str;
}

function resize_nicklist(frames, nicks) {
    const maxlen = show_nicks ? Math.max(1, nicks.reduce(function (a, c) {
        return c.length > a ? c.length : a;
    }, 0)) : 0;
    frames.nicklist.transparent = !show_nicks;
    frames.nicklist.moveTo(frames.top.x + frames.top.width - 1 - maxlen - 1, 2);
    frames.nicks.moveTo(frames.nicklist.x + 1, 2);
    frames.nicklist.width = maxlen + 1;
    frames.nicks.width = maxlen + 1;
    frames.output.width = (frames.top.width - (show_nicks ? frames.nicklist.width : 0));
}

function redraw_nicklist(frames, nicks, colours) {
    frames.nicks.clear();
    if (show_nicks) {
        nicks.forEach(function (e, i) {
            frames.nicks.gotoxy(1, i + 1);
            frames.nicks.putmsg(colours[e] + e + '\x01n\x01w');
        });
    }
}

function init_display() {
    const w = console.screen_columns;
    const h = console.screen_rows;
    const f = { top: new Frame(1, 1, w, h, BG_BLACK|LIGHTGRAY) };
    f.title = new Frame(1, 1, w, 1, theme_bg_color|LIGHTGRAY, f.top);
    f.output = new Frame(1, 2, 1, h - 3, BG_BLACK|LIGHTGRAY, f.top);
    f.divider = new Frame(1, h - 1, w, 1, theme_bg_color|LIGHTGRAY, f.top);
    f.nicklist = new Frame(w - 2, 2, 2, h - 3, BG_BLACK|LIGHTGRAY, f.top);
    f.nicks = new Frame(w - 1, 2, 1, h - 3, BG_BLACK|LIGHTGRAY, f.nicklist);
    f.input = new Frame(1, h, w, 1, BG_BLACK|WHITE, f.top);
    f.output_scroll = new ScrollBar(f.output, { autohide: true });
    f.nick_scroll = new ScrollBar(f.nicks, { autohide: true });
    f.output.word_wrap = true;
    f.divider.gotoxy(f.divider.width - 5, 1);
    f.divider.putmsg(theme_fg_color + '/help');
    f.top.open();
    return f;
}

function refresh_stats(frames, session) {
    if (input_state == 'chat' ) {            
        frames.divider.clear();
        if (stat_mode == 0) {
            frames.divider.putmsg(format(theme_fg_color + "CLIENT STATS" + theme_2nd_color + "> " + 
                theme_fg_color + "Latency" +  theme_2nd_color + " [" + theme_fg_color + "%d" + theme_2nd_color + "] " +
                theme_fg_color + "Chatters" + theme_2nd_color + " [" + theme_fg_color + "%d" + theme_2nd_color + "] " +
                theme_fg_color + "Mentions" + theme_2nd_color + " [" + theme_fg_color + "%d" + theme_2nd_color + "]",
                session.latency,
                session.nicks.length,
                session.mention_count
            ));        
        } else {
            frames.divider.putmsg(format(theme_fg_color + "GLOBAL STATS" + theme_2nd_color + "> " + 
                theme_fg_color + "BBSes" +    theme_2nd_color + " [" + theme_fg_color + "%d" + theme_2nd_color + "] " +
                theme_fg_color + "Rooms" +    theme_2nd_color + " [" + theme_fg_color + "%d" + theme_2nd_color + "] " +
                theme_fg_color + "Users" +    theme_2nd_color + " [" + theme_fg_color + "%d" + theme_2nd_color + "] " +
                theme_fg_color + "Activity" + theme_2nd_color + " [%s"                       + theme_2nd_color + "]",
                session.stats[0],
                session.stats[1],
                session.stats[2],
                ACT_LVL[Number(session.stats[3])]
            ));
        }
        frames.divider.gotoxy(frames.divider.width - 5, 1);
        frames.divider.putmsg(theme_fg_color + '/help');
    }
}

function append_message(frames, msg, mention) {
    const top = frames.output.offset.y;
    if (frames.output.data_height > frames.output.height) {
        while (frames.output.down()) {
            yield();
        }
        frames.output.gotoxy(1, frames.output.height);
    }

    // programmatically insert indent(s), if the message length exceeds the frame width.
    if (strlen(msg) >= frames.output.width - 8) {
        var indent = "\r\n" + (new Array(7).join( " " )) + INDENT_MARKER + " ";
        msg = lfexpand(word_wrap(msg, frames.output.width - 9)).replace(/\r\n$/g, "").replace(/\r\n/g, indent).trim();
    }

    frames.output.putmsg(
        (mention ? "\x01k\x017" : "\x01k\x01h") + getShortTime(new Date()) + // timestamp formatting
        (mention ? ( "\x01n\x01r\x01h\x01i" + MENTION_MARKER ) : " ") +      // mention formatting
        "\x01n" + msg + '\r\n'                                               // message itself
    );
    frames.output.scroll(0, -1);
    if (input_state == 'scroll') frames.output.scrollTo(0, top);
}

function getShortTime(d) {
    return format( "%02d:%02d", d.getHours(), d.getMinutes() );
}

function display_message(frames, msg, mention) { //, colour param was listed but not used..     
    const body = pipe_to_ctrl_a(truncsp(msg.body) || '').split(' ');
    append_message(frames, body[0] + '\x01n\x01w ' + body.slice(1).join(' ') + '\x01n\x01w', mention);
}

function display_server_message(frames, msg) {
    append_message(frames, '\x01h\x01w' + pipe_to_ctrl_a(truncsp(msg) || ''));
}

function display_title(frames, room, title) {
    frames.title.clear();
    frames.title.putmsg(theme_mrc_title + theme_fg_color + " " + theme_2nd_color + "#" + theme_fg_color + room + theme_2nd_color + ':' + theme_fg_color + title);
}

function set_alias(alias) {
    save_setting(user.alias, 'aliases', alias);
}

function new_alias() {
    const prefix = format("|03%s", "<");
    const suffix = format("|03%s", ">");
    const alias = prefix + format(
        '|%02d%s',
        PIPE_COLOURS[Math.floor(Math.random() * PIPE_COLOURS.length)],
        user.alias.replace(/\s/g, '_')
    ) + suffix;
    set_alias(alias);
    settings.aliases[user.alias] = alias;
}

function list_themes () {
    var theme_list = [];
    var theme_files = directory(backslash(js.startup_dir) + "mrc-theme-*.ini");
    for (var t = 0; t < theme_files.length; t++) {
        theme_list.push(file_getname(theme_files[t].replace("mrc-theme-", "").replace(".ini", "")));
    }
    return theme_list.join(", ");
}

function set_theme (theme_name) {
    var f = new File(js.startup_dir + format("mrc-theme-%s.ini", theme_name));
    if ( f.open('r') ) {
        const theme = f.iniGetObject();
        f.close();
        f = undefined;
        theme_bg_color = getColor(theme.theme_bg_color);
        theme_fg_color = "\x01n" + getColor(getColor(theme.theme_fg_color));    // call 2x to get the CTRL_A color
        theme_2nd_color = "\x01n" + getColor(getColor(theme.theme_2nd_color));  // for the fb and 2nd colors.
        theme_mrc_title = theme.theme_mrc_title;
        save_setting(user.alias, "theme", theme_name);
    }
}

function save_setting (alias, setting_name, setting_value) {
    const f = new File(js.startup_dir + 'mrc-client.ini');
    f.open('r+');
    f.iniSetValue(setting_name, alias, setting_value);
    f.close();
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

    if (settings.show_nicks[user.alias]) {
        show_nicks = settings.show_nicks[user.alias];
    }

    if (settings.theme[user.alias]) {
        set_theme(settings.theme[user.alias]);
    }

    const frames = init_display();
    const inputline = new InputLine(frames.input);
    inputline.show_cursor = true;
    inputline.max_buffer = 140;

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
            display_server_message(frames, format('\x01h\x01w/\x01h\x01c%s \x01h\x01w- \x01n\x01w%s', cmd, help));
        }
    });
    session.on('local_help', function (msg) {
        display_server_message(frames, '\x01h\x01w/\x01h\x01cscroll \x01h\x01w- \x01n\x01wScroll the output area');
        display_server_message(frames, '\x01h\x01w/\x01h\x01cscroll_nicks \x01h\x01w- \x01n\x01wScroll the nicklist');
        display_server_message(frames, '\x01h\x01w/\x01h\x01cnick_prefix \x01h\x01w- \x01n\x01wSet a single-character prefix for your handle, eg. /nick_prefix @');
        display_server_message(
            frames,
            '\x01h\x01w/\x01h\x01cnick_color \x01h\x01w- \x01n\x01wSet your nick color to one of '
            + PIPE_COLOURS.reduce(function (a, c) {
                a += format('|%02d%s ', c, c);
                return a;
            }, '')
            + ', eg. /nick_color 11'
        );
        display_server_message(frames, '\x01h\x01w/\x01h\x01cnick_suffix \x01h\x01w- \x01n\x01wSet an eight-character suffix for your handle, eg. /nick_suffix <poop>');
        display_server_message(frames, '\x01h\x01w/\x01h\x01ctoggle_nicks \x01h\x01w- \x01n\x01wShow/hide the nicklist');
        display_server_message(frames, '\x01h\x01w/\x01h\x01ctoggle_stats \x01h\x01w- \x01n\x01wSwitch between global MRC stats and client/session stats.');
        display_server_message(frames, "\x01h\x01w/\x01h\x01ctheme \x01n\x01w- \x01nSelect a theme: " + list_themes() );
        display_server_message(frames, '\x01h\x01w/\x01h\x01cquit \x01n\x01w- \x01h\x01wExit the program');
    });
    session.on('message', function (msg) {
        if (msg.from_user == 'SERVER') {
            display_server_message(frames, msg.body);
        } else {
            var mention = false;
            if (msg.body.toUpperCase().indexOf(user.alias.toUpperCase()) >= 0 && msg.from_user !== user.alias) {
                console.beep();
                mention = true;
                session.mention_count = session.mention_count + 1;
                refresh_stats (frames, session);
            }            
            display_message(frames, msg, mention); //, nick_colours[msg.from_user] || '');
        }
    });
    session.on('nicks', function (nicks) {
        nicks.forEach(function (e, i) {
            nick_colours[e] = NICK_COLOURS[i % NICK_COLOURS.length];
        });
        resize_nicklist(frames, nicks);
        redraw_nicklist(frames, nicks, nick_colours);
        refresh_stats (frames, session);
    });
    session.on('sent_privmsg', function (user, msg) {
        display_message(frames, { body: '--> ' + (nick_colours[user] || '') + user + ' ' + msg });
    });
    session.on('topic', function (room, topic) {
        display_title(frames, room, topic);
    });
    session.on('stats', function (stats) {
        refresh_stats(frames, session);
    });
    session.on('latency', function () {
        refresh_stats(frames, session);
    });

    if (settings.startup.motd) session.motd();
    if (settings.startup.banners) session.banners();
    if (settings.startup.room) session.join(settings.startup.room);

    var cmd, line, user_input;
    while (!js.terminated && !break_loop) {
        session.cycle();
        frames.divider.gotoxy(frames.divider.width - 16, 1);
        frames.divider.putmsg(theme_2nd_color + "[" + theme_fg_color + ("000" + inputline.buffer.length).slice(-3) + theme_2nd_color + '/' + theme_fg_color + inputline.max_buffer + theme_2nd_color + "]");
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
                            frames.divider.putmsg(theme_fg_color + 'UP/DOWN, PGUP/PGDN, HOME/END to scroll, ENTER to return');
                            break;
                        case 'scroll_nicks':
                            input_state = 'scroll_nicks';
                            frames.divider.clear();
                            frames.divider.putmsg(theme_fg_color + 'UP/DOWN to scroll nicklist, ENTER to return');
                            break;
                        case 'nick_prefix':
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
                                    var re = new RegExp('(\\|\\d\\d)*' + user.alias, 'i');
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
                        case 'toggle_nicks':
                            show_nicks = !show_nicks;
                            resize_nicklist(frames, session.nicks);
                            redraw_nicklist(frames, session.nicks, nick_colours);
                            save_setting(user.alias, "show_nicks", show_nicks);
                            break;
                        case "toggle_stats":                            
                            stat_mode = stat_mode === 0 ? 1 : 0;
                            refresh_stats(frames, session);
                            break;
                        case "theme":
                            set_theme( cmd[1] );
                            frames.title.attr = theme_bg_color|theme_fg_color;
                            frames.divider.attr = theme_bg_color|theme_fg_color;
                            display_title(frames, session.room, session.room_topic);
                            refresh_stats(frames, session);
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
                if (user_input == KEY_UP && sframe.offset.y > 0) {
                    sframe.scroll(0, -1);
                } else if (user_input == KEY_DOWN && sframe.offset.y + sframe.height < sframe.data_height) {
                    sframe.scroll(0, 1);
                } else if (user_input == KEY_PAGEUP && sframe.offset.y > 0 ) {
                    sframe.scroll(0, -1 * sframe.height);
                    frames.output_scroll.cycle();
                } else if (user_input == KEY_PAGEDN && sframe.offset.y + sframe.height < sframe.data_height ) {
                    sframe.scroll(0, sframe.height);
                    frames.output_scroll.cycle();
                } else if (user_input == KEY_HOME) {
                    sframe.scrollTo(0, 0);
                    frames.output_scroll.cycle();
                } else if (user_input == KEY_END) {
                    sframe.scrollTo(0, sframe.data_height-sframe.height-1);
                    frames.output_scroll.cycle();
                } else if (user_input == '' || user_input == 'q') {
                    frames.output.scrollTo(1, frames.output.data_height - frames.output.height-1);
                    frames.output_scroll.cycle();
                    input_state = 'chat';
                    refresh_stats(frames, session);
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
