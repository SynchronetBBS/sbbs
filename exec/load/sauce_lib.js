// $Id$
// vi: tabstop=4

const defs = {

	datatype: {
		none:		0,
		character:	1,
		bitmap:		2,
		vector:		3,
		audio:		4,
		bin:		5,
		xbin:		6,
		archive:	7,
		exe:		8,
	},

	// only 'character' data files are supported here:
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
		nonblink:		(1<<0),	// High intensity BG, aka iCE colors
		spacing_mask:	(3<<1),	// Letter spacing
		spacing_legacy:	(0<<1),	// Legacy value. No preference.
		spacing_8pix:	(1<<1),	// 8-pixel font
		spacing_9pix:	(2<<1), // 9-pixel font
		ratio_mask:		(3<<3),	// aspect ratio
		ratio_legacy:	(0<<3), // Legacy value. No preference.
		ratio_rect:		(1<<3), // Rectangular pixels
		ratio_square:	(2<<3),	// Square pixels
	},

	id_length: 5,
	version_length: 2,
	trailer_length: 128,
	comment_length: 64,
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
	if(file.read(defs.id_length + defs.version_length) != 'SAUCE00')
		return false;

	var obj = { comment:[], cols:0, rows:0, ice_color:false };
	obj.title = truncsp(file.read(35));
	obj.author = truncsp(file.read(20));
	obj.group = truncsp(file.read(20));
	obj.date = truncsp(file.read(8));
	obj.filesize = file.readBin(4);
	obj.datatype = file.readBin(1);
	obj.filetype = file.readBin(1);
	obj.tinfo1 = file.readBin(2);
	obj.tinfo2 = file.readBin(2);
	obj.tinfo3 = file.readBin(2);
	obj.tinfo4 = file.readBin(2);
	var comments = file.readBin(1);
	obj.tflags = file.readBin(1);
	obj.tinfos = truncsp(file.read(22));

	// Do some convenience field parsing/conversions here
	if(obj.datatype == defs.datatype.bin ||
		(obj.datatype == defs.datatype.character
			&& obj.filetype <= defs.filetype.ansimation)) {
		if(obj.tflags&defs.ansiflag.nonblink)
			obj.ice_color = true;
		if(obj.datatype == defs.datatype.bin) {
			obj.cols = obj.filetype * 2;
			obj.rows = obj.filesize / (obj.cols * 2);
		} else {
			obj.cols = obj.tinfo1;
			obj.rows = obj.tinfo2;
		}
		obj.fontname = obj.tinfos;
	}

	// Read the Comment Block here
	if(comments) {
		file.position -= defs.trailer_length + defs.id_length + (comments * defs.comment_length);
		if(file.read(defs.id_length) == 'COMNT') {
			while(comments--) {
				var line = file.read(defs.comment_length);
				if(line)
					obj.comment.push(truncsp(line));
			}
		}
	}
	return obj;
}

this;
