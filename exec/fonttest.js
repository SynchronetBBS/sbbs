// $Id: fonttest.js,v 1.1 2018/02/02 13:04:12 rswindell Exp $

var cterm = load({}, 'cterm_lib.js');
var ansiterm = load({}, 'ansiterm_lib.js');
load('sbbsdefs.js');

var verbose = true;

function printable(ch)
{
	switch(ascii(ch)) {
		case '\x00':
		case '\x07':
		case '\b':
		case '\t':
		case '\r':
		case '\n':
		case '\f':
		case '\x1b':
			return '.';
	}
	return ch;
}

function demo_font(slot, blink)
{
	print('Enabled modes ' + cterm.query_mode());
	var activated = cterm.activate_font(cterm.font_styles.blink, slot);
	print("Activate font result: " + activated);
	if(activated !== true)
		return;
	ansiterm.send('ext_mode', blink ? 'clear':'set', 'no_blink');
	console.attributes = LIGHTGRAY|BLINK;
	for(var ch = 0; ch < 255; ch++) {
		if(verbose)
			console.write(format("%02x%c ", ch, printable(ch)));
		else
			console.write(format("%c", printable(ch)));
	}
	console.crlf();
	printf("\xc9\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xbb\r\n");
	printf("\xba Hello, World! This is font %-3u \xba\r\n", slot);
	printf("\xc8\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xbc\r\n");
	printf("\1nNormal  High     Blink    High-blink\r\n");
	printf("\1nExample \1hExample  \1n\1iExample  \1hExample\r\n");
	console.crlf();
	console.attributes = LIGHTGRAY;
}

function demo_files()
{
	var files = directory(argv[0]);
	var filenum = 0;
	var blink = false;
	while(files.length && bbs.online) {
		console.clear(WHITE);
		printf("Font file: %s\1n\r\n\r\n", files[filenum]);
		var f = new File(files[filenum]);
		if(!f.open("rb")) {
			alert("FAILURE opening " + f.name);
			break;
		}
		var data = f.read();
		f.close();
		cterm.load_font(cterm.font_slot_last, data, /* force: */true);
		demo_font(cterm.font_slot_last, blink);
		console.mnemonics("Which, ~+, ~-, ~Blink-toggle, ~Verbosity-toggle, ~Delete, or ~Quit: ");
		var which = console.getkeys('-+BQDV', files.length - 1);
		switch(which) {
			case 'Q':
				exit();
			case 'V':
				verbose = !verbose;
				break;
			case 'B':
				blink = !blink;
				break;
			case '+':
				filenum++;
				if(filenum >= files.length)
					filenum = 0;
				break;
			case 'D':
				file_remove(files[filenum]);
				files.splice(filenum, 1);
				// fall-through
			case '-':
				filenum--;
				if(filenum < 0)
					filenum = files.length - 1;
				break;
			default:
				filenum = parseInt(which);
				if(!filenum)
					filenum = 0;
				break;
		}
	}
}

function demo_slots()
{
	var slot = 0;
	var blink = false;
	while(bbs.online) {
		console.clear(WHITE);
		printf("Slot %d\1n\r\n\r\n", slot);
		demo_font(slot, blink);
		console.mnemonics("Which, ~+, ~-, ~Blink-toggle, ~Verbosity-toggle, or ~Quit: ");
		var which = console.getkeys('-+QVB', cterm.font_slot_last);
		switch(which) {
			case 'Q':
				exit();
			case 'V':
				verbose = !verbose;
				break;
			case 'B':
				blink = !blink;
				break;
			case '+':
				slot++;
				if(slot > cterm.font_slot_last)
					slot = 0;
				break;
			case '-':
				slot--;
				if(slot < 0)
					slot = cterm.font_slot_last;
				break;
			default:
				slot = parseInt(which);
				if(!slot)
					slot = 0;
				break;
		}
	}
}

function main()
{
	ansiterm.send('ext_mode', 'save_all');
	js.on_exit("ansiterm.send('ext_mode', 'restore_all')");

	if(argc)
		demo_files();
	else
		demo_slots();
}

main();