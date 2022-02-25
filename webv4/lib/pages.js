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

	const _file = fullpath(file);

	if (_file === undefined) return ret;

	if (_file.indexOf(settings.web_pages) != 0 && _file.indexOf(settings.web_mods_pages) != 0) return ret;

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
			ctrl = pageName(file_getname(file));
			break;
	}
	ctrl = truncsp(skipsp(ctrl));

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

function getExternalLink(fn) {
	const p = getPagePath(fn);
	if (p === null) throw new Error('Invalid page ' + fn + ',,,' + settings.web_mods_pages);
	const f = new File(p);
	f.open('r');
	const c = f.read().split(',');
	f.close();
	if (c.length < 2) throw new Error('Invalid page ' + fn + ',,,' + settings.web_mods_pages);
	return c[0];
}

function _getPageList(dir) {

	if (!file_isdir(dir)) return [];

	const webctrl = getWebCtrl(dir);
	const sep = system.platform.search(/^win/i) == 0 ? '\\' : '/';

    const pages = directory(dir + '*').reduce(function (a, c) {

		if (file_isdir(c)) {
            const list = _getPageList(c);
            if (Object.keys(list).length) {
				const t = c.replace(/\\*\/*$/, '').split(sep).slice(-1)[0];
            	a.push({
					file: t,
					name: pageName(t),
					type: 'list',
					title: t,
					list: list
				});
            }
            return a;
        }

		const ext = file_getext(c).toUpperCase();
        if (c.search(/(\.xjs\.ssjs|webctrl\.ini)$/i) >= 0) return a;
        if (['.HTML', '.SSJS', '.XJS', '.TXT', '.LINK'].indexOf(ext) < 0) return a;

		const fn = file_getname(c);
		if (webctrl && !webCtrlTest(webctrl, fn)) return a;
		if (ext == '.LINK') {
			const f = new File(c);
			if (!f.open('r')) return a;
			const l = f.readln().split(',');
			f.close();
			if (l.length < 2) return a;
			a.push({
				file: fn,
				name: pageName(c),
				title: l[1],
				page: fn,
				type: 'link'
			});
		} else {
			const ctrl = getCtrlLine(c);
			if (!ctrl.options.hidden) {
				a.push({
					file: fn,
					name: pageName(fn),
					page: fn,
					title: ctrl.title,
					type: 'page',
				});
			}
		}
        return a;
    }, []);

	return pages;

}

function pageName(p) {
	return p.replace(/^\d*-/, '').replace(/\..*$/, '');
}

function mergePageLists(stock, mods) {
	return mods.reduce(function (a, c) {
		const idx = a.findIndex(function (e) {
			return e.name == c.name;
		});
		if (idx < 0) {
			a.push(c);
		} else if (a[idx].type != c.type) {
			a[idx] = c;
		} else if (a[idx].type == 'page' || a[idx].type == 'link') {
			a[idx] = c;
		} else if (a[idx].type == 'list') {
			a[idx].list = mergePageLists(a[idx].list, c.list);
		}
		return a;
	}, stock).sort(function (a, b) {
		if (a.file < b.file) return -1;
		if (a.file > b.file) return 1;
		return 0;
	});
}

function getPageList() {
	var stock = _getPageList(settings.web_pages);
	var mods = _getPageList(settings.web_mods_pages);
	return mergePageLists(stock, mods);
}

function getPagePath(page) {
	var ret = null;
	if (file_exists(settings.web_mods_pages + page)) {
		ret = fullpath(settings.web_mods_pages + page);
		if (ret.indexOf(settings.web_mods_pages) != 0) ret = null;
	} else if (file_exists(settings.web_pages + page)) {
		ret = fullpath(settings.web_pages + page);
		if (ret.indexOf(settings.web_pages) != 0) ret = null;
	}
	return ret;
}

function getPage(page) {

	var ret = '';

	var p = getPagePath(page);
	if (p === null) return ret;

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
			js.exec(p, new function () {});
			break;
		case '.XJS':
			js.exec(xjs_compile(p), new function () {});
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
	const pp = getPagePath(page);
	var ini = getWebCtrl(pp.replace(file_getname(page), ''));
	if ((typeof ini === "boolean" && !ini) || webCtrlTest(ini, page)) {
		write(getPage(page));
	}
}

function doRedirect(page) {
	if (page.search(/^001-forum.ssjs/) === 0) {
		http_reply.status = '302 Found';
		http_reply.header['Location'] = './?page=001-forum.xjs' + http_request.query_string.replace(/^page=001-forum.ssjs/, '');
		return true;
	}
	if (page.search(/\.link$/) > -1) {
		var loc = pages.getExternalLink(page);
        http_reply.status = '301 Moved Permanently';
        http_reply.header['Location'] = loc;
        return true;
    }
	return false;
}

var pages = {
	doRedirect: doRedirect,
	getCtrlLine: getCtrlLine,
	getExternalLink: getExternalLink,
	getPath: getPagePath,
	getList: getPageList,
	write: writePage,
};

pages;