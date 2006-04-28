var picspath=web_root_dir+'/gallery_ex/pics';

writeln('<html><head><title>'+http_request.query['gallery']+' gallery</title></head>');
writeln('<body>');
var pics=directory(picspath+'/'+http_request.query['gallery']+'/*.jpg');
for(index in pics) {
	var filename=pics[index];
	filename=filename.replace(/.*\//,'');
	writeln('<a target="image" href="pics/'+http_request.query['gallery']+'/'+filename+'"><img border="0" src="thumbs/'+http_request.query['gallery']+'/'+filename+'"></a> ');
}
writeln('</body></html>');
