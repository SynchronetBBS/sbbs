if(!file_isdir(http_request.real_path)) {
	var ep404=new File(web_error_dir+"404.html");
	ep404.open("rb",true);
	write(ep404.readAll().join("\n"));
}
else {
	http_reply.status="200 OK";
	var files=directory(http_request.real_path+'/*');
	writeln("<html><head><title>Files in: "+http_request.virtual_path+"</title></head><body>");
	write("Files in: "+http_request.virtual_path+"<br>");
	writeln("<table>");
	for(fn in files) {
		var thisfile=files[fn].replace(/^.*?\/([^\/]+\/?)$/,"$1");
		if(thisfile=='access.ars')
			continue;
		write('<tr><td><a href="'+http_request.virtual_path+thisfile+'">'+thisfile+"</a></td>");
		if(file_isdir(files[fn])) {
			write('<td>Directory</td>');
		}
		else {
			write('<td>'+file_size(files[fn])+' Bytes</td>');
		}
		write("</tr>");
	}
	writeln("</table></body></html>");
}
