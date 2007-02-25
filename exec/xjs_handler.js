/* Example Dynamic-HTML Content Parser */

/* $Id$ */

var xjs_filename;

if(this.http_request!=undefined)	/* Requested through web-server */
	xjs_filename = http_request.real_path;
else
	xjs_filename = argv[0];

var cwd='';

xjs_load(xjs_filename);

function xjs_compile(filename) {
	if(cwd != '') {
		if(filename.search(/^((\/)|([A-Za-z]:[\/\\]))/)==-1)
			filename=cwd+filename;
	}
	cwd=filename;
	cwd=backslash(cwd.replace(/[^\\\/]*$/,''));
	var ssjs_filename=filename+".ssjs";

	// Probably a race condition on Win32
	if(file_exists(ssjs_filename)) {
		if(file_date(ssjs_filename)<=file_date(filename)) {
			file_remove(ssjs_filename);
		}
	}

	if(!file_exists(ssjs_filename)) {
		var file = new File(filename);
		if(!file.open("r",true,8192)) {
			writeln("!ERROR " + file.error + " opening " + filename);
			exit();
		}
		var text = file.readAll(8192);
		file.close();

		var script="";

		var in_xjs=false;
		for (line in text) {
			var str=text[line];
			while(str != '') {
				if(!in_xjs) {
					if(str.search(/<\?(xjs)?\s+/)==-1) {
						if(str.substr(-5)=='<?xjs') {
							str=str.substr(0, str.length-5);
							in_xjs=true;
						}
						if(str.substr(-2)=='<?') {
							str=str.substr(0, str.length-2);
							in_xjs=true;
						}
						if(str != '')
							script += "writeln("+str.toSource()+");";
						str='';
					}
					else {
						str=str.replace(/^(.*?)<\?(xjs)?\s+/,
							function (str, p1, p2, offset, s) {
								if(p1 != '')
									script += "write("+p1.toSource()+");";
								in_xjs=true;
								return '';
							}
						);
					}
				}
				else {
					if(str.search(/\?>/)==-1) {
						script += str;
						str='';
					}
					else {
						str=str.replace(/^(.*?)\?>/,
							function (str, p1, offset, s) {
								script += p1+";";
								in_xjs=false;
								return '';
							}
						);
					}
				}
			}
			script += '\n';
		}

		var f=new File(ssjs_filename);
		if(f.open("w",false)) {
			f.write(script);
			f.close();
		}
	}
	return(ssjs_filename);
}

function xjs_load(filename)
{
	var old_cwd=cwd;

	load(xjs_compile(filename));
	cwd=old_cwd;
}
