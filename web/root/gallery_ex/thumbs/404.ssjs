var picspath=web_root_dir+'/gallery_ex/pics';
var thumbpath=web_root_dir+'/gallery_ex/thumbs';

var gallery=http_request.real_path;
gallery=gallery.replace(/^.*\/([^\/]*?)\/[^\/]*$/,"$1");
var filename=http_request.real_path;
filename=filename.replace(/.*\//,'');

if(!file_isdir(thumbpath+'/'+gallery)) {
	mkdir(thumbpath+'/'+gallery);
}
var fullthumb=thumbpath+'/'+gallery+'/'+filename
if(!file_exists(fullthumb)) {
	var fullpic=picspath+'/'+gallery+'/'+filename
	if(file_exists(fullpic)) {
		system.exec('convert '+fullpic+' -thumbnail 120 '+fullthumb);
	}
}

if(file_exists(fullthumb)) {
	http_reply.status="200 Ok";
	http_reply.header['Content-Type']='image/jpeg';
	var f=new File(fullthumb);
	f.open('r',true);
	write(f.read());
	f.close();
}
else {
	http_reply.status="404 Not Found";
	write("<html><head><title>404 Error</title></head><body>404 Not Found</body></html>");
}
