/*	filedir.js
	Provides the FileDir and DirFile objects for use in conjunction with the
	built-in file_area.dir and file_area.lib_list[].dir_list arrays.
	Use this to find details (filenames, descriptions, statistics, etc.) of
	files in a given directory of the file libraries.
*/

// Record & field sizes & positions, from sbbsdefs.h
var	LEN_FCDT = 9,
	LEN_FDESC = 58,
	LEN_ALIAS = 25;
	F_CDT = 0,
	F_DESC = (F_CDT + LEN_FCDT),
	F_ULER = (F_DESC + LEN_FDESC + 2),
	F_TIMESDLED = (F_ULER + 30 + 2),
	F_OPENCOUNT	= (F_TIMESDLED + 5 + 2),
	F_MISC = (F_OPENCOUNT + 3 + 2),
	F_ALTPATH = (F_MISC + 1),
	F_LEN = (F_ALTPATH + 2 + 2),
	F_IXBSIZE = 22,
	F_EXBSIZE = 512;

/*	Returns a slice of array of numbers 'arr' of length 'count' starting at
	'index', converted into a string, assuming that each element is an
	ASCII code.  ASCII 3 (ETX) and 0 (NUL) are omitted from the string, as
	they seem to be the filler characters of choice in .dat and .exb files. */
var sliceToString = function(arr, index, count) {
	var tmp = "";
	for(var i = index; i < index + count; i++) {
		if(arr[i] != 3 && arr[i] != 0)
			tmp += ascii(arr[i]);
	}
	return tmp;
}

/*	An attempt at formatting 'filename' so that it can be matched against
	snprintf "%11.11s" filenames in the .ixb files. */
var ixbFilenameFormat = function(filename) {

	var ext = file_getext(filename);
	if(typeof ext == "string" && ext.length > 1) {
		filename = filename.replace(ext, "");
		filename = format("%-8s", filename);
		filename += ext.substr(1);
	} else {
		filename = format("%-11s", filename);
	}

	return filename;

}

/*	Read a record from a file directory's .ixb file
	-	Where 'dir' is a property of file_area.dir or an element of the
		file_area.lib_list[x].dir_list array
	-	Where 'filename' is the filename (without path) of a file in the
		directory's data storage directory
	-	Returns an object representing the record (see the return value)
		-	Use 'datOffset' in conjunction with readDat to locate this file's
			.dat record
*/
var readIxb = function(dir, filename) {

	var fn = format("%s%s.ixb", dir.data_dir, dir.code);
	if(!file_exists(fn))
		throw "readIxb: Invalid code or .ixb file does not exist.";

	var record = null;
	var matchName = ixbFilenameFormat(filename);

	var f = new File(fn);
	f.open("rb");
	for(var i = 0; i < f.length; i += F_IXBSIZE) {
		if(i + F_IXBSIZE > f.length)
			throw "readIxb: Invalid offset.";
		f.position = i;
		var ixb = f.readBin(1, F_IXBSIZE);
		var str = format("%11.11s", sliceToString(ixb, 0, 11));
		if(str != matchName)
			continue;
		record = ixb;
		break;
	}
	f.close();

	if(record === null)
		return;

	var ret = {
		'datOffset' : record[11]|(record[12]<<8)|(record[13]<<16),
		'uploadDate' : record[14]|(record[15]<<8)|(record[16]<<16)|(record[17]<<24),
		'downloadDate' : record[18]|(record[19]<<8)|(record[20]<<16)|(record[21]<<24)
	};


	return ret;

}

