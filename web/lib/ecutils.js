function getTitle(i, filename)
{
	var title;
	var j;
	var k;
	var ext=file_getext(filename).toUpperCase();
	if(i==null)
		return;

	if(ext == ".JS" || (ext == ".SSJS" && filename.search(/\.xjs\.ssjs$/i)==-1)) {
		var title = i[0].replace(/\/\//g, "");
		return title;
	}
	if(ext == ".HTML" || ext == ".XJS") {
		// Seek first comment line in an HTML document
		for(j = 0; j < i.length; j++) {
			var k = i[j].match(/^\<\!\-\-.*\-\-\>$/);
			if(k === null)
				continue;
			var title = k[0].replace(/[\<\!\-+|\-+\>]/g, "");
			return title
		}
	}
	if(ext == ".TXT") {
		return file_getname(filename);
	}
}
