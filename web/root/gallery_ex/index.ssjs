var picspath=web_root_dir+'/gallery_ex/pics';
var gallery=http_request.query['gallery'];

if(gallery==undefined) {
	writeln("<html><head><title>Synchronet Galleries"+picspath+"</title></head>");
}
else {
	writeln('<html><head><title>'+gallery+'</title></head>');
}
writeln('<frameset cols="*,200" style="margin: 0;" marginheight="0" marginwidth="0" border="0" framespacing="0">');
writeln('  <frameset rows="*,110" style="margin: 0;" marginheight="0" marginwidth="0" border="0" framespacing="0">');
writeln('  <frame name="image" src="blank.html" marginheight="0" marginwidth="0" border="0" framespacing="0">');
if(gallery==undefined) {
	writeln('  <frame name="bottom" src="blank.html" marginheight="0" marginwidth="0" border="0" framespacing="0">');
}
else {
	writeln('  <frame name="bottom" src="gallery_bottom.ssjs?gallery='+gallery+'" marginheight="0" marginwidth="0" border="0" framespacing="0">');
}
writeln(' </frameset>');
writeln(' <frame name="gallery_index" src="gallery_index.ssjs" marginheight="0" marginwidth="0" border="0" framespacing="0">');
writeln('</frameset>');
writeln('<body>Frames required</body>');
writeln('</html>');
