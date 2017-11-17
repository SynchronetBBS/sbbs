load('sbbsdefs.js');
load('frame.js');
load('progress-bar.js');
load(js.exec_dir + 'lib.js');

// Flush any existing named values (valname) and return the last one
// (I haven't checked yet, but I assume Queue is FIFO)
function get_last_queued_value(queue, valname) {
    var val, temp_val;
    while (typeof (temp_val = queue.read(valname)) !== 'undefined') {
        val = temp_val;
    }
    return val;
}

function get_node_response_time(filename) {
    return (file_exists(filename) ? (file_date(filename) * 1000) : null);
}

function await_page_response(settings, frame) {
    if (!settings.queue.disabled) {
        var queue = new Queue(settings.queue.queue_name);
        var valname = "chat_" + bbs.node_num;
    } else {
        var valname = system.temp_dir + 'syspage_response.' + bbs.node_num;
    }
    var answered = false;
    var stime = system.timer;
    var utime = system.timer;
    var progress_bar = new ProgressBar(1, 2, frame.width, frame);
    progress_bar.init();
    while (
        (system.timer - stime) * 1000 < settings.terminal.wait_time &&
        console.inkey(K_NONE, 5) == '' &&
        !answered
    ) {
        var now = system.timer;
        if (now - utime >= 1) {
            progress_bar.set_progress(
                (((now - stime) * 1000) / settings.terminal.wait_time) * 100
            );
            utime = now;
        }
        var val = (
            !settings.queue.disabled
            ? get_last_queued_value(queue, valname)
            : get_node_response_time(valname)
        );
        if (typeof val == 'number' && val > stime) answered = true;
        frame.cycle();
        bbs.node_sync();
        yield();
    }
    progress_bar.set_progress(100);
    progress_bar.cycle();
    progress_bar.close();
    return answered;
}

function main() {
    console.clear();
    var sys_stat = bbs.sys_status;
    bbs.sys_status|=SS_MOFF;
    var settings = load_settings(js.exec_dir + 'settings.ini');
    var frame = new Frame(1, 1, console.screen_columns, 4, WHITE);
    frame.center(format(settings.terminal.wait_message, system.operator));
    frame.gotoxy(1, 3);
    frame.center(settings.terminal.cancel_message);
    frame.open();
    if (settings !== null && await_page_response(settings, frame)) {
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
        frame.gotoxy(1, 3);
        frame.center(format(bbs.text(522).trim(), system.operator));
        console.pause();
        frame.close();
    }
    bbs.sys_status = sys_stat;
}

main();
