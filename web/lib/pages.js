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
				var list = getPageList(e);
				if (Object.keys(list).length > 0) {
					pages[e.split(e.substr(-1, 1)).slice(-2, -1)] = list;
				}
			} else if (e.search(/(\.xjs\.ssjs|webctrl\.ini)$/i) < 0) {
				var fn = file_getname(e);
				var title = getPageTitle(e);
				if ((	typeof webctrl === 'undefined' ||
						webCtrlTest(webctrl, fn)
					) && title.search(/^HIDDEN/) < 0
				) {
					pages[title] = file_getname(e);
				}
			}
		}
	);

	return pages;

}

function getPageTitle(file) {

	if (backslash(
			fullpath(file)
		).indexOf(
			backslash(fullpath(settings.web_root + '/pages'))
		) !== 0
	) {
		return;
	}

	var title;
	var ext = file_getext(file).toUpperCase();
	var f = new File(file);
	if (ext === '.JS' ||
		(ext === '.SSJS' && file.search(/\.xjs\.ssjs$/i) === -1)
	) {
		f.open('r');
		var i = f.readln();
		f.close();
		title = i.replace(/\/\//g, '');
	} else if (ext === '.HTML' || ext === '.XJS') {
		// Seek first comment line in an HTML document
		f.open('r');
		while (!f.eof) {
			var i = f.readln();
			var k = i.match(/^\<\!\-\-.*\-\-\>$/);
			if (k !== null) {
				title = k[0].replace(/[\<\!\-+|\-+\>]/g, '');
				break;
			}
		}
		f.close();
	} else if (ext === '.TXT') {
		title = file_getname(file);
	}

	return title;

}

function getPage(page) {

	var ret = '';

	page = settings.web_root + 'pages/' + page;

	if (!file_exists(page)) return ret;

	var ext = file_getext(page).toUpperCase();

	if (user.alias != settings.guest) {
		var title = getPageTitle(page);
		if (title != 'HIDDEN') {
			setSessionValue(
				user.number,
				'action',
				title.replace(/^HIDDEN(\:)+/, '')
			);
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