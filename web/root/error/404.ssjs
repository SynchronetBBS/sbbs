if(!file_isdir(http_request.real_path)) {
	var ep404=new File(web_error_dir+"404.html");
	ep404.open("rb",true);
	write(ep404.readAll().join("\n"));
}
else {
	http_reply.status="200 OK";
	write("Files in: "+http_request.virtual_path+"<br>");
	var files=directory(http_request.real_path+'/*');
	for(fn in files) {
		files[fn]=files[fn].replace(/^.*\//,'');
		write('<a href="'+http_request.virtual_path+files[fn]+'">'+files[fn]+"</a><br>");
	}
}
