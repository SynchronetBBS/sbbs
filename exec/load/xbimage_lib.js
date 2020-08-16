// $Id: xbimage_lib.js,v 1.3 2018/02/06 09:08:30 rswindell Exp $

// Library for creating or reading XBin "image" (xbimage) files

var REVISION = "$Revision: 1.3 $".split(' ')[1];
var sauce_lib = load({}, 'sauce_lib.js');
var xbin = load({}, 'xbin_lib.js');
var cga = load({}, 'cga_defs.js');

const illegal_chars = [0, 7, 8, 9, 10, 12, 13, 27, 32];

var max_glyphs = (0x100 - illegal_chars.length) * 4;

function glyphs_from_bmp(bmp, charwidth, charheight)
{
	var glyph = [];
	var width = bmp.infoheader.biWidth / charwidth;
	var rowwidth = width;
	/* Each pixel row i padded to 32-bits */
	if((rowwidth%4) != 0)
		rowwidth += 4 - (rowwidth%4);
	// first convert into a large array of glyphs
	for(var y = 0; y < bmp.infoheader.biHeight; y++) {
		for(var x = 0; x < width; x++) {
			var i = (Math.floor(y / charheight) * width) + x;
			if(!glyph[i])
				glyph[i] = [];
			glyph[i][y%charheight] = bmp.bits.charAt((y * rowwidth) + x);
		}
	}

	return glyph;
}

function find_glyph(list, glyph)
{
	for(var i = 0; i < list.length; i++) {
		if(JSON.stringify(list[i]) == JSON.stringify(glyph))
			return i;
	}
	return -1;
}

function alloc_glyph(list)
{
	for(var i = 0; i < list.length; i++) {
		if(illegal_chars.indexOf(i&0xff) < 0 && list[i] === null)
			return i;
	}
	alert("Unable to allocate new glyph");
	return -1;
}

function objcopy(obj)
{
	return JSON.parse(JSON.stringify(obj));
}

function remap(map, height, width, index, glyph)
{
	var new_glyph = alloc_glyph(glyph);

	if(new_glyph < 0)
		return false;

	glyph[new_glyph] = objcopy(glyph[index]);
	glyph[index] = null;
//	print('new ' + new_glyph + ' old ' + index);

	for(var y = 0; y < height; y++)
		for(var x = 0; x < width; x++) {
			if(map[y][x] == index)
				map[y][x] = new_glyph;
		}

	return true;
}


