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
			var size = getsizestr(file_size(files[fn]),true);
			writeln('<td align=right>&nbsp;'+size+'&nbsp;</td>');
			writeln('<td align=right nowrap>&nbsp;' + strftime("%b-%d-%y %H:%M",file_date(files[fn])) + '</td>');
		}
		writeln("</tr>");
	}
	writeln("</table></body></html>");
}

function getsizestr(size, bytes)
{
	var outstr='';

	if(bytes) {
		if(size < 1000) {       /* Bytes */
			outstr=format("%ld bytes",size);
			return(outstr);
		}
		if(size<10000) {        /* Bytes with comma */
			outstr=format("%ld,%03ld bytes",(size/1000),(size%1000));
			return(outstr);
		}
		size = size/1024;
	}
	if(size<1000) { /* KB */
		outstr=format("%ld KB",size);
		return(outstr);
	}
	if(size<999999) { /* KB With comma */
		outstr=format("%ld,%03ld KB",(size/1000),(size%1000));
		return(outstr);
	}
	size = size/1024;
	if(size<1000) { /* MB */
		outstr=format("%ld MB",size);
		return(outstr);
	}
	if(size<999999) { /* MB With comma */
		outstr=format("%ld,%03ld MB",(size/1000),(size%1000));
		return(outstr);
	}
	size = size/1024;
	if(size<1000) { /* GB */
		outstr=format("%ld GB",size);
		return(outstr);
	}
	if(size<999999) { /* GB With comma */
		outstr=format("%ld,%03ld GB",(size/1000),(size%1000));
		return(outstr);
	}
	size = size/1024;
	if(size<1000) { /* TB (Yeah, right) */
		outstr=format("%ld TB",size);
		return(outstr);
	}
	sprintf(outstr,"Plenty");
	return(outstr);
}
