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
	writeln("<table border=0>");
	for(fn in files) {
		var thisfile=files[fn].replace(/^.*?\/([^\/]+\/?)$/,"$1");
		if(thisfile=='access.ars')
			continue;
		writeln('<tr><td><a href="'+http_request.virtual_path+thisfile+'">'+thisfile+"</a></td>");
		if(file_isdir(files[fn])) {
			writeln('<td align=right colspan=2>&lt;directory&gt;</td>');
		}
		else {
                        var size = file_size(files[fn])/1024;
			if(size<1) size=1;
			writeln('<td align=right>&nbsp;'+ Math.floor(size)+'k&nbsp;</td>');
			writeln('<td align=right nowrap>&nbsp;' + strftime("%b-%d-%y %H:%M",file_date(files[fn])) + '</td>');
		}
		writeln("</tr>");
	}
	writeln("</table></body></html>");
}
