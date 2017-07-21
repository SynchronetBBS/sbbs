function getFileContents(file) {
	var ret = '';
	var f = new File(file);
	if (f.open('r')) {
		ret = f.read();
		f.close();
	}
	return ret;
}

function getSidebarModules() {
	return directory(settings.web_root + 'sidebar/*').filter(
		function (e, i, a) { return (!file_isdir(e)); }
	);
}

function getSidebarModule(module) {

	var ret = '';
	if (!file_exists(module)) return ret;
	var ext = file_getext(module).toUpperCase();

	switch (ext) {
		case '.SSJS':
			if (ext === '.SSJS' && module.search(/\.xjs\.ssjs$/i) >= 0) break;
			load({}, module, true);
			break;
		case '.XJS':
			load({}, xjs_compile(module), true);
			break;
		case '.HTML':
			ret = getFileContents(module);
			break;
		case '.TXT':
			ret = '<pre>' + getFileContents(module) + '</pre>';
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
			var str = getSidebarModule(module);
			if (str !== '') write(str);
			write('</li>');
		}
	);
	write('</ul>');
}
