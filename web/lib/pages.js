var getWebCtrl = function() {
	if(!file_exists(settings.web_root + "pages/webctrl.ini"))
		return false;
	var f = new File(settings.web_root + "pages/webctrl.ini");
	if(!f.open("r")) {
		log("Unable to open pages/webctrl.ini");
		exit();
	}
	var ini = f.iniGetAllObjects();
	f.close();
	return ini;
}

var webCtrlTest = function(ini, filename) {
	var ret = true;
	for(var i = 0; i < ini.length; i++) {
		if(!wildmatch(false, filename, ini[i].name))
			continue;
		if(	typeof ini[i].AccessRequirements == "undefined"
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

var webCtrlFilter = function(pages) {
	var ini = getWebCtrl();
	if(typeof ini == "boolean" && !ini)
		return pages;
	pages = pages.filter(
		function(page) {
			return webCtrlTest(ini, page.page);
		}
	);
	return pages;
}

var getPages = function(primary) {

	if(typeof primary == "undefined")
		var wc = "*";
	else if(!primary)
		var wc = "*_*";
	else
		var wc = "*-*";

	var pages = [];
	var d = directory(settings.web_root + "pages/" + wc);
	d.forEach(
		function(item) {
			if(file_isdir(item))
				return;
			var fn = file_getname(item);
			var title = getPageTitle(item);
			if(typeof title == "undefined" || title.search(/^HIDDEN/) == 0)
				return;
			pages.push(
				{	'page' : fn,
					'title' : title
				}
			);
		}
	);
	return webCtrlFilter(pages);

}

var getPageTitle = function(file) {

	var ext = file_getext(file).toUpperCase();

	var f = new File(file);
	f.open('r');
	var i = f.readAll();
	f.close();

	if(ext == ".JS" || (ext == ".SSJS" && file.search(/\.xjs\.ssjs$/i)==-1)) {
		var title = i[0].replace(/\/\//g, "");
		return title;
	}

	if(ext == ".HTML" || ext == ".XJS") {
		// Seek first comment line in an HTML document
		for(var j = 0; j < i.length; j++) {
			var k = i[j].match(/^\<\!\-\-.*\-\-\>$/);
			if(k === null)
				continue;
			var title = k[0].replace(/[\<\!\-+|\-+\>]/g, "");
			return title;
		}
	}

	if(ext == ".TXT")
		return file_getname(file);

}

var getPage = function(page) {

	var ret = "";

	page = settings.web_root + "pages/" + page;

	if(!file_exists(page))
		return ret;

	var ext = file_getext(page).toUpperCase();

	if(user.alias != settings.guest) {
		var title = getPageTitle(page);
		if(title != "HIDDEN")
			setSessionValue(user.number, 'action', title);
	}

	switch(ext) {
		case ".SSJS":
			if(ext == ".SSJS" && page.search(/\.xjs\.ssjs$/i) >= 0)
				break;
			load(page, true);
			break;
		case ".XJS":
			load(xjs_compile(page), true);
			break;
		case ".HTML":
			var f = new File(page);
			f.open("r");
			if(f.is_open) {
				ret = f.read();
				f.close();
			}
			break;
		case ".TXT":
			var f = new File(page);
			f.open("r");
			if(f.is_open) {
				ret = "<pre>" + f.read() + "</pre>";
				f.close();
			}
			break;
		default:
			break;
	}

	return ret;

}