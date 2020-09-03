// $Id: showavatar.js,v 1.2 2019/06/15 01:23:30 rswindell Exp $

var usernum = user.number;
var draw = false;
var above = false;
var right = false;
var top = false;
for(var i in argv) {
	switch(argv[i]) {
		case '-draw':
			draw = true;
			break;
		case '-above':
			draw = true;
			above = true;
			break;
		case '-right':
			draw = true;
			right = true;
			break;
		case '-top':
			draw = true;		
			top = true;
			break;
		default:
			if(parseInt(argv[i], 10))
				usernum = parseInt(argv[i], 10);
	}
}
	
if(!bbs.mods.avatar_lib)
	bbs.mods.avatar_lib = load({}, 'avatar_lib.js');

if(draw) {
	bbs.mods.avatar_lib.draw(usernum, /* name: */null, /* netaddr: */null, above, right, top);	
	console.attributes = 7;	// Clear the background attribute as the next line might scroll, filling with BG attribute
} else {
	bbs.mods.avatar_lib.show(usernum);
}
