/* $Id: mrc-client.js,v 1.5 2019/05/05 04:52:40 echicken Exp $ */

/**
 * Multi Relay Chat Client Module
 * echicken -at- bbs.electronicchicken.com
 *
 * I started out with good intentions.  At least it works.
 *
 * -------------------------------------------------------------
 * Codefenix's comments:
 * codefenix -at- conchaos.synchro.net
 *
 * Sincere thanks and kudos to echicken for laying down the
 * foundation with a great initial working build. I've used it
 * for a long time to join weekly MRC meetups, and it has always
 * worked great.
 *
 * In 2023 I took it upon myself to start studying this code,
 * and attempted make some basic cosmetic changes I felt were
 * needed, like timestamped messages and a togglable nick list.
 *
 * In 2024, nelgin implemented SSL (BIG thanks for that!). Later
 * that same year I decided to get back into it add basic support
 * for mentions, indented line-wrapping, and basic customizable 
 * themes.
 *
 * 2025 brings the most changes yet: twit filter, browseable
 * mentions, and numerous fixes previously missed.
 *
 * I'm a big believer in MRC, and I've always tried to be 
 * respectful of the original design of this code and not make 
 * too many jarring changes. Please excuse some of the coding 
 * decisions I made that may appear less than elegant. I do hope 
 * to optimize and improve the features implemented. Thanks to 
 * everyone who has provided feedback and words of encouragement.
 *
 *
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
var paused_msg_buffer = []; 
var show_nicks = false;
var chat_sounds = false;
var stat_mode = 0; // 0 = Local Session stats (Chatters, Latency, & Mentions); 1 = Global MRC Stats

var mentions = [];
var mention_index = 0;
const MENTION_MARKER = ascii(15);
const INDENT_MARKER = ascii(28);
const NOTIF_FILE = system.data_dir + "msgs/" + format("%04d.msg", user.number);

// default theme
var theme_bg_color = BG_BLUE;
var theme_fg_color = "\x01w\x01h";
var theme_2nd_color = "\x01b\x01h";
var theme_mrc_title = "\x01w\x01hMRC";

var f = new File(js.startup_dir + 'mrc-client.ini');
if (!f.open('r')) {
    alert("Error " + f.error + " (" + strerror(f.error) + ") opening " + f.name);
    exit(1);
}

const settings = {
    root: f.iniGetObject(),
    startup: f.iniGetObject('startup'),
    aliases: f.iniGetObject('aliases') || {},
    client: f.iniGetObject('client') || {},
    show_nicks: f.iniGetObject('show_nicks') || {},
    theme: f.iniGetObject('theme') || {},
    msg_color: f.iniGetObject('msg_color') || {},
    twit_list: f.iniGetObject('twit_list') || {},
    chat_sounds: f.iniGetObject('chat_sounds') || {}
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

function init_display(msg_color) {
    const w = console.screen_columns;
    const h = console.screen_rows;
    const f = { top: new Frame(1, 1, w, h, BG_BLACK|LIGHTGRAY) };
    f.title = new Frame(1, 1, w, 1, theme_bg_color|LIGHTGRAY, f.top);
    f.output = new Frame(1, 2, 1, h - 3, BG_BLACK|LIGHTGRAY, f.top);
    f.divider = new Frame(1, h - 1, w, 1, theme_bg_color|LIGHTGRAY, f.top);
    f.nicklist = new Frame(w - 2, 2, 2, h - 3, BG_BLACK|LIGHTGRAY, f.top);
    f.nicks = new Frame(w - 1, 2, 1, h - 3, BG_BLACK|LIGHTGRAY, f.nicklist);
    f.input_color = new Frame(1, h, 1, 1, BG_BLACK|LIGHTGRAY, f.top);
    f.input = new Frame(2, h, w-2, 1, BG_BLACK|WHITE, f.top);
    f.output_scroll = new ScrollBar(f.output, { autohide: true });
    f.nick_scroll = new ScrollBar(f.nicks, { autohide: true });
    f.output.word_wrap = true;
    f.input_color.putmsg( pipeToCtrlA( format( "|%02d>", msg_color) ) );
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

function append_message(frames, msg, mention, when) {
    
    if (input_state !== "chat") {                     // pause incoming messages while scrolling.
        paused_msg_buffer.push({"msg": msg,           // we'll capture any incoming messages in the meantime
                                "mention": mention,   // and display them when done scrolling.
                                "when": new Date()});
        return;                                       
    }
    
    const top = frames.output.offset.y;
    if (frames.output.data_height > frames.output.height) {
        while (frames.output.down()) {
            yield();
        }
        frames.output.gotoxy(1, frames.output.height);
    }

    // programmatically insert indent(s), if the message length exceeds the frame width.
    if (strlen(msg) >= frames.output.width - (console.screen_columns < 132 ? 8 : 11)) {
        var indent = "\r\n" + (new Array( console.screen_columns < 132 ? 7 : 10 ).join( " " )) + INDENT_MARKER + " ";
        msg = lfexpand(word_wrap(msg, frames.output.width - 9)).replace(/\r\n$/g, "").replace(/\r\n/g, indent).trim();
    }

    frames.output.putmsg(
        (mention ? "\x01k\x017" : "\x01k\x01h") + getShortTime(when || new Date()) + // timestamp formatting
        (mention ? ( "\x01n\x01r\x01h\x01i" + MENTION_MARKER ) : " ") +      // mention formatting
        "\x01n" + msg + '\r\n'                                               // message itself
    );
    frames.output.scroll(0, -1);

    if (mention) {
        mentions.push(top+1);
    }

    if (input_state == 'scroll' || input_state == 'mentions') frames.output.scrollTo(0, top);
}

function getShortTime(d) {
    if (console.screen_columns < 132 ) {
        return format( "%02d:%02d", d.getHours(), d.getMinutes() );
    } else {
        return format( "%02d:%02d:%02d", d.getHours(), d.getMinutes(), d.getSeconds() );
    }
}

function display_message(frames, msg, mention) {
    const body = pipeToCtrlA(truncsp(msg.body) || '').split(' ');
    append_message(frames, body[0] + '\x01n\x01w ' + body.slice(1).join(' ') + '\x01n\x01w', mention);
}

function display_server_message(frames, msg) {
    append_message(frames, '\x01h\x01w' + pipeToCtrlA(truncsp(msg) || ''));
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
    return alias;
}

function list_themes() {
    var theme_list = [];
    var theme_files = directory(backslash(js.startup_dir) + "mrc-theme-*.ini");
    for (var t = 0; t < theme_files.length; t++) {
        theme_list.push(file_getname(theme_files[t].replace("mrc-theme-", "").replace(".ini", "")));
    }
    return theme_list.join(", ");
}

function set_theme(theme_name) {
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

function save_setting(alias, setting_name, setting_value) {
    const f = new File(js.startup_dir + 'mrc-client.ini');
    f.open('r+');
    f.iniSetValue(setting_name, alias, setting_value);
    f.close();
}

function manage_twits(cmd, twit, twitlist, frames) {
    switch (cmd) {
        case "add":
            if (twitlist.indexOf(twit) < 0) {
                twitlist.push(twit);
                if (twitlist.indexOf(twit) >= 0) {
                    save_setting(user.alias, "twit_list", twitlist.join(ascii(126)));
                    display_server_message(frames, "\x01n\x01c\x01h" + twit + "\x01w successfully added to Twit List\x01n.\x01n\x01w");
                }
            }
            break;
        case "del":
            var indexRmv = twitlist.indexOf(twit);
            if (indexRmv >= 0) {
                twitlist.splice(indexRmv, 1);
                if (twitlist.indexOf(twit.toLowerCase()) < 0) {
                    save_setting(user.alias, "twit_list", twitlist.join(ascii(126)));
                    display_server_message(frames, "\x01n\x01c\x01h" + twit + "\x01w successfully REMOVED from Twit List\x01n.\x01n\x01w");
                }
            }
            break;
        case "list":
            if (twitlist.length > 0) {
                display_server_message(frames, "\x01n\x01c\x01hTwit List (\x01w" + twitlist.length + "\x01c)\x01n:\x01n\x01w");
                for (var twit in twitlist ) {
                    display_server_message(frames, "\x01n\x01w - \x01h" + twitlist[twit] + "\x01n\x01w");
                }
            } else {
                display_server_message(frames, "\x01n\x01c\x01hYour Twit List is \x01wempty\x01n.\x01n\x01w");
            }
            break;
        case "clear":
            twitlist.splice(0,twitlist.length);
            save_setting(user.alias, "twit_list", "");
            display_server_message(frames, "\x01n\x01c\x01hYour Twit List is cleared.\x01n\x01w");
            break;
        default:
            display_server_message(frames, "\x01k\x01h*.:\x01n\x01w (*) Invalid twit command.\x01n\x01w");
            display_server_message(frames, "\x01k\x01h*.:\x01n\x01w     See\x01H\x01K:\x01N \x01H/\x01Chelp\x01N \x01Htwit\x01n\x01w");
            display_server_message(frames, "\x01k\x01h*.: __\x01n\x01w");
    }
}

function display_system_messages(frames) { // display local system messages (e.g.: user new email alerts, etc.)
    if (file_size(NOTIF_FILE) > 0) {
        var fnotif = new File(NOTIF_FILE);
        if ( fnotif.open('r') ) {
            const notif = fnotif.readAll();
            fnotif.close();
            fnotif = undefined;
            display_server_message(frames, "\x01k\x01h*.:\x01y\x01h :: System Notification ::\x01n\x01w");
            for (var notif_line in notif) {
                display_server_message(frames, "\x01k\x01h*.: " + notif[notif_line]);
            }
            display_server_message(frames, "\x01k\x01h*.:\x01k\x01h__\x01n\x01w");
        }
        if (chat_sounds) {
            console.beep();
        }
        file_removecase(NOTIF_FILE);
    }
}

function browse_mentions(frames) {
    if (mentions.length > 0) {
        mention_index = mentions.length-1;
        input_state = 'mentions';
        frames.divider.clear();
        frames.divider.putmsg(theme_fg_color + format('UP/DOWN to browse mentions (%d of %d), ENTER to return', mention_index+1, mentions.length ));
        frames.output.scrollTo(1, mentions[mention_index]);
        frames.output_scroll.cycle();
    }
}

function display_external_text(frames, which) {
    var text = [];
    var fhelp = new File(js.startup_dir + "mrc-" + which + ".msg");
    if ( fhelp.open('r') ) {
        text = fhelp.readAll();
        fhelp.close(); 
        for (var line in text) {
            display_server_message(frames, text[line]);
        }  
    } else {
        display_server_message(frames, "\x01w\x01h (*) Topic not found: \x01c" + which + "\x01n" );
    }    
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
    if (settings.msg_color[user.alias]) {
        session.msg_color = settings.msg_color[user.alias];
    }
    if (settings.twit_list[user.alias]) {
        session.twit_list = settings.twit_list[user.alias].split(ascii(126));
    }
    if (settings.chat_sounds[user.alias]) {
        chat_sounds = settings.chat_sounds[user.alias];
    }

    const frames = init_display(session.msg_color);
    const inputline = new InputLine(frames.input);
    inputline.show_cursor = true;
    inputline.max_buffer = 140;
    inputline.cursor_attr = BG_BLACK|session.msg_color;

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

    session.on('local_help', function (help_topic) {
        display_external_text(frames, "help-" + (help_topic ? help_topic : "main"));
        if (help_topic == "theme") {
            display_server_message(frames, "\x01n\x01w Available theme options: " + list_themes());
        }
        display_server_message(frames, "\x01k\x01h*.:\x01k\x01h__\x01n\x01w");        
    });
    session.on('message', function (msg) {
        if (msg.from_user == 'SERVER') {
            display_server_message(frames, msg.body);
        } else {
            if (session.twit_list.indexOf(msg.from_user.toLowerCase()) < 0) {
                var mention = false;
                if (msg.body.toUpperCase().indexOf(user.alias.toUpperCase()) >= 0 && msg.from_user !== user.alias) {
                    if (chat_sounds) {
                        console.beep();
                    }
                    mention = true;
                    session.mention_count = session.mention_count + 1;
                    refresh_stats(frames, session);
                }
                display_message(frames, msg, mention);
            } /*else {
                log ( "filtered message from " + msg.from_user + ".");
            }*/
        }
    });
    session.on('nicks', function (nicks) {
        nicks.forEach(function (e, i) {
            nick_colours[e] = NICK_COLOURS[i % NICK_COLOURS.length];
        });
        resize_nicklist(frames, nicks);
        redraw_nicklist(frames, nicks, nick_colours);
        refresh_stats(frames, session);
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
    session.on('ctcp-msg', function (msg) {
        display_server_message(frames, pipeToCtrlA( msg ) );
    });

    if (settings.startup.splash) display_external_text(frames, "splash");
    if (settings.startup.motd) session.motd();
    session.send_notme("|07- |11" + user.alias + " |03has arrived.");
    mswait(20);
    session.send_command('TERMSIZE:' + console.screen_columns + 'x' + console.screen_rows);
    mswait(20);
    session.send_command('BBSMETA: SecLevel(' + user.security.level + ') SysOp(' + system.operator + ')');
    mswait(20);
    session.send_command('USERIP:' + (bbs.atcode("IP") == "127.0.0.1" ? client.ip_address : bbs.atcode("IP")) );
    if (settings.startup.room) session.join(settings.startup.room);

    if (settings.startup.commands) {
        var startup_cmds = settings.startup.commands.split(',');
        for (var startup_cmd in startup_cmds) {
            session.send_command(startup_cmds[startup_cmd].toUpperCase());
        }
    }

    var cmd, line, user_input;
    var lastnodechk = time();
    while (!js.terminated && !break_loop) {
        if ((time() - lastnodechk) >= 10) {
            // Check the node "interrupt flag" once every 10 seconds
            if (system.node_list[bbs.node_num - 1].misc & NODE_INTR) {
                bbs.nodesync(); // this will display a message to to the user and disconnect
                break;
            }
            lastnodechk = time();
        }
        session.cycle();
        if (input_state == 'chat') {
            frames.divider.gotoxy(frames.divider.width - 16, 1);
            frames.divider.putmsg(theme_2nd_color + "[" + theme_fg_color + ("000" + inputline.buffer.length).slice(-3) + theme_2nd_color + '/' + theme_fg_color + inputline.max_buffer + theme_2nd_color + "]");

            if (inputline.buffer.search(/^\/(identify|register|roompass|update password|roomconfig password) ./i) == 0) {
                inputline.frame.left();
                inputline.frame.write("*"); // mask text after commands involving passwords
            } 
        }
        user_input = inputline.getkey();
        if (typeof user_input == 'string') {
            if (input_state == 'chat') {
                // Define some hotkeys
                if (user_input == KEY_LEFT || user_input == KEY_RIGHT) { // Change user's text color                  
                    session.msg_color = session.msg_color + (user_input == KEY_LEFT ? -1 : 1);
                    if (session.msg_color < 1) {
                        session.msg_color = 15;
                    } else if (session.msg_color > 15) {
                        session.msg_color = 1;
                    }
                    inputline.cursor_attr = BG_BLACK|session.msg_color;
                    frames.input_color.clear();
                    frames.input_color.putmsg( pipeToCtrlA( format( "|%02d>", session.msg_color) ) );
                    save_setting(user.alias, "msg_color", session.msg_color);
                } else if (user_input == KEY_PAGEUP) { // Shortcut for scroll
                    input_state = 'scroll';
                    if (frames.output.offset.y > 0) {
                        frames.output.scroll(0, -1 * frames.output.height);
                        frames.output_scroll.cycle();                    
                    }
                    frames.divider.clear();
                    frames.divider.putmsg(theme_fg_color + 'UP/DOWN, PGUP/PGDN, HOME/END to scroll, ENTER to return');
                } else if (user_input == KEY_UP) {
                    browse_mentions(frames);
                } else if (user_input == CTRL_D) {
                    inputline.clear();
                } else if (user_input.substring(0, 1) == '/') { // It's a command
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
                        case "sound":
                            chat_sounds = !chat_sounds;
                            display_server_message(frames, "\x01n\x01c\x01hChat sounds \x01w\x01h" + ( chat_sounds ? "ON" : "OFF" ) + "\x01c\x01h.\x01n\x01w");
                            save_setting(user.alias, "chat_sounds", chat_sounds);
                            break;                            
                        case "theme":
                            if (cmd.length == 2) {
                                set_theme( cmd[1] );
                                frames.title.attr = theme_bg_color|theme_fg_color;
                                frames.divider.attr = theme_bg_color|theme_fg_color;
                                display_title(frames, session.room, session.room_topic);
                                refresh_stats(frames, session);
                            }
                            break;
                        case "mentions":
                            browse_mentions(frames);
                            break;
                        case "twit":
                            if (cmd.length >= 2) {
                                manage_twits( cmd[1].toLowerCase(), cmd.slice(2).join(' ').toLowerCase(), session.twit_list, frames );
                            }
                            break;
                        case "?": // shortcut for "help", because why not?
                            session.send_command("help");
                            break;
                        default:
                            if (typeof session[cmd[0]] == 'function') {
                                session[cmd[0]](cmd.slice(1).join(' '));
                                // This check doesn't let commands out unless they're
                                // strictly defined in mrc-session.js, meaning we'd  have
                                // to constantly chase down new server commands each time
                                // they're added/removed.
                            } else {
                                // Just send the command to the server if it's not defined as
                                // a local command. The server will tell the user if it's not valid.
                                session.send_command(user_input.substr(1));
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
                    && [KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, 
                        KEY_HOME, KEY_END, KEY_INSERT, KEY_PAGEDN].indexOf(user_input) < 0
                ) {
                    session.send_room_message(user_input);
                }
            } else if (input_state == 'mentions') {
                if (user_input == KEY_UP || user_input == KEY_DOWN) {
                    mention_index = mention_index + (user_input == KEY_UP ? -1 : 1);
                    if (mention_index < 0) {
                        mention_index = 0;
                    } else if (mention_index > mentions.length-1) {
                        mention_index = mentions.length-1;
                    }
                    if (frames.output.offset.y !== mentions[mention_index] ) {
                        frames.output.scrollTo(1, mentions[mention_index]);
                        frames.output_scroll.cycle();
                        frames.divider.clear();
                        frames.divider.putmsg(theme_fg_color + format('UP/DOWN to browse mentions (%d of %d), ENTER to return', mention_index+1, mentions.length ));
                    }
                } else if (user_input == '' || user_input == 'q') {
                    frames.output.scrollTo(1, frames.output.data_height - frames.output.height-1);
                    frames.output_scroll.cycle();
                    input_state = 'chat';
                    session.mention_count = 0;
                    refresh_stats(frames, session);
                    if (paused_msg_buffer.length > 0) {
                        for (var pmb in paused_msg_buffer) {
                            append_message(frames, paused_msg_buffer[pmb].msg, paused_msg_buffer[pmb].mention, paused_msg_buffer[pmb].when);
                        }
                        paused_msg_buffer = [];
                    }
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
                    if (paused_msg_buffer.length > 0) {
                        for (var pmb in paused_msg_buffer) {
                            append_message(frames, paused_msg_buffer[pmb].msg, paused_msg_buffer[pmb].mention, paused_msg_buffer[pmb].when);
                        }
                        paused_msg_buffer = [];
                    }
                }
            }
        }
        if (frames.top.cycle()) {
            frames.output_scroll.cycle();
            frames.nick_scroll.cycle();
            console.gotoxy(inputline.frame.__relations__.child[0].x, inputline.frame.y);
        }

        display_system_messages(frames);

        yield();
    }
    console.clear(autopause=false); // prevent an unintended auto-pause when quitting
}

main();
