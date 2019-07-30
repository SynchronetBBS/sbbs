require('xjs.js', 'xjs_compile');

function getWebCtrl(dir) {
	if (!file_exists(dir + 'webctrl.ini')) return false;
	var f = new File(dir + 'webctrl.ini');
	if (!f.open('r')) {
		log('Unable to open ' + dir + 'webctrl.ini');
		exit();
	}
	var ini = f.iniGetAllObjects();
	f.close();
	return ini;
}

function webCtrlTest(ini, filename) {
	var ret = true;
	for (var i = 0; i < ini.length; i++) {
		if (!wildmatch(false, filename, ini[i].name)) continue;
		if (typeof ini[i].AccessRequirements === 'undefined' ||
			user.compare_ars(ini[i].AccessRequirements)
		) {
			continue;
		}
		ret = false;
		break;
	}
	return ret;
}

function getCtrlLine(file) {

    const ret = {
        options: {
            hidden: false,
            no_sidebar: false
        },
        title: file_getname(file)
    };

	if (fullpath(file).indexOf(fullpath(settings.web_pages)) != 0) return ret;

    var ctrl = '';
	const f = new File(file);
	const ext = file_getext(file).toUpperCase();
	switch (ext) {
		case '.JS':
		case '.SSJS':
			if (!f.open('r')) return ret;
            while (!f.eof) {
                var i = f.readln().match(/\/\/(.*?)$/);
                if (i !== null && i.length > 1) {
                    ctrl = i[1];
                    break;
                }
            }
			f.close();
			break;
		case '.XJS':
		case '.HTML':
			if (!f.open('r')) return ret;
			while (!f.eof) {
				var i = f.readln().match(/\<\!\-\-(.*?)\-\-\>/);
				if (i !== null && i.length > 1) {
					ctrl = i[1];
					break;
				}
			}
			f.close();
			break;
		default:
			ctrl = file_getname(file);
			break;
	}

	var opts = ctrl.match(/^(.*?)\:/);
	if (opts === null || opts.length < 2) {
		opts = [];
	} else {
		opts = opts[1].toUpperCase().split('|');
	}

	var title = ctrl.replace(/^.*\:/, '');
    if (opts.indexOf('HIDDEN') > -1) ret.options.hidden = true;
    if (opts.indexOf('NO_SIDEBAR') > -1) ret.options.no_sidebar = true;
    if (title != '') ret.title = title;

    return ret;

}

function getPageList(dir) {

	dir = backslash(fullpath(dir));
	if (dir.indexOf(settings.web_pages) !== 0) return {};

	const webctrl = getWebCtrl(dir);

	const sep = system.platform.search(/^win/i) == 0 ? '\\' : '/';

    const pages = directory(dir + '*').reduce(function (a, c) {
        if (file_isdir(c)) {
            const list = getPageList(c);
            if (Object.keys(list).length) {
            	a[c.replace(/\\*\/*$/, '').split(sep).slice(-1)] = { type: 'list', list: list };
            }
            return a;
        }
        const ext = file_getext(c).toUpperCase();
        if (c.search(/(\.xjs\.ssjs|webctrl\.ini)$/i) < 0
            && ['.HTML', '.SSJS', '.XJS', '.TXT', '.LINK'].indexOf(ext) > -1
        ) {
            const fn = file_getname(c);
            if (webctrl && !webCtrlTest(webctrl, fn)) return a;
            if (ext == '.LINK') {
                const f = new File(c);
                if (!f.open('r')) return a;
                const l = f.readln().split(',');
                f.close();
                if (l.length < 2) return a;
                a[l[1]] = { page: l[0], type: 'link' };
            } else {
                const ctrl = getCtrlLine(c);
                if (!ctrl.options.hidden) {
                    a[ctrl.title] = { page: file_getname(c), type: 'page' };
                }
            }
        }
        return a;
    }, {});

	return pages;

}

function getPage(page) {

	var ret = '';

	var p = settings.web_pages + page;

	if (!file_exists(p)) return ret;

	var ext = file_getext(p).toUpperCase();

	if (user.alias != settings.guest) {
		var ctrl = getCtrlLine(p);
		if (typeof ctrl !== 'undefined' && !ctrl.options.hidden) {
			setSessionValue(user.number, 'action', ctrl.title);
		}
	}

	switch(ext) {
		case '.SSJS':
			if (ext === '.SSJS' && p.search(/\.xjs\.ssjs$/i) >= 0) break;
			(function () {
				load(p, true);
			})();
			break;
		case '.XJS':
			(function () {
				load(xjs_compile(p), true);
			})();
			break;
		case '.HTML':
			var f = new File(p);
			f.open('r');
			if (f.is_open) {
				ret = f.read();
				f.close();
			}
			break;
		case '.TXT':
			var f = new File(p);
			f.open('r');
			if (f.is_open) {
				ret = '<pre>' + f.read() + '</pre>';
				f.close();
			}
			break;
		default:
			break;
	}

	return ret;

}

function writePage(page) {
	var ini = getWebCtrl(settings.web_pages +  '/' + page.replace(file_getname(page), ''));
	if ((typeof ini === "boolean" && !ini) || webCtrlTest(ini, page)) {
		write(getPage(page));
	}
}
