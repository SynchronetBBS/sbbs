// dir.js

// Example use of global directory() method.

// $Id: dir.js,v 1.2 2005/10/12 08:49:13 rswindell Exp $

// @format.tab-size 8, @format.use-tabs true

load("sbbsdefs.js");

if (argc==0)
	path = prompt("Path");
else
	path = argv[0];
if (path==undefined)
	exit();
if (path.indexOf('*')<0 && path.indexOf('?')<0)
	path += "*"; // No pattern specified
print(path);
dir = directory(path,GLOB_PERIOD);
for (i in dir)  {
	if(this.bbs && bbs.sys_status&SS_ABORT)
		break;
	print(dir[i]);
}
