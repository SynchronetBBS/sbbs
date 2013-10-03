load(webIni.WebDirectory + '/lib/ecutils.js');
var e = directory(webIni.RootDirectory + "/pages/*");
for(var g in e) {
	if(file_isdir(e[g]))
		continue;
	var fn = file_getname(e[g]);
	if(!checkWebCtrl(webIni.RootDirectory + "/pages/", fn))
		continue;
	var ext = file_getext(e[g]).toUpperCase();
	var h = new File(e[g]);
	h.open("r");
	var i = h.readAll();
	h.close();
	var title = getTitle(i, e[g]);
	if(title===undefined || title.search(/^HIDDEN/)==0)
		continue;
	print("<a class='link' href=./index.xjs?page=" + file_getname(e[g]) + ">" + title + "</a><br />");
}
