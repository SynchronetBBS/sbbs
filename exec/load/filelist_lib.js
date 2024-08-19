// Library to deal with lists of files (e.g. FILES.BBS files)

require("sbbsdefs.js", 'LEN_FDESC');

"use strict";

const filenames = [
	"FILES.BBS",
	"DESCRIPT.ION",
	"00_INDEX.TXT",
	"SFFILES.BBS"
];

// Parse a FILES.BBS (or similar) file listing file
// Returns an array of file-metadata-objects suitable for FileBase.add()
// Note: file descriptions must begin with an alphabetic character
//       unless desc_offset (description char-offset) is specified
function parse(lines, desc_offset, verbosity)
{
	var file_list = [];
	for(var i = 0; i < lines.length; i++) {
		var line = lines[i];
		var match = line.match(/(^[\w]+[\w\-\!\#\.]*)\W+[^A-Za-z]*(.*)/);
//		writeln('fname line match: ' + JSON.stringify(match));
		if(match && match.length > 1) {
			var file = { name: match[1], desc: match[2] };
			if(desc_offset)
				file.desc = line.substring(desc_offset).trim();
			if(file.desc && file.desc.length > LEN_FDESC)
				file.extdesc = word_wrap(file.desc, 45);
			file_list.push(file);
			continue;
		}
		match = line.match(/\W+\|\s+(.*)/);
		if(!match) {
			if(verbosity)
				alert("Ignoring line: " + line);
			continue;
		}
//		writeln('match: ' + JSON.stringify(match));
		if(match && match.length > 1 && file_list.length) {
			var file = file_list[file_list.length - 1];
			if(!file.extdesc)
				file.extdesc = file.desc + "\n";
			file.extdesc += match[1] + "\n";
			var combined = file.desc + " " + match[1].trim();
			if(combined.length <= LEN_FDESC)
				file.desc = combined;
		}
	}
	return file_list;
}

this;
