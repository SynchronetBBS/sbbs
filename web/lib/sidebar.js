function getSidebarModules() {
	var sidebarModules = [];
	var d = directory(settings.web_root + 'sidebar/*');
	d.forEach(
		function(item) {
			if(file_isdir(item))
				return;
			var fn = file_getname(item);
			// Check webctrl.ini
			sidebarModules.push(fn);
		}
	);
	return sidebarModules;
}

function getSidebarModule(module) {

	var ret = '';
	if (!file_exists(module)) return ret;
	var ext = file_getext(module).toUpperCase();

	switch (ext) {
		case '.SSJS':
			if (ext === '.SSJS' && module.search(/\.xjs\.ssjs$/i) >= 0) break;
			load(module, true);
			break;
		case '.XJS':
			load(xjs_compile(module), true);
			break;
		case '.HTML':
			var f = new File(module);
			f.open('r');
			if (f.is_open) {
				ret = f.read();
				f.close();
			}
			break;
		case '.TXT':
			var f = new File(module);
			f.open();
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

function writeSidebarModules() {
	var modules = getSidebarModules();
	write('<ul class="list-group">');
	modules.forEach(
		function (module) {
			if (module.search(/\.xjs\.ssjs$/i) >= 0) return;
			write('<li class="list-group-item sidebar">');
			var str = getSidebarModule(settings.web_root + "sidebar/" + module);
			if (str !== '') write(str);
			write('</li>');
		}
	);
	write('</ul>');
}