// $Id$

var cterm = load({}, 'cterm_lib.js');

function load_font(slot, filename)
{
	var f = new File(system.ctrl_dir + "fonts/" + filename);
	if(!f.open("rb")) {
		alert("FAILURE opening " + f.name);
		return false;
	}
	var data = f.read();
	f.close();
	cterm.load_font(slot, data);
	return true;
}

function fonts(key)
{
	var ini = new File(file_cfgname(system.ctrl_dir, "fonts.ini"));
	if(!ini.open('r'))
		return false;

	var charheight = cterm.charheight(console.screen_rows);
	var slotnames = ini.iniGetObject(null);
	var filenames = ini.iniGetObject("filenames:" + charheight);
	var obj = ini.iniGetObject(key + ':' + charheight);
	ini.close();
	if(!obj)
		return false;
	for(var p in obj) {
		if(parseInt(p, 10) || slotnames[p]) {	// load
			load_font(slotnames[p] ? slotnames[p] : p, obj[p]);
		} else {	// activate
			var slotval = slotnames[obj[p]];
			var slotnum = parseInt(slotval, 10);
			log(LOG_DEBUG, "activate: " + obj[p] + " slot " + slotnum + " >= " + cterm.font_slot_first);
			if(slotval !== undefined && slotnum >= cterm.font_slot_first
				&& !cterm.fonts_loaded[slotnum]) {
				// Need to load the font
				var filename;
				if(filenames)
					filename = filenames[obj[p]];
				if(!filename)
					filename = obj[p] + '.f' + charheight;
				load_font(slotnum, filename);
			}
			if(cterm.activate_font(cterm.font_styles[p], slotval == undefined ? obj[p] : slotnum) == false)
				log(LOG_WARNING, "Failed to activate font: " + cterm.font_styles[p]);
		}
	}
}

if(cterm.supports_fonts() != false)
	for(var i in argv)
		fonts(argv[i]);

this;