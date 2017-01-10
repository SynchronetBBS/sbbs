function getLanguage(file, section) {

	var stock = 'english.ini';

	if (!file_exists(settings.web_lib + 'language/' + file)) {
		return getLanguage(stock, section);
	}

	var f = new File(settings.web_lib + 'language/' + file);
	if (!f.open('r')) throw 'Unable to open language file ' + file;
	var ini = f.iniGetObject(section);
	f.close();
	if (ini === null && file !== stock) ini = getLanguage(stock, section);

	return ini;

}