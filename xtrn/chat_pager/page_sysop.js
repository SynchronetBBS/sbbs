load('sbbsdefs.js');
load(js.exec_dir + 'lib.js');
load('progress-bar.js');

function get_last_queued_value(queue, valname) {
    var val, temp_val;
    while (typeof (temp_val = queue.read(valname)) !== 'undefined') {
        val = temp_val;
    }
    return val;
}

function await_page_response(settings) {
    var queue = new Queue(settings.queue.queue_name);
    var valname = "chat_" + bbs.node_num;
    var answered = false;
    var stime = system.timer;
    var utime = system.timer;
    var progress_bar = new ProgressBar(1, 2);
    progress_bar.init();
    while (
        (system.timer - stime) * 1000 < settings.terminal.wait_time &&
        console.inkey(K_NONE) == '' &&
        !answered
    ) {
        var now = system.timer;
        if (now - utime >= 1) {
            progress_bar.set_progress(
                (((now - stime) * 1000) / settings.terminal.wait_time) * 100
            );
            progress_bar.cycle();
            utime = now;
        }
        var val = get_last_queued_value(queue, valname);
        if (typeof val == 'number' && val > stime) answered = true;
        yield();
    }
    progress_bar.set_progress(100);
    progress_bar.cycle();
    progress_bar.close();
    return answered;
}

function main() {
    var settings = load_settings(js.exec_dir + 'settings.ini');
    console.clear(WHITE);
    console.home(0, 0);
    console.center(format(settings.terminal.wait_message, system.operator));
    console.crlf();
    console.center(settings.terminal.cancel_message);
    var xy = console.getxy();
    if (settings !== null && await_page_response(settings)) {
        if (settings.terminal.irc_pull) {
            bbs.exec(
                format(
                    '?irc.js -a %s %s %s',
                    settings.terminal.irc_server,
                    settings.terminal.irc_port,
                    settings.terminal.irc_channel
                )
            );
        }
    } else {
        console.gotoxy(xy);
        console.clearline();
        console.center(format(bbs.text(522).trim(), system.operator));
        console.pause();
    }
}

main();
