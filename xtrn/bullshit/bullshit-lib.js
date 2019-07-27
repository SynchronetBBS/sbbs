load('sbbsdefs.js');
load('funclib.js');

function loadSettings(path) {

	var f = new File((path || js.exec_dir) + 'bullshit.ini');
	if (!f.open('r')) throw 'Failed to open bullshit.ini.';
	var settings = f.iniGetObject();
	settings.colors = f.iniGetObject('colors');
	settings.files = f.iniGetObject('files');
	f.close();

	Object.keys(settings.colors).forEach(function (k) {
        settings.colors[k] = settings.colors[k].toUpperCase().split(',');
        if (typeof getColor == 'undefined') return;
		if (settings.colors[k].length > 1) {
			settings.colors[k].forEach(function (e, i, a) {
                a[i] = getColor(e);
            });
		} else {
			settings.colors[k] = getColor(settings.colors[k][0]);
		}
	});

	return settings;

}

function readUserHistory() {
    const dummy = { files: {}, messages: [] };
    const fn = format('%suser/%04d.bullshit', system.data_dir, user.number);
    if (!file_exists(fn)) return dummy;
    const f = new File(fn);
    if (f.open('r')) {
        const obj = JSON.parse(f.read());
        f.close();
        return obj;
    }
    return dummy;
}

function writeUserHistory(obj) {
    const fn = format('%suser/%04d.bullshit', system.data_dir, user.number);
    log('Writing ' + fn);
    const f = new File(fn);
    if (f.open('w')) {
        f.write(JSON.stringify(obj));
        f.close();
    }
}

function setBulletinRead(key) {
    const history = readUserHistory();
    if (typeof key == 'string') {
        history.files[key] = time();
    } else if (typeof key == 'number') {
        if (history.messages.indexOf(key) < 0) history.messages.push(key);
    }
    writeUserHistory(history);
}

function loadList(settings) {

    const history = readUserHistory();

    const list = [];
	Object.keys(settings.files).forEach(function (key) {
		if (!file_exists(settings.files[key])) return;
        if (settings.newOnly && typeof history.files[key] == 'number' && file_date(settings.files[key]) <= history.files[key]) return;
        if (js.global.console) {
            list.push({
                str: format(
                    '%-' + (console.screen_columns - 29) + 's%s',
                    key, system.timestr(file_date(settings.files[key]))
                ),
                key: key
            });
        } else {
            list.push({
                title: key,
                type: 'file',
                date: system.timestr(file_date(settings.files[key])),
                path: settings.files[key]
            });
        }
	});

	const msgBase = new MsgBase(settings.messageBase);
	msgBase.open();
	var shown = 0;
	for (var m = msgBase.last_msg; m >= msgBase.first_msg; m = m - 1) {
		try {
			var h = msgBase.get_msg_header(m);
		} catch (err) {
			continue;
		}
		if (h === null) continue;
        if (settings.newOnly && history.messages.indexOf(m) >= 0) continue;
        if (h.attr&MSG_DELETE) continue;
        if (js.global.console) {
            list.push({
                str: format(
                    '%-' + (console.screen_columns - 29) + 's%s',
                    h.subject.substr(0, console.screen_columns - 30),
                    system.timestr(h.when_written_time)
                ),
                key: m
            });
        } else {
            list.push({
                title: h.subject,
                type: 'message',
                date: system.timestr(h.when_written_time),
                num: h.number
            });
        }
		shown++;
		if (settings.maxMessages > 0 && shown >= settings.maxMessages) break;
	}
	msgBase.close();

    return list;
}

this;