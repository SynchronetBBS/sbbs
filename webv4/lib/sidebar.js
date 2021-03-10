function getFileContents(file) {
	const f = new File(file);
	if (!f.open('r')) return '';
	const ret = f.read();
	f.close();
	return ret;
}

function moduleName(m) {
	return file_getname(m).replace(/^\d*-/, '');
}

function mergeModuleLists(stock, mods) {
	return mods.reduce(function (a, c) {
		const idx = a.findIndex(function (e) {
			return moduleName(e) == moduleName(c);
		});
		if (idx < 0) {
			a.push(c);
		} else {
			a[idx] = c;
		}
		return a;
	}, stock).sort(function (a, b) {
		const fna = file_getname(a);
		const fnb = file_getname(b);
		if (fna < fnb) return -1;
		if (fna > fnb) return 1;
		return 0;
	});
}

function _getSidebarModules(dir) {
	if (!file_isdir(dir)) return [];
	return directory(dir + '*').filter(function (e) {
		return (!file_isdir(e));
	});
}

function getSidebarModules() {
	const stock = _getSidebarModules(settings.web_sidebar);
	const mods = _getSidebarModules(settings.web_mods_sidebar);
	return mergeModuleLists(stock, mods);
}

function getSidebarModule(module) {

	var ret = '';
	if (!file_exists(module)) return ret;
	const ext = file_getext(module).toUpperCase();

	switch (ext) {
		case '.SSJS':
			if (ext === '.SSJS' && module.search(/\.xjs\.ssjs$/i) >= 0) break;
			js.exec(module, new function () {});
			break;
		case '.XJS':
			js.exec(xjs_compile(module), new function () {});
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

var sidebar = {
	write: function () {
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
};

sidebar;