function create(filename, glyph, charheight, width, height, fg_color, bg_color, palette, blink_first, sauce)
{
	if(blink_first === undefined)
		blink_first = true;

	if(fg_color == undefined)
		fg_color = cga.LIGHTGRAY;

	if(bg_color == undefined)
		bg_color = cga.BLACK;


	print("glyphs " + glyph.length);

	// Create map of glyph indexes to char cells (locations)
	var map = [];
	var index = 0;
	for(var y = 0; y < height; y++)
		for(var x = 0; x < width; x++) {
			if(!map[y])
				map[y] = [];
			map[y][x] = index++;
		}

	// De-duplicate the glyphs
	print("de-duplicating");
	var index = 0;
	for(var y = 0; y < height; y++) {
		for(var x = 0; x < width; x++) {
			var found = find_glyph(glyph, glyph[map[y][x]]);
			if(found != index) {
				map[y][x] = found;
				glyph[index] = null;	// Mark the glyph as a dupe
			}
			index++;
		}
	}
	print("done");

	for(var i in illegal_chars) {
		if(glyph[illegal_chars[i]])
			remap(map, height, width, illegal_chars[i], glyph);
	}

	for(var i = 256; i < height * width; i++) {
		if(glyph[i]) {
//			print("remapping glyph: " + i);
			remap(map, height, width, i, glyph);
		}
	}

	var unique = 0;
	for(var i = 0; i < glyph.length; i++)
		if(glyph[i])
			unique++;
	print("Unique glyphs: " + unique);

	if(unique > max_glyphs) {
		alert("Required glyphs exceeds maximum: " + max_glyphs);
		if(false) {
			var dump = [];
			for(var i = 0; i < glyph.length; i++) {
				if(!glyph[i])
					continue;

				var str = '';;
				for(var row = 0; row < charheight; row++) {
					str += format("%02X ", ascii(glyph[i][row]));
				}
				str += ': ' + i;
				dump.push(str);
			}
			dump.sort();
			for(var i in dump)
				printf("%s\r\n", dump[i]);
		}
		return false;
	}

	var font_data = [];
	var font_count = Math.ceil(unique / (0x100 - illegal_chars.length));
	for(var i = 0; i < font_count; i++) {
		font_data[i] = new Array(charheight * 0x100);
		for(var j = 0; j < font_data[i].length; j++)
			font_data[i][j] = '\0';
	}
	print("Total glyphs: " + glyph.length);
	print("Total glyphs: " + height * width);
	for(var i = 0; i < glyph.length; i++) {
		if(glyph[i]) {
			for(var row = 0; row < charheight; row++) {
				if(!glyph[i][row])
					glyph[i][row] = '\0';
//				print(i);
				font_data[Math.floor(i/256)][((i%256)*charheight) + row] = glyph[i][row];
			}
		}
	}

	print("Creating "  + filename);
	var file = new File(filename);
	if(!file.open('wb')) {
		alert(filename);
		return false;
	}
	file.write(xbin.id, xbin.ID_LEN);
	file.writeBin(width, 2);
	file.writeBin(height, 2);
	file.writeBin(charheight, 1);
	var flags;
	if(blink_first) {
		flags = xbin.FLAG_FONT_BLINK | xbin.FLAG_NONBLINK;
		if(font_count > 1)
			flags |= xbin.FLAG_FONT_HIGHBLINK | xbin.FLAG_NONHIGH;
		if(font_count > 2)
			flags |= xbin.FLAG_FONT_NORMAL;
		if(font_count > 3)
			flags |= xbin.FLAG_FONT_HIGH;
	} else {
		flags = xbin.FLAG_FONT_NORMAL;
		if(font_count > 1)
			flags |= xbin.FLAG_FONT_HIGH | xbin.FLAG_NONHIGH;
		if(font_count > 2)
			flags |= xbin.FLAG_FONT_BLINK | xbin.FLAG_NONBLINK;
		if(font_count > 3)
			flags |= xbin.FLAG_FONT_HIGHBLINK;
	}
	if(palette)
		flags |= xbin.FLAG_PALETTE;
	file.writeBin(flags, 1);

	if(palette && palette.length) {
		for(var i = 0; i < palette.length; i++)
			file.writeBin(palette[i], /* size (each color channel, in bytes): */1);
	}
	for(var i = 0; i < font_count; i++)
		file.write(font_data[i].join(''));

	for(var y = 0; y < height; y++) {
		for(var x = 0; x < width; x++) {
			var index = map[y][x];
			file.writeBin(index&0xff, 1);
			var attr = fg_color | (bg_color << 4);
			switch(index >> 8) {
				case 3:
					attr |= cga.HIGH;
				case 2:
					if(blink_first)
						break;
					attr |= cga.BLINK;
				case 1:
					attr |= cga.HIGH;
				case 0:
					if(blink_first)
						attr |= cga.BLINK;
			}
			file.writeBin(attr, 1);
		}
	}

	if(sauce) {
		if(!sauce.author)
			sauce.author = "xbimage_lib.js " + REVISION;
		if(!sauce.group)
			sauce.group = "Synchronet";
		sauce.datatype = sauce_lib.defs.datatype.xbin;
		sauce.filetype = 0;
		sauce.cols = width;
		sauce.rows = height;
		sauce_lib.write(file, sauce);
	}

	file.close();

	return true;
}

// Leave as last line:
this;