// Performs text.dat string replacements from ctrl/text.*.ini and/or ctrl/text.*.json

// one or more replacement files must be passed on the command-line in the form:
// ctrl/text.<arg>.ini or ctrl/text.<arg>.json

// see ctrl/text.*.ini for example text.ini syntax

/*** example text.json:
{
	"LANG": "Español",
	"On": "En",
	"Off": "Apagado",
	"Yes": "Si",
	"No": "No",
	"None": "Nada",
	"All": "Todo",
	"Only": "Solamente",
	"Unlimited": "ilimitado",
	"Scanning": "Escaneo",
	"Done": "Hecho",
}
******************************************/

const text = require('text.js', 'TOTAL_TEXT');

"use strict";

function replace_text(fname) {
	const ext = file_getext(fname).toLowerCase();
	if(ext != ".ini" && ext != ".json") {
		if(file_exists(fname + ".ini"))
			return replace_text(fname + ".ini")
		if(file_exists(fname + ".json"))
			return replace_text(fname + ".json");
		throw new Error("Filename does not exist: " + fname);
	}
	const file = new File(fname);
	if(!file.open('r'))
		throw new Error("Error " + file.error + " opening " + file.name);
	var obj;
	if(ext == ".ini")
		obj = file.iniGetObject();
	else if(ext == ".json")
		obj = JSON.parse(file.readAll().join("\n"));
	file.close();
	for(var i in obj) {
		if(!bbs.replace_text(text[i], obj[i]))
			throw new Error(format('Failed to replace string (%s = %s)', i, text[i]));
	}
	return true;
}

for(var i in argv)
	replace_text(system.ctrl_dir + "text." + argv[i]);
