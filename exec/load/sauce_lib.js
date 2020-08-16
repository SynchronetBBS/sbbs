// $Id: sauce_lib.js,v 1.10 2020/03/26 07:40:03 rswindell Exp $
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
	max_comments: 255,
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

	if(file.length < defs.trailer_length) {
		if(typeof fname != 'object')
			file.close();
		return false;
	}

	file.position = file.length - defs.trailer_length;
	if(file.read(defs.id_length + defs.version_length) != 'SAUCE00') {
		if(typeof fname != 'object')
			file.close();
		return false;
	}

	var obj = {};
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
	obj.comment = [];

	// Convenience fields
	obj.cols =  0;
	obj.rows = 0
	obj.ice_color = false;

	// Do some convenience field parsing/conversions here
	obj.date = new Date(obj.date.substr(0, 4), parseInt(obj.date.substr(4,2))-1, obj.date.substr(6,2));
	if(obj.datatype == defs.datatype.bin ||
		(obj.datatype == defs.datatype.character
			&& obj.filetype <= defs.filetype.ansimation)) {
		if(obj.tflags&defs.ansiflag.nonblink)
			obj.ice_color = true;
		if(obj.datatype == defs.datatype.bin) {
			obj.cols = obj.filetype * 2;
			obj.rows = Math.floor(obj.filesize / (obj.cols * 2));
		} else {
			obj.cols = obj.tinfo1;
			obj.rows = obj.tinfo2;
		}
	} else if(obj.datatype == defs.datatype.xbin) {
		obj.cols = obj.tinfo1;
		obj.rows = obj.tinfo2;
	}

	// Read the Comment Block here
	if(comments) {
		file.position -= defs.trailer_length + defs.id_length + (comments * defs.comment_length);
		if(file.read(defs.id_length) == 'COMNT') {
			while(comments--) {
				var line = file.read(defs.comment_length);
				if(!line)
					continue;
				var a = line.split('\n');	// Work-around for PabloDraw-Windows
				for(var i in a)
					obj.comment.push(truncsp(a[i]));
			}
		}
	}
	if(typeof fname != 'object')
		file.close();
	return obj;
}

function valueof(val)
{
	return val ? val : "";
}

// Pass either a filename or a File instance
function write(fname, obj)
{
	var file;

	if(typeof fname == 'object')
		file = fname;
	else {
		file = new File(fname);
		if(!file.open("r+b"))
			return false;
	}

	var existing = this.read(file);
	if(existing)
		file.position = existing.filesize;
	else
		file.position = file.length;
	obj.filesize = file.position;
	file.truncate(obj.filesize);

	// Do some convenience field parsing/conversions here
	if(obj.ice_color == true)
		obj.tflags |= defs.ansiflag.nonblink;
	switch(obj.datatype) {
		case defs.datatype.bin:
			obj.filetype = obj.cols / 2;
			break;
		case defs.datatype.xbin:
		case defs.datatype.character:
			obj.tinfo1 = obj.cols;
			obj.tinfo2 = obj.rows;
			break;
	}	

	file.write('\x1a');	// Ctrl-Z, CPM-EOF
	if(obj.comment.length) {
		file.write('COMNT');
		for(var i in obj.comment)
			file.printf("%-64s", valueof(obj.comment[i]));
	}
	file.write('SAUCE00');
	file.printf("%-35.35s", valueof(obj.title));
	file.printf("%-20.20s", valueof(obj.author));
	file.printf("%-20.20s", valueof(obj.group));
	// Use current date:
	var now = new Date();
	file.printf("%04u%02u%02u", now.getFullYear(), now.getMonth() + 1, now.getDate());
	file.writeBin(obj.filesize);
	file.writeBin(obj.datatype, 1);
	file.writeBin(obj.filetype, 1);
	file.writeBin(obj.tinfo1, 2);
	file.writeBin(obj.tinfo2, 2);
	file.writeBin(obj.tinfo3, 2);
	file.writeBin(obj.tinfo4, 2);
	file.writeBin(obj.comment.length, 1);
	file.writeBin(obj.tflags, 1);
	file.write(valueof(obj.tinfos), 22);
	if(typeof fname != 'object')
		file.close();
	return true;
}

// Pass either a filename or a File instance
function remove(fname)
{
	var file;

	if(typeof fname == 'object')
		file = fname;
	else {
		file = new File(fname);
		if(!file.open("r+b"))
			return false;
	}

	var obj = this.read(file);
	var result = file.truncate(obj.filesize);
	if(typeof fname != 'object')
		file.close();
	return result;
}

this;
