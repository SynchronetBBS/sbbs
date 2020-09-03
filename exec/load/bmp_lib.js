// $Id: bmp_lib.js,v 1.1 2018/02/02 12:46:44 rswindell Exp $

// Library for dealing with Microsoft BMP/DIB files

function read(file, invert)
{
	var fileheader = {};
	/* BITMAPFILEHEADER  */
	fileheader.bfType = file.readBin(2);
	if(fileheader.bfType != 0x4D42) {
		file.close();
		alert(fileheader.bfType);
		return false;
	}
	fileheader.bfSize = file.readBin(4);
	fileheader.bfReserved = file.readBin(4);
	fileheader.bfOffBits = file.readBin(4);

	var infoheader = {};
	/* BITMAPINFOHEADER */
	infoheader.biSize = file.readBin(4);
	infoheader.biWidth = file.readBin(4);
	infoheader.biHeight = file.readBin(4);
	infoheader.biPlanes = file.readBin(2);
	infoheader.biBitCount = file.readBin(2);
	infoheader.biCompress = file.readBin(4);
	infoheader.biSizeImage = file.readBin(4);
	infoheader.biXPelsPerMeter = file.readBin(4);
	infoheader.biYPelsPerMeter = file.readBin(4);
	infoheader.biClrUsed = file.readBin(4);
	infoheader.biClrImportant = file.readBin(4);

	file.position = fileheader.bfOffBits;
	var bits = file.read();
	file.close();

	if(infoheader.biHeight > 0)	{ /* bottom-up image, reverse it */
		var tmp = [];
		var target_y = 0;
		var width = infoheader.biWidth / 8;
		/* Each pixel row i padded to 32-bits */
		if((width%4) != 0)
			width += 4 - (width%4);
		for(var y = infoheader.biHeight -1 ; y > 0 ; y--, target_y++) {
			for(var x = 0; x < width; x++) {
				var ch = bits.charAt((y * width) + x);
				if(invert)
					ch = ascii(~ascii(ch));
				tmp[(target_y * width) + x] = ch;
			}
		}
		bits = tmp.join('');			
	} else
		infoheader.biHeight = -infoheader.biHeight;

	return { fileheader: fileheader, infoheader: infoheader, bits: bits };
}

this;