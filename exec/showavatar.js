// $Id$

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
	
var Avatar = load({}, 'avatar_lib.js');
if(draw) {
	Avatar.draw(usernum, /* name: */null, /* netaddr: */null, above, right, top);	
	console.attributes = 7;	// Clear the background attribute as the next line might scroll, filling with BG attribute
} else {
	Avatar.show(usernum);
}
