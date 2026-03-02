require("userdefs.js", 'USER_ANSI');
require("smbdefs.js", 'MSG_ANONYMOUS');

// Avatar support here:
if(!(bbs.file_attr & MSG_ANONYMOUS) && console.term_supports(USER_ANSI)) {
	if(!bbs.mods.avatar_lib)
		bbs.mods.avatar_lib = load({}, 'avatar_lib.js');
	bbs.mods.avatar_lib.draw(null
		, bbs.file_uploader, null
		, /* above: */true
		, /* right-justified: */true
		, /* top: */false
		, /* max_columns: */80);
	console.attributes = 7;	// Clear the background attribute as the next line might scroll, filling with BG attribute
}
