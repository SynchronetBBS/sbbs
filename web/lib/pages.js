function getWebCtrl() {
	if (!file_exists(settings.web_root + 'pages/webctrl.ini')) return false;
	var f = new File(settings.web_root + 'pages/webctrl.ini');
	if (!f.open('r')) {
		log('Unable to open pages/webctrl.ini');
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
		if (typeof ini[i].AccessRequirements === 'undefined'
			||
			user.compare_ars(ini[i].AccessRequirements)
		) {
			continue;
		}
		ret = false;
		break;
	}
	return ret;
}

function webCtrlFilter(pages) {
	var ini = getWebCtrl();
	if (typeof ini === 'boolean' && !ini) return pages;
	pages = pages.filter(
		function (page) {
			return webCtrlTest(ini, page.page);
		}
	);
	return pages;
}

function getPages() {

	var pages = [];
	var d = directory(settings.web_root + 'pages/*');
	d.forEach(
		function (item) {
			if (file_isdir(item)) return;
			var fn = file_getname(item);
			var title = getPageTitle(item);
			if (typeof title === 'undefined' ||
				title.search(/^HIDDEN/) === 0 ||
				fn.search(/\.xjs\.ssjs$/i) >= 0
			) {
				return;
			}
			pages.push(
				{	'page' : fn,
					'title' : title,
					'primary' : (fn.search(/^\d+_/) === 0 ? false : true)
				}
			);
		}
	);
	return webCtrlFilter(pages);

}

function getPageTitle(file) {

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
			load(page, true);
			break;
		case '.XJS':
			load(xjs_compile(page), true);
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