// $Id: showfileavatar.js,v 1.5 2019/06/15 01:23:30 rswindell Exp $

const FM_ANON			=(1<<1);
require("userdefs.js", 'USER_ANSI');

// Avatar support here:
if(!(bbs.file_attr&FM_ANON) && console.term_supports(USER_ANSI)) {
	if(!bbs.mods.avatar_lib)
		bbs.mods.avatar_lib = load({}, 'avatar_lib.js');
	bbs.mods.avatar_lib.draw(null, bbs.file_uploader, null, /* above: */true, /* right-justified: */true);
	console.attributes = 7;	// Clear the background attribute as the next line might scroll, filling with BG attribute
}