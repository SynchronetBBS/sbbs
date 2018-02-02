// $Id$

// Utility module for creating and displaying XBin image files.

var image_lib = load({}, 'xbimage_lib.js');
var bmplib = load({}, 'bmp_lib.js');
var cga = load({}, 'cga_defs.js');
var sauce_lib = load({}, 'sauce_lib.js');

function convert_from_bmp(filename, charheight, fg_color, bg_color, invert, blink_first, title, author, group)
{
	if(!charheight)
		charheight = 16;
	if(fg_color === undefined)
		fg_color = cga.LIGHTGRAY;
	if(bg_color === undefined)
		bg_color = cga.BLACK;

	var file = new File(filename);

	if(!file.open('rb')) {
		alert('Error opening ' + file.name);
		return false;
	}

	var bmp = bmplib.read(file, invert);
	if(!bmp)
		return false;

	if(bmp.infoheader.biWidth%8 != 0) {
		alert(format("Bitmap width (%u) is not evenly-divisible by 8", bmp.infoheader.biWidth));
		return false;
	}

	// Add SAUCE 
	var sauce = {
		 title: title ? title : filename
		,author: author
		,group: group
		,comment: []
		};

	var glyph = image_lib.glyphs_from_bmp(bmp, 8, charheight);

	printf("Converting %s (%ux%u) ", filename, bmp.infoheader.biWidth, bmp.infoheader.biHeight);
	filename = filename.slice(0, -file_getext(filename).length) + '.' + charheight + ".xb" ;
	printf("to %u char-height XBin: %s\n", charheight, filename);
	var width = Math.ceil(bmp.infoheader.biWidth / 8);
	var height = Math.ceil(bmp.infoheader.biHeight / charheight)
	if(!image_lib.create(filename, glyph, charheight
		, width, height
		, fg_color, bg_color
		, blink_first
		, sauce
		)) {
		alert("Failed");
		return false;
	}
	printf("%s (%ux%u) created successfully\r\n", filename, width, height);
	return true;
}

function show(filename, xpos, ypos, fg_color, bg_color, delay, cycle, cleanup)
{
	var cterm = load({}, 'cterm_lib.js');

	if(cterm.supports_fonts() === false)
		return false;

	if(!file_getext(filename))
		filename += '.' + cterm.charheight(console.screen_rows) + '.xb';

	var file = new File(filename);

	if(!file.open('rb')) {
		alert('Error opening ' + file.name);
		return false;
	}

	var image = image_lib.read(file);
	file.close();
	if(!image)
		return false;

//	print(lfexpand(JSON.stringify(image, null, 4)));
//	console.getkey();
	
	var result = cterm.xbimage_draw(image, xpos, ypos, fg_color, bg_color, delay, cycle);
	if(cleanup)
		cterm.xbimage_cleanup(image);

	if(typeof result == 'string')
		alert(result);

	return result;
}

function demo(filename, delay)
{
	var cterm = load({}, 'cterm_lib.js');

	if(cterm.supports_fonts() === false) {
		log(LOG_WARNING, "No CTerm/Font support detected");
		return false;
	}

	var ini = new File(filename);
	if(!ini.open('r')) {
		alert(filename);
		return false;
	}

	var global = ini.iniGetObject();
	var obj = ini.iniGetAllObjects();
	ini.close();

	if(global.name) {
		print("\1n\1hDemonstration beginning: \1y" + global.name);
		console.inkey(0, delay);
	}
	if(global.delay === undefined)
		global.delay = delay;


	for(var i in obj) {
		if(obj[i].delay === undefined)
			obj[i].delay = global.delay;
		if(obj[i].xpos === undefined)
			obj[i].xpos = global.xpos;
		if(obj[i].ypos === undefined)
			obj[i].ypos = global.ypos;

		if(show(obj[i].name, obj[i].xpos, obj[i].ypos
			,obj[i].fg_color, obj[i].bg_color, obj[i].delay, obj[i].cycle) != true)
			break;
		if(console.aborted)
			break;
	}
	cterm.xbimage_cleanup();
	if(global.name) {
		print("\1n\1hDemonstration concluded: \1y" + global.name);
		console.inkey(0, delay);
	}
}

function info(filename, verbose)
{
	var file = new File(filename);
	if(!file.open('rb')) {
		alert('Error opening ' + file.name);
		return false;
	}

	printf(format("%-64s : ", file.name));
	var image = image_lib.read(file);
	file.close();
	printf("%4u x %-4u Flags(%x) Fonts(%u) Char(%u)\r\n"
		,image.width, image.height, image.flags, image.font.length, image.charheight);
	if(verbose) {
		image.bin = "[" + image.bin.length + "]";
		for(var i in image.font)
			image.font[i] = "[" + image.font[i].length + "]";
		print(lfexpand(JSON.stringify(image, null, 4)));
	}
	return true;
}

function main()
{
	var optval={};
	var cmds=[];
	var files = [];
	var charheights = [];
	var fg_color;
	var bg_color;
	var delay;
	var cycle;
	var blink_first = true;
	var invert = false;
	var verbose = false;
	var xpos;
	var ypos;
	var title;
	var author;
	var group;

	for(var i in argv) {

		var arg = argv[i];
		var val = ""
		var eq = arg.indexOf('=');

		if(eq > 0) {
			val = arg.substring(eq+1);
			arg = arg.substring(0,eq);
		}
		optval[arg] = val;

		switch(arg) {
			case "-fg":
				fg_color = parseInt(val, 10);
				break;
			case "-bg":
				bg_color = parseInt(val, 10);
				break;
			case "-delay":
				delay = parseInt(val, 10);
				break;
			case "-cycle":
				cycle = true;
				break;
			case "-invert":
				invert = true;
				break;
			case "-char":
				charheights.push(parseInt(val, 10));
				break;
			case "-title":
				title = val;
				break;
			case "-author":
				author = val;
				break;
			case "-group":
				group = val;
				break;
			case "-normal":
				blink_first = false;
				break;
			case "-v":
				verbose = true;
				break;
			case "create":
			case "show":
				cmds.push(arg);
				break;
			case "demo":
				cmds.push(arg);
				break;
			case "info":
				cmds.push(arg);
				break;
			default:
				if(file_exists(arg))
					files.push(arg);
				else
					alert("File does not exist: " + arg);
				break;
		}
	}

	if(!charheights.length)
		charheights = [16, 14, 8];

	for(var ci in cmds) {
		var cmd = cmds[ci].toLowerCase();
		switch(cmd) {
			case "create":
				for(var ch in charheights)
					if(!convert_from_bmp(files[0], charheights[ch], fg_color, bg_color, invert, blink_first,title, author, group))
						break;
				break;
			case "show":
				show(optval[cmd] ? optval[cmd] : files[0], xpos, ypos, fg_color, bg_color, delay, cycle, /* cleanup: */true);
				load('fonts.js', 'default');
				break;
			case "demo":
				demo(optval[cmd] ? optval[cmd]: files[0], delay);
				load('fonts.js', 'default');
				break;
			case "info":
				for(var i in files)
					info(files[i], verbose);
				break;
		}
	}
}

main();
