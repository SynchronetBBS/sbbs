load('sbbsdefs.js');
load('frame.js');
load('scrollbar.js');
load('typeahead.js');

const cache_ttl = 300; // seconds - make configgy
const timeout = 30; // seconds - make configgyable

const type_map = {
    '0': 'File',
    '1': 'Directory',
    '2': 'CSO Phone Book Server',
    '3': 'Error',
    '4': 'BinHexed Macintosh File',
    '5': 'DOS Binary Archive',
    '6': 'UUEncoded Text File',
    '7': 'Index-Search Server',
    '8': 'Telnet',
    '9': 'Binary File',
    '+': 'Redundant Gopher Server',
    'T': 'TN3270',
    'g': 'GIF',
    'I': 'Image',
};

const state = {
    doc: [],
    item: -1,
    item_type: '1',
    history: [],
    history_idx: -1
};

const frames = {
    top: new Frame(1, 1, console.screen_columns, console.screen_rows, BG_BLACK|LIGHTGRAY)
};
frames.address_bar = new Frame(frames.top.x, frames.top.y, frames.top.width, 1, BG_BLUE|WHITE, frames.top);
frames.help = new Frame(frames.top.x, frames.top.y + 1, frames.top.width, frames.top.height - 2, BG_BLACK|LIGHTGRAY, frames.top);
frames.content = new Frame(frames.top.x, frames.top.y + 1, frames.top.width, frames.top.height - 2, BG_BLACK|LIGHTGRAY, frames.top);
frames.status_bar = new Frame(frames.top.x, frames.top.height, frames.top.width - 6, 1, BG_BLUE|WHITE, frames.top);
frames.help_bar = new Frame(frames.status_bar.x + frames.status_bar.width, frames.status_bar.y, 6, 1, BG_BLUE|WHITE, frames.top);
frames.help_bar.putmsg('H)elp');

const scrollbar = new ScrollBar(frames.content);

function get_address() {
    const typeahead = new Typeahead({
        x: 1,
        y: 1,
        prompt: 'Address: ',
        text: 'gopher.floodgap.com:70'
    });
    const ret = typeahead.getstr().split(':');
    typeahead.close();
    frames.address_bar.invalidate();
    frames.address_bar.draw();
    return {
        host: ret[0],
        port: ret[1] ? parseInt(ret[1], 10) : 70,
        selector: '',
        type: 1
    };
}

function parse_line(line) {
    line = line.split(/\t/);
    return {
        type: line[0].substr(0, 1),
        text: line[0].substr(1),
        selector: line[1],
        host: line[2],
        port: parseInt(line[3], 10)
    };
}

function go_cache(host, port, selector, type) {
    const fn = format('%sgopher_%s-%s-%s', system.temp_dir, host, port, selector.replace(/[^a-zA-Z0-9\\.\\-]/g, '_'));
    if (!file_exists(fn) || time() - file_date(fn) > cache_ttl) return false;
    const f = new File(fn);
    if (!f.open('r')) return false;
    try {
        state.doc = JSON.parse(f.read());
    } catch (err) {
        log(LOG_ERROR, err);
        f.close();
        return false;
    }
    f.close();
    if (['4', '5', '9', 'g', 'I'].indexOf(type) > -1 && !file_exists(system.temp_dir + state.doc)) return false;
    log('Cache hit for ' + fn);
    return true;
}

function go_cache_it(host, port, selector) {
    const fn = format('%sgopher_%s-%s-%s', system.temp_dir, host, port, selector.replace(/[^a-zA-Z0-9\\.\\-]/g, '_'));
    const f = new File(fn);
    if (!f.open('w')) return;
    f.write(JSON.stringify(state.doc));
    f.close();
}

function go_fetch(host, port, selector, type) {
    const socket = new Socket();
    if (!socket.connect(host, port)) return null;
    socket.sendline(selector);
    const dl_start = time();
    if (type == '1') {
        state.doc = [];
        var line;
        while (time() - dl_start < timeout && !js.terminated && socket.is_connected && (line = socket.recvline()) != '.') {
            state.doc.push(parse_line(line));
        }
    } else if (type == '0' || type == '6') {
        var fn = format('%sgopher-txt_%s-%s-%s', system.temp_dir, host, port, selector.replace(/[^a-zA-Z0-9\\.\\-]/g, '_'));
        state.doc = fn;
        var f = new File(fn);
        f.open('w');
        while (time() - dl_start < timeout && !js.terminated && socket.is_connected) {
            f.writeln(socket.recvline());
        }
        f.close();
    } else if (['4', '5', '9', 'g', 'I'].indexOf(type) > -1) {
        var fn = format(system.temp_dir + selector.replace(/[^a-zA-Z0-9\\.\\-]/g, '_'));
        state.doc = fn;
        var f = new File(fn);
        f.open('w+b');
        while (time() - dl_start < timeout && !js.terminated && socket.is_connected) {
            f.writeBin(socket.recvBin(1), 1);
        }
        f.close();
    }
    go_cache_it(host, port, selector);
}

function go_for(host, port, selector, type) {
    set_status('Loading ...');
    state.item = -1;
    state.item_type = type;
    if (!go_cache(host, port, selector, type)) go_fetch(host, port, selector, type);
    set_address(host, port, selector);
}

function is_link(str) {
    return str.search(/[0-9\+gIT]/) > -1;
}

function item_color(str) {
    var ret = '\1n\1w';
    switch (str) {
        case '0': // Text files
        case '6':
            ret = '\1h\1y';
            break;
        case '1': // Directory
            ret = '\1h\1c';
            break;
        case '2': // Unsupported
        case '7':
        case '8':
        case 'T':
            ret = '\1h\1b';
            break;
        case '4': // Downloads
        case '5':
        case '9':
        case 'g':
        case 'I':
            ret = '\1h\1g';
            break;
        case '3': // Error
            ret = '\1h\1r';
            break;
    }
    return ret;
}

