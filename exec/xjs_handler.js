/* Example Dynamic-HTML Content Parser */

/* $Id: xjs_handler.js,v 1.16 2012/09/01 02:02:46 echicken Exp $ */

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

