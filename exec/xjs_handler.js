/* Example Dynamic-HTML Content Parser */

/* $Id$ */

load("xjs.js");
var xjs_filename;

if(this.http_request!==undefined)	/* Requested through web-server */
	xjs_filename = http_request.real_path;
else
	xjs_filename = argv[0];

var cwd='';

function xjs_load(filename)
{
	var old_cwd=cwd;

	load(xjs_compile(filename));
	cwd=old_cwd;
}

xjs_load(xjs_filename);

