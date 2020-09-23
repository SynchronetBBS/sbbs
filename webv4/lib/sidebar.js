function getFileContents(file) {
	const f = new File(file);
	if (!f.open('r')) return '';
	const ret = f.read();
	f.close();
	return ret;
}

function getSidebarModules() {
	return directory(settings.web_sidebar + '*').filter(function (e) {
		return (!file_isdir(e));
	});
}

function getSidebarModule(module) {

	var ret = '';
	if (!file_exists(module)) return ret;
	const ext = file_getext(module).toUpperCase();

	switch (ext) {
		case '.SSJS':
			if (ext === '.SSJS' && module.search(/\.xjs\.ssjs$/i) >= 0) break;
			(function () {
				load(module, true);
			})();
			break;
		case '.XJS':
			(function () {
				load(xjs_compile(module), true);
			})();
			break;
		case '.HTML':
			ret = getFileContents(module);
			break;
		case '.TXT':
			const tfc = getFileContents(module);
			if (tfc.length) ret = '<pre>' + tfc + '</pre>';
			break;
		default:
			break;
	}

	return ret;

}

function writeSidebarModules() {
	const modules = getSidebarModules();
	write('<ul class="list-group">');
	modules.forEach(function (module) {
		if (module.search(/\.xjs\.ssjs$/i) >= 0) return;
		write('<li class="list-group-item sidebar">');
		const str = getSidebarModule(module);
		if (str.length) write(str);
		write('</li>');
	});
	write('</ul>');
}
