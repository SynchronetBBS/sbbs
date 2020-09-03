// $Id: xbin_lib.js,v 1.3 2018/02/06 09:13:08 rswindell Exp $

load('xbin_defs.js');

function read(file)
{
	if(file.read(this.ID_LEN) != this.id)
		return false;
	var image = {};
	image.width = file.readBin(2);
	image.height = file.readBin(2);
	image.charheight =file.readBin(1);
	image.flags = file.readBin(1);
	image.font_count = 0;
	if(image.flags&this.FLAG_FONT_NORMAL)
		image.font_count++;
	if(image.flags&this.FLAG_FONT_HIGH)
		image.font_count++;
	if(image.flags&this.FLAG_FONT_BLINK)
		image.font_count++;
	if(image.flags&this.FLAG_FONT_HIGHBLINK)
		image.font_count++;

	if(image.flags&this.FLAG_FONT_NORMAL)
		image.normal = (image.font_count-1)&2;	// Either 0 or 2
	if(image.flags&this.FLAG_FONT_HIGH)
		image.high = (image.font_count-1)&3;	// Either 1 or 3
	if(image.flags&this.FLAG_FONT_BLINK)
		image.blink = 0;
	if(image.flags&this.FLAG_FONT_HIGHBLINK)
		image.highblink = 1;

	if(image.flags&this.FLAG_PALETTE) {
		image.palette = [];
		for(var i = 0; i < 16; i++)	/* R G B, one byte each though only the low 6 bits should be used */
			image.palette[i] = file.readBin(/* size: */1, /* count: */3);
	}

//	printf("image.font_count = %d\r\n", image.font_count);
	image.font = [];
	for(var i = 0; i < image.font_count; i++)
		image.font[i] = file.read(0x100 * image.charheight);

	if(!(image.flags&this.FLAG_COMPRESSED))
		image.bin = file.read(image.width * image.height * 2);
	else {
		image.bin = '';
		while(image.bin.length < image.width * image.height * 2 && !file.error) {
			var byte = file.readBin(1);
			var type = byte >> 6;
			var repeat = (byte&0x3f) + 1;
			switch(type) {
				case 0:	// No compression
					for(var i = 0; i < repeat; i++) {
						image.bin += file.read(1);
						image.bin += file.read(1);
					}
					break;
				case 1:	// char compression
					var char = file.read(1);
					for(var i = 0; i < repeat; i++) {
						image.bin += char;
						image.bin += file.read(1);
					}
					break;
				case 2:	// attr compression
					var attr = file.read(1);
					for(var i = 0; i < repeat; i++) {
						image.bin += file.read(1);
						image.bin += attr;
					}
					break;
				case 3:	// char & attr compression
					var char = file.read(1);
					var attr = file.read(1);
					for(var i = 0; i < repeat; i++) {
						image.bin += char;
						image.bin += attr;
					}
					break;
			}
		}		
	}

	return image;
}

function write(file, image)
{
	if(image.flags)
		image.flags &= ~this.FLAG_COMPRESSED;
	file.write(this.id, this.ID_LEN);
	file.writeBin(image.width, 2);
	file.writeBin(image.height, 2);
	file.writeBin(image.charheight, 1);
	file.writeBin(image.flags, 1);

	if(image.palette && image.palette.length) {
		for(var i = 0; i < image.palette.length; i++)
			file.writeBin(image.palette[i], /* size (each color channel, in bytes): */1);
	}
	for(var i = 0; i < image.font_count; i++)
		file.write(image.font[i]);

	file.write(image.bin);
}

// Leave as last line:
this;