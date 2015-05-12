load("file_size.js");

// Load the icons definitions...
icons=new File(file_cfgname(system.ctrl_dir,"webicons.ini"));
if(icons.exists) {
	icons.open("r",true);
	allicons=icons.iniGetObject();
	for(ic in allicons) {
		if(ic!=ic.toLowerCase()) {
			allicons[ic.toLowerCase()]=allicons[ic];
		}
	}
}
else {
	log(system.ctrl_dir+"webicons.ini does not exist!");
}

if(!file_isdir(http_request.real_path)) {
	var ep404=new File(web_error_dir+"404.html");
	ep404.open("rb",true);
	write(ep404.readAll().join("\n"));
}
else {
	http_reply.status="200 OK";
	var files=directory(http_request.real_path+'/*');
	writeln("<html><head><title>Files in: "+http_request.virtual_path+"</title></head><body>");
	writeln("<h2>Files in: "+http_request.virtual_path+"</h2>");
	if(http_request.virtual_path != '/') {
		files.unshift('..');
	}
	writeln("<table border=0>");
	for(fn in files) {
		var thisfile=files[fn].replace(/^.*?\/([^\/]+\/?)$/,"$1");
		if(thisfile=='access.ars' || thisfile=='webctrl.ini')
			continue;
		if(allicons==undefined) {
			writeln('<tr><td><a href="'+http_request.virtual_path+thisfile+'">'+thisfile+"</a></td>");
		}
		else {
			thisfilename=thisfile;
			ext=thisfile.match(/\.([^\.]+)$/);
			var extension='DefaultIcon';
			if(ext!=undefined && ext[1]!=undefined) {
				extension=ext[1];
			}
			if(file_isdir(files[fn])) {
				extension='DIRECTORY';
			}
			if(files[fn]=='..') {
				extension='..';
				thisfilename="Parent directory";
			}
			extension=extension.toLowerCase();
			if(allicons[extension]!=undefined)
				icon='<img src="'+allicons[extension]+'">';
			else
				icon='<img src="'+allicons["DefaultIcon"]+'">';
			writeln('<tr><td>'+icon+'</td><td><a href="'+http_request.virtual_path+thisfile+'">'+thisfilename+"</a></td>");
		}
		if(file_isdir(files[fn])) {
			writeln('<td align=right colspan=2>&lt;directory&gt;</td>');
		}
		else {
			var size = file_size_str(file_size(files[fn]),true);
			writeln('<td align=right>&nbsp;'+size+'&nbsp;</td>');
			writeln('<td align=right nowrap>&nbsp;' + strftime("%b-%d-%y %H:%M",file_date(files[fn])) + '</td>');
		}
		writeln("</tr>");
	}
	writeln("</table></body></html>");
}
