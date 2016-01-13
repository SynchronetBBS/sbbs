/*
 * Pass a directory code and get back an array of objects for the files
 * in the specified directory.
 * 
 * Similar to filedir.js.
 * 
 * Each element in the array is an object with the following properties:
 * 
 * base:       Base filename.
 * ext:        Filename extension.
 * datoffset:  Offset in .dat file of file data.
 * uldate:     Date uploaded
 * dldate:     Date downloaded(?)
 * credits:    Number of credits(?)
 * desc:       Short description (58 chars max)
 * uploader:   Name of user that uploaded the file.
 * downloaded: Number of times the file has been downloaded.
 * opencount:  I have no idea what this is.
 * misc:       Misc flags... see FM_* below.
 * anonymous:  Anonymous upload
 * altpath:    Index into the internal array of alt paths (not visible to JavaScript)
 * extdesc:    May be undefined... extended description.
 * path:       Full path to file.  Undefined if the file doesn't exist or
 *             is in an alt path (since they're not visible to JS).
 */

var FM_EXTDESC = 1;
var FM_ANON = (1<<1);

function FileBase(dir) {
	var f = new File(format("%s%s.ixb", file_area.dir[dir].data_dir, dir));
	var dat = new File(format("%s%s.dat", file_area.dir[dir].data_dir, dir));
	var exb = new File(format("%s%s.exb", file_area.dir[dir].data_dir, dir));
	var record = null;
	var ret = [];

	function FileEntry() {
		var byte;

		function getrec(file, len) {
			var ret=file.read(len);
			ret = ret.replace(/[\x03\r\n].*$/,'');
			return ret;
		}

		// Read from the IXB file
		this.base = f.read(8).replace(/ +$/,'');
		this.ext = f.read(3);
		this.datoffset = f.readBin(2);
		byte = f.readBin(1);
		this.datoffset |= (byte<<16);
		this.uldate = f.readBin(4);
		this.dldate = f.readBin(4);

		// Read from the DAT file
		dat.position = this.datoffset;
		this.credits = parseInt(getrec(dat, 9), 10);
		this.desc = getrec(dat, 58);
		dat.readBin(2);	// Should be \r\n
		this.uploader = getrec(dat, LEN_ALIAS);
		dat.read(5);	// Padding?
		dat.readBin(2);	// Should be \r\n
		this.downloaded = parseInt(getrec(dat, 5), 10);
		dat.readBin(2);	// Should be \r\n
		this.opencount = parseInt(getrec(dat, 3), 10);
		dat.readBin(2);	// Should be \r\n
		this.misc = getrec(dat, 1);
		if (this.misc.length > 0)
			this.misc = ascii(this.misc)-32;
		else
			this.misc = 0;
		if (this.misc & FM_ANON)
			this.anonymous = true;
		else
			this.anonymous = false;
		this.altpath = parseInt(getrec(dat, 2), 16);

		// Read from the EXB file
		var exbpos = (dat.position / 118) * 512;
		if (exb.is_open && (this.misc & FM_EXTDESC)) {
			if (exb.length > exbpos) {
				exb.position = exbpos;
				this.extdesc = exb.read(512).replace(/\x00.*$/, '');
			}
		}
		if (this.altpath > 0 && file_area.alt_paths !== undefined) {
			if (file_exists(file_area.alt_paths[this.altpath-1]+this.name))
				this.path = fullpath(file_area.alt_paths[this.altpath-1]+this.name);
		}
		else {
			if (file_exists(file_area.dir[dir].path+this.name))
				this.path = fullpath(file_area.dir[dir].path+this.name);
		}
	}
	FileEntry.prototype = {
		get name() {
			return this.base+'.'+this.ext;
		}
	};

	if(!f.exists)
		return ret;
	if (!f.open("rb"))
		return ret;

	if(!dat.exists) {
		f.close();
		log(LOG_DEBUG, "readIxb: Invalid code or "+dat.name+" does not exist.");
		return ret;
	}
	if (!dat.open("rb")) {
		log(LOG_DEBUG, "readIxb: Unable to open "+dat.name+".");
		return ret;
	}

	exb.open("rb");

	for(var i = 0; f.position+1<f.length ; i++)
		ret.push(new FileEntry(i));

	f.close();
	dat.close();
	if (exb.is_open)
		exb.close();

	return ret;
}
