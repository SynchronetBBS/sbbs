var e = directory(webIni.RootDirectory + "/pages/*");
for(var g in e) {
	var fn = file_getname(e[g]);
	if(!checkWebCtrl(webIni.RootDirectory + "/pages/", fn))
		continue;
	var ext = file_getext(e[g]).toUpperCase();
	var h = new File(e[g]);
	h.open("r");
	var i = h.readAll();
	h.close();
	if(ext == ".JS" || (ext == ".SSJS" && e[g].search(/\.xjs\.ssjs$/i)==-1)) {
		var title = i[0].replace(/\/\//g, "");
		if(title == "HIDDEN")
			continue;
		print("<a class='link' href=./index.xjs?page=" + file_getname(e[g]) + ">" + title + "</a><br />");
	}
	if(ext == ".HTML" || ext == ".XJS") {
		// Seek first comment line in an HTML document
		for(j = 0; j < i.length; j++) {
			var k = i[j].match(/^\<\!\-\-.*\-\-\>$/);
			if(k === null)
				continue;
			var title = k[0].replace(/[\<\!\-+|\-+\>]/g, "");
			if(title.match("HIDDEN") != null)
				break;
			print("<a class='link' href=./index.xjs?page=" + file_getname(e[g]) + ">" + title + "</a><br />");
			break;
		}
	}
	if(ext == ".TXT") {
		if(i[0] == "HIDDEN")
			continue;
		print("<a class='link' href=./index.xjs?page=" + file_getname(e[g]) + ">" + i[0] + "</a><br />");
	}
}
