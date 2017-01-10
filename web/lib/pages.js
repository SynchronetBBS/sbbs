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

	if (fullpath(file).indexOf(fullpath(settings.web_root + '/pages')) !== 0) {
		return;
	}

	var f = new File(file);
	var ctrl = '';
	var ext = file_getext(file).toUpperCase();
	switch (ext) {
		case '.JS':
		case '.SSJS':
			if (!f.open('r')) return;
			ctrl = f.readln().replace(/\/\//g, '');
			f.close();
			break;
		case '.XJS':
		case '.HTML':
			if (!f.open('r')) return;
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

	return {
		options : {
			hidden : (opts.indexOf('HIDDEN') >= 0),
			no_sidebar : (opts.indexOf('NO_SIDEBAR') >= 0)
		},
		title : title
	};

}

function getPageList(dir) {

	dir = backslash(fullpath(dir));
	if (dir.indexOf(backslash(fullpath(settings.web_root + '/pages'))) !== 0) {
		return {};
	}

	var webctrl = getWebCtrl(dir);

	var pages = {};
	directory(dir + '*').forEach(
		function (e) {
			if (file_isdir(e)) {
				e = fullpath(e);
				var list = getPageList(e);
				if (Object.keys(list).length > 0) {
					pages[e.split(e.substr(-1, 1)).slice(-2, -1)] = list;
				}
			} else if (e.search(/(\.xjs\.ssjs|webctrl\.ini)$/i) < 0) {
				var fn = file_getname(e);
				var ctrl = getCtrlLine(e);
				if ((	typeof webctrl === 'undefined' ||
						webCtrlTest(webctrl, fn)
					) && !ctrl.options.hidden
				) {
					pages[ctrl.title] = file_getname(e);
				}
			}
		}
	);

	return pages;

}

function getPage(page) {

	var ret = '';

	page = settings.web_root + 'pages/' + page;

	if (!file_exists(page)) return ret;

	var ext = file_getext(page).toUpperCase();

	if (user.alias != settings.guest) {
		var ctrl = getCtrlLine(page);
		if (typeof ctrl !== 'undefined' && !ctrl.options.hidden) {
			setSessionValue(user.number, 'action', ctrl.title);
		}
	}

	switch(ext) {
		case '.SSJS':
			if (ext === '.SSJS' && page.search(/\.xjs\.ssjs$/i) >= 0) break;
			load({}, page, true);
			break;
		case '.XJS':
			load({}, xjs_compile(page), true);
			break;
		case '.HTML':
			var f = new File(page);
			f.open('r');
			if (f.is_open) {
				ret = f.read();
				f.close();
			}
			break;
		case '.TXT':
			var f = new File(page);
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