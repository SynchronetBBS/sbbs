var picspath=web_root_dir+'/gallery_ex/pics';

writeln("<html><head><title>Image Gallery List</title></head>");
writeln("<body>");
var galleries=new Array();
galleries=directory(picspath+'/*');
for(gallery in galleries) {
	var name=galleries[gallery];
	name=name.replace(/.*\/(.*)\//,'$1');
	var pics=new Array();
	pics=directory(picspath+'/'+name+'/*.jpg');
	writeln('<p><a href="gallery_bottom.ssjs?gallery='+name+'" target="bottom">'+name+'</a><br>'+strftime("%m/%d/%Y %H:%M:%S",file_date(picspath+'/'+name))+'<br>'+(pics.length)+' pictures</p>');
}
write('</table></body></html>');
