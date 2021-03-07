'use strict';

js.load_path_list.unshift(js.exec_dir+"dorkit/");
load("dorkit.js");
require("l2lib.js", "Player_Def");

var i;
var p;

lln('Working...');
for (i=0; i<200; i++) {
	p = pfile.get(i);
	if (p === null)
		break;
	p.battle = 0;
	p.busy = 0;
	p.onnow = 0;
	p.put();
}
file_removecase(getfname('update.tmp'));
lln('Done!  I\'ve also deleted the UPDATE.TMP file.');
