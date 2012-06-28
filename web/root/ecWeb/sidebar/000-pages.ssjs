/* pages.ssjs, from ecWeb v2 for Synchronet BBS 3.15+
   by Derek Mullin (echicken -at- bbs.electronicchicken.com) */

/* A sidebar widget that generates a list of links to static pages based on the
   contents of the ~pages/ directory. */

var e = directory(webIni.webRoot + "/pages/*");
for(var g in e) {
	var h = new File(e[g]);
	if(h.open("r",true)) {
		var i = h.readAll();
		h.close();
		if(file_isdir(e[g])) continue;
		else if(file_getext(e[g]).toUpperCase() == ".JS" || file_getext(e[g]).toUpperCase() == ".SSJS") print("<a class='link' href=./pages.ssjs?page=" + file_getname(e[g]) + ">" + i[0].replace(/\/\//g, "") + "</a>");
		else if(file_getext(e[g]).toUpperCase() == ".HTML") print("<a class='link' href=./pages.ssjs?page=" + file_getname(e[g]) + ">" + i[0].replace(/[\<\!\-+|\-+\>]/g, "") + "</a>");
		else if(file_getext(e[g]).toUpperCase() == ".TXT") print("<a class='link' href=./pages.ssjs?page=" + file_getname(e[g]) + ">" + i[0] + "</a>");
		else continue;
		print("<br />");
	}
}

