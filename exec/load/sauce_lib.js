// $Id$
// vi: tabstop=4

const defs = {

	datatype: {
		none:		0,
		character:	1,
		bitmap:		2,
		vector:		3,
		audio:		4,
		bintext:	5,
		xbin:		6,
		archive:	7,
		exe:		8,
	},

	filetype: {
		ascii:		0,
		ansi:		1,
		ansimation:	2,
		rip:		3,
		pcboard:	4,
		avatar:		5,
		html:		6,
		source:		7,
		tundra:		8,
	},

	ansiflag: {
		nonblink:		(1<<0),	// high intensity BG, aka iCE colors
		spacing_mask:	(3<<1),	// letter spacing
		spacing_legacy:	(0<<1),	// Legacy value. No preference.
		spacing_8pix:	(1<<1),	// 8-pixel font
		spacing_9pix:	(2<<1), // 9-pixel font
		ratio_mask:		(3<<3),	// aspect ratio
		ratio_legacy:	(0<<3), // Legacy value. No preference.
		ratio_rect:		(1<<3), // Rectangular pixels
		ratio_square:	(2<<3),	// Square pixels
	},

	trailer_length: 128
};

// Pass either a filename or a File instance
function read(fname)
{
	var file;

	if(typeof fname == 'object')
		file = fname;
	else {
		file = new File(fname);
		if(!file.open("rb"))
			return false;
	}

	if(file.length < defs.trailer_length)
		return false;

	file.position = file.length - defs.trailer_length;
	if(file.read(7) != 'SAUCE00')
		return false;

	var obj = {};
	obj.title = file.read(35).trim();
	obj.author = file.read(20).trim();
	obj.group = file.read(20).trim();
	obj.date = file.read(8).trim();
	obj.filesize = file.readBin(4);
	obj.datatype = file.readBin(1);
	obj.filetype = file.readBin(1);
	obj.tinfo1 = file.readBin(2);
	obj.tinfo2 = file.readBin(2);
	obj.tinfo3 = file.readBin(2);
	obj.tinfo4 = file.readBin(2);
	obj.comments = file.readBin(1);
	obj.tflags = file.readBin(1);
	obj.tinfos = truncsp(file.read(22));
	obj.ice_color = function ()  {
		return this.filetype == defs.filetype.ansi && this.tflags&defs.ansiflag.nonblink;
	}
	return obj;
}


this;