/*	Read a record from a file directory's .dat file
	-	Where 'dir' is a property of file_area.dir or an element of the
		file_area.lib_list[x].dir_list array
	-	Where 'offset' is the record's starting position in the file (as read
		from the file's record in the directory's .ixb file)
	-	Returns an object representing the record (see the return value)
*/
var readDat = function(dir, offset) {

	var fn = format("%s%s.dat", dir.data_dir, dir.code);
	var f = new File(fn);
	f.open("rb");
	if(offset + F_LEN > f.length)
		throw "readDat: Invalid offset.";
	f.position = offset;
	var chunk = f.readBin(1, F_LEN);
	f.close();

	// Some (altPath) or all of these Number conversions are likely wrong.
	var ret = {
		altPath : Number(sliceToString(chunk, F_ALTPATH, 2)),
		credit : Number(sliceToString(chunk, F_CDT, LEN_FCDT)),
		description : sliceToString(chunk, F_DESC, LEN_FDESC),
		uploader : sliceToString(chunk, F_ULER, LEN_ALIAS),
		timesDownloaded : Number(sliceToString(chunk, F_TIMESDLED, 5)),
		openCount : Number(sliceToString(chunk, F_OPENCOUNT, 3))
	};

	return ret;

}

/*	Read a record from a file directory's .exb file
	-	Where 'dir' is a property of file_area.dir or an element of the
		file_area.lib_list[x].dir_list array
	-	Where 'offset' is the record's starting position in the file
		(ixb.datOffset/F_LEN) * F_EXBSIZE)
	-	Returns an empty string if .exb not present or record is absent
		Returns the exb record as a string otherwise
*/
var readExb = function(dir, offset) {

	if(offset < 0)
		return "";

	var fn = format("%s%s.exb", dir.data_dir, dir.code);
	if(!file_exists(fn))
		return "";

	var f = new File(fn);
	f.open("rb");
	if(offset + F_EXBSIZE <= f.length) {
		f.position = offset;
		var chunk = sliceToString(f.readBin(1, F_EXBSIZE), 0, F_EXBSIZE);
	}
	f.close();

	return (typeof chunk == "undefined") ? "" : chunk;
}

/*	DirFile object
	-	A file in a directory of a library
	-	Where 'dir' is a property of file_area.dir or an element of the
		file_area.lib_list[x].dir_list array
	-	Where 'filename' is the filename (without path) of a file in the
		directory's data storage directory
	-	Has read-only public properties for each item in the private
		'properties' object. (May add setters in the future for modifying
		this data.)
*/
var DirFile = function(dir, filename) {

	var self = this;

	var properties = {
		'fullPath' : dir.path + filename, 
		'name' : filename,
		'altPath' : 0, // ?
		'credit' : 0, // Credit?
		'description' : "",
		'extendedDescription' : "",
		'uploader' : "",
		'timesDownloaded' : 0,
		'uploadDate' : 0,
		'downloadDate' : 0,
		'openCount' : 0 // ?
	};

	Object.keys(properties).forEach(
		function(p) {
			self.__defineGetter__(p, function() { return properties[p];	});
		}
	);

	var init = function() {
		try {
			var ixb = readIxb(dir, filename);
		} catch(err) {
			throw "DirFile: " + err;
		}
		if(typeof ixb == "undefined")
			throw "DirFile: No ixb record found.";
		properties.uploadDate = ixb.uploadDate;
		properties.downloadDate = ixb.downloadDate;
		try {
			var dat = readDat(dir, ixb.datOffset);
		} catch(err) {
			throw "DirFile: " + err;
		}
		for(var d in dat) {
			if(typeof properties[d] == "undefined")
				continue;
			properties[d] = dat[d];
		}
		properties.extendedDescription = readExb(dir, (ixb.datOffset/F_LEN) * F_EXBSIZE);
	}

	init();

}

/*	FileDir object
	-	Get a list of all files in a file library directory
	-	Where 'dir' is a property of file_area.dir or an element of the
		file_area.lib_list[x].dir_list array
	-	FileDir.files will be an array of DirFile objects, representing all
		files in this directory. (See DirFile above.)
*/
var FileDir = function(dir) {

	var dir,
		files = [];

	this.__defineGetter__("files", function() { return files; });

	var init = function() {

		var list = directory(dir.path + "*").map(
			function(f, i, a) {
				var fn = file_getname(f);
				if(fn != "." || fn != "..")
					return fn;
			}
		);

		list.forEach(
			function(f) {
				try {
					var dirFile = new DirFile(dir, f);
					files.push(dirFile);
				} catch(err) {
					log(err);
				}
			}
		);

	}

	init();

}