function next_link() {
    var ret = state.item;
    for (var n = state.item + 1; n < state.doc.length; n++) {
        if (!is_link(state.doc[n].type)) continue;
        ret = n;
        break;
    }
    return ret;
}

function previous_link() {
    var ret = state.item;
    for (var n = state.item - 1; n >= 0; n--) {
        if (!is_link(state.doc[n].type)) continue;
        ret = n;
        break;
    }
    return ret;
}

// Determine what line of the display item l of the current document appears on
function get_row(l) {
    var row = 0;
    for (var n = 0; n <= l && n < state.doc.length; n++) {
        row += truncsp(word_wrap(state.doc[n].text)).split(/\n/).length;
    }
    return row;
}

function print_line(e, i) {
    frames.content.gotoxy(1, get_row(i));
    if (is_link(e.type)) {
        frames.content.putmsg(item_color(e.type) + e.text + '\r\n');
    } else {
        frames.content.putmsg('\1n\1w' + e.text + '\r\n');
    }
}

function scroll_to(i) {
    const row = get_row(i);
    while (row > frames.content.offset.y + frames.content.height - 1) {
        frames.content.down();
        yield();
    }
    while (row < frames.content.offset.y + 1) {
        frames.content.up();
        yield();
    }
    frames.content.gotoxy(1, row - frames.content.offset.y);
}

function lowlight(e, i) {
    scroll_to(i);
    frames.content.putmsg(item_color(e.type) + e.text + '\r\n');
}

function highlight(e, i) {
    scroll_to(i);
    frames.content.attr = BG_CYAN|WHITE;
    frames.content.putmsg(e.text + '\r\n');
    frames.content.attr = BG_BLACK|LIGHTGRAY;
    set_status(e.host + ':' + e.port + e.selector + ', type: ' + type_map[e.type]);
}

function set_address(host, port, selector) {
    frames.address_bar.clear();
    frames.address_bar.putmsg('gopher://' + host + ':' + port + selector);
}

function set_status(msg) {
    frames.status_bar.clear();
    frames.status_bar.putmsg(msg);
    frames.top.cycle();
}

function print_document() {
    frames.content.clear();
    if (state.item_type == '1') {
        state.doc.forEach(print_line);
        while (frames.content.up()) {
            yield();
        }
        state.item = next_link();
        highlight(state.doc[state.item], state.item);
    } else if (state.item_type == '0' || state.item_type == '6') {
        console.clear(BG_BLACK|LIGHTGRAY);
        console.printfile(state.doc);
        console.pause();
        go_back();
        frames.top.invalidate();
        frames.top.draw();
    } else if (['4', '5', '9', 'g', 'I'].indexOf(state.item_type) > -1) {
        console.clear(BG_BLACK|LIGHTGRAY);
        bbs.send_file(state.doc);
        go_back();
        frames.top.invalidate();
        frames.top.draw();
    }
    set_status('');
}

function go_get(host, port, selector, type) {
    state.history_idx++;
    state.history.splice(state.history_idx);
    state.history[state.history_idx] = {
        host: host,
        port: port,
        selector: selector,
        type: type
    };
    go_for(host, port, selector, type);
    print_document();
}

function go_back() {
    if (state.history_idx <= 0) return;
    state.history_idx--;
    const loc = state.history[state.history_idx];
    go_for(loc.host, loc.port, loc.selector, loc.type);
    print_document();
}

function go_forward() {
    if (state.history_idx >= state.history.length - 1) return;
    state.history_idx++;
    const loc = state.history[state.history_idx];
    go_for(loc.host, loc.port, loc.selector, loc.type);
    print_document();
}

function main() {
    frames.top.open();
    frames.top.cycle();
    scrollbar.cycle();
    go_get('gopher.floodgap.com', 70, '', '1');
    do {
        switch (state.input) {
            case '\t':
                if (state.item_type == '1') {
                    lowlight(state.doc[state.item], state.item);
                    state.item = next_link();
                    highlight(state.doc[state.item], state.item);
                }
                break;
            case '`':
                if (state.item_type == '1') {
                    lowlight(state.doc[state.item], state.item);
                    state.item = previous_link();
                    highlight(state.doc[state.item], state.item);
                }
                break;
            case '\r':
            case '\s':
                if (state.item_type == '1' && ['0', '1', '4', '5', '6', '9', 'g', 'I'].indexOf(state.doc[state.item].type) > -1) {
                    go_get(state.doc[state.item].host, state.doc[state.item].port, state.doc[state.item].selector, state.doc[state.item].type);
                }
                break;
            case 'g':
            case 'G':
                var addr = get_address();
                if (addr && addr.host) go_get(addr.host, addr.port, addr.selector, addr.type);
                break;
            case KEY_LEFT:
                go_back();
                break;
            case KEY_RIGHT:
                go_forward();
                break;
            case KEY_UP:
                if (frames.content.offset.y > 0) frames.content.scroll(0, -1);
                break;
            case KEY_DOWN:
                if (frames.content.offset.y + frames.content.height < frames.content.data_height) frames.content.scroll(0, 1);
                break;
            default:
                break;
        }
        if (frames.top.cycle()) {
            scrollbar.cycle();
            console.gotoxy(console.screen_columns, console.screen_rows);
        }
    } while ((state.input = console.getkey()).toLowerCase() != 'q');
    frames.top.close();
}

main();