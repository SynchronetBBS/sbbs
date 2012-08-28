var e = directory(webIni.RootDirectory + "/pages/*");
for(var g in e) {
	var ext = e[g].toUpperCase().split(".").slice(1).join(".");
	var h = new File(e[g]);
	h.open("r");
	var i = h.readAll();
	h.close();
	if(file_isdir(e[g]) || ext == "XJS.SSJS")
		continue;
	if(ext == "JS" || ext == "SSJS") {
		var title = i[0].replace(/\/\//g, "");
		if(title == "HIDDEN")
			continue;
		print("<a class='link' href=./index.xjs?page=" + file_getname(e[g]) + ">" + title + "</a><br />");
	}
	if(ext == "HTML" || ext == "XJS") {
		// Seek first comment line in an HTML document
		for(j = 0; j < i.length; j++) {
			var k = i[j].match(/^\<\!\-\-.*\-\-\>$/);
			if(k === null)
				continue;
			var title = k[0].replace(/[\<\!\-+|\-+\>]/g, "");
			if(title.match("HIDDEN") != null)
				break;
			print("<a class='link' href=./index.xjs?page=" + file_getname(e[g]) + ">" + title + "</a><br />");
		}
	}
	if(ext == "TXT") {
		if(i[0] == "HIDDEN")
			continue;
		print("<a class='link' href=./index.xjs?page=" + file_getname(e[g]) + ">" + i[0] + "</a><br />");
	}
}