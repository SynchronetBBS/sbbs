/*
 * An intentionally simple TIC handler for Synchronet.
 *
 * How to set up... add a timed event:
 * Internal Code                   TICKIT
 * Start-up Directory
 * Command Line                    ?tickit.js
 * Enabled                         Yes
 * Execution Node                  1
 * Execution Months                Any
 * Execution Days of Month         Any
 * Execution Days of Week          None
 * Execution Time                  00:00
 * Requires Exclusive Execution    No
 * Force Users Off-line For Event  No
 * Native Executable               No
 * Use Shell to Execute            No
 * Background Execution            No
 * Always Run After Init/Re-init   Yes
 *
 * And set up a semaphore in your mailer... for binkd:
 * flag /sbbs/data/tickit.now *.tic *.TIC
 */

require("sbbsdefs.js", 'LEN_FDESC');
require("fidocfg.js", 'TickITCfg');
require("fido.js", 'FIDO');

var cfgfile;
var force_replace = false;
var use_diz_always = true;
var debug = false;

for (var i in argv) {
	if(argv[i] == "-force-replace")
		force_replace = true;
	else if(argv[i] == "-debug")
		debug = true;
	else if(argv[i][0] != '-')
		cfgfile = argv[i];
}

var tickit = new TickITCfg(cfgfile);
if(tickit.gcfg.forcereplace === true)
	force_replace = true;
var sbbsecho = new SBBSEchoCfg(tickit.gcfg.echocfg);
var file_list = {};
var files_imported = 0;

const REVISION = "2.0";

var tickitVersion = "TickIT "+REVISION;
// emit tickitVersion to the log for general purposes - wk42
log(LOG_INFO, tickitVersion);
// emit system.temp_dir to the log for debug purposes; mainly with which
// temp directory is used when tickit.js is executed (event vs jsexec) - wk42
log(LOG_DEBUG, "Using system.temp_dir = '"+system.temp_dir+"'");

if (!String.prototype.repeat) {
  String.prototype.repeat = function(count) {
    'use strict';
    if (this == null) {
      throw new TypeError('can\'t convert ' + this + ' to object');
    }
    var str = '' + this;
    count = +count;
    if (count != count) {
      count = 0;
    }
    if (count < 0) {
      throw new RangeError('repeat count must be non-negative');
    }
    if (count == Infinity) {
      throw new RangeError('repeat count must be less than infinity');
    }
    count = Math.floor(count);
    if (str.length == 0 || count == 0) {
      return '';
    }
    // Ensuring count is a 31-bit integer allows us to heavily optimize the
    // main part. But anyway, most current (August 2014) browsers can't handle
    // strings 1 << 28 chars or longer, so:
    if (str.length * count >= 1 << 28) {
      throw new RangeError('repeat count must not overflow maximum string size');
    }
    var rpt = '';
    for (;;) {
      if ((count & 1) == 1) {
        rpt += str;
      }
      count >>>= 1;
      if (count == 0) {
        break;
      }
      str += str;
    }
    // Could we try:
    // return Array(count + 1).join(this);
    return rpt;
  };
}

function rename_or_move(src, dst)
{
	if (file_rename(src, dst))
		return true;
	if (!file_copy(src, dst))
		return false;
	return file_remove(src);
}

function process_tic(tic)
{
	var dir;
	var path;
	var ld;
	var i,j;
	var cfg;
	var handler;
	var handler_arg;

	log(LOG_INFO, "Working with '"+tic.file+"' in '"+tic.area.toUpperCase()+"'.");

	if (tickit.gcfg.path !== undefined)
		path = backslash(tickit.gcfg.path);
	if (tickit.gcfg.dir !== undefined)
		dir = tickit.gcfg.dir.toLowerCase();
	if (tickit.gcfg.handler !== undefined) {
		handler = tickit.gcfg.handler;
		handler_arg = tickit.gcfg.handlerarg;
	}

	var force_replace_area = false;
	cfg = tickit.acfg[tic.area.toLowerCase()];
	if (cfg !== undefined) {
		if (cfg.path !== undefined) {
			path = backslash(cfg.path);
			dir = undefined;
		}
		if (cfg.dir !== undefined) {
			dir = cfg.dir.toLowerCase();
			if (cfg.path === undefined)
				path = undefined;
		}
		if (cfg.handler !== undefined) {
			handler = cfg.handler;
			handler_arg = cfg.handlerarg;
		}
		if (cfg.forcereplace === true) {
			log(LOG_INFO, "ForceReplace enabled for area "+tic.area.toUpperCase()+".");
			force_replace_area = true;
		}
	}

	if (handler !== undefined) {
		try {
			if (handler.Handle_TIC(tic, this, handler_arg))
				return true;
		}
		catch (e) {
			log(LOG_ERROR, "TICK Handler threw an exception: "+e);
		}
	}

	if (dir !== undefined) {
		if (file_area.dir[dir] === undefined) {
			log(LOG_ERROR, "Internal code '"+dir+"' invalid!");
			return false;
		}
		if (path === undefined)
			path = file_area.dir[dir].path;
		else {
			log(LOG_ERROR, "Having both Dir and Path set not currently supported!");
			return false;
		}
	}
	else {
		if (path === undefined) {
			log(LOG_ERROR, "Unable to find path for area '"+tic.area+"'!");
			return false;
		}
		if (!file_isdir(path)) {
			log(LOG_ERROR, "Path '"+path+"' does not exist!");
			return false;
		}
	}

	function do_move(path, tic) {
		var dst = path+tic.file;
		var actual = file_getcase(dst);
		if (actual) {
			file_remove(actual);
			dst = actual;
		}
		if (rename_or_move(tic.full_path, dst))
			tic.full_path = dst;
		else {
			log(LOG_ERROR, "Cannot move '"+tic.full_path+"' to '"+dst+"'!");
			return false;
		}
		return true;
	}

	log(LOG_INFO, "Moving "+tic.full_path+" to "+path+".");
	// TODO: optionally delete replaced files even if it's not an overwrite
	if (file_exists(path+tic.file) && !force_replace && !force_replace_area) {
		if (typeof tic.replaces == 'undefined') {
			log(LOG_ERROR, format('"%s" already exists in "%s", but has no matching Replaces line', tic.file, path));
			return false;
		} else if (!wildmatch(tic.file, tic.replaces)) {
			log(LOG_ERROR, format('"%s" already exists in "%s", but TIC Replaces line is "%s"', tic.file, path, tic.replaces));
			return false;
		} else {
			if (!do_move(path, tic)) return false;
		}
	} else {
		if (!do_move(path, tic)) return false;
	}

	if (dir !== undefined) {
		if (file_list[dir] === undefined)
			file_list[dir] = [];
		file = { name: tic.file, cost: tic.size, desc: tic.desc, extdesc: tic.ldesc };
		file_list[dir].push(file);
//		log(LOG_INFO, JSON.stringify(file));
	}
	log(LOG_INFO, "Deleting TIC file '"+tic.tic_filename+"'.");
	file_remove(tic.tic_filename);

	return true;
}

function add_links(seenbys, links, list)
{
	var l;
	var i;

	l = list.split(/,/);
	for (i=0; i<l.length; i++) {
		if (seenbys[l[i]] !== undefined) {
			log(LOG_INFO, "Node "+l[i]+" has already seen this file.");
			continue;
		}
		links[l[i]]='';
	}
}

function get_zone(addr)
{
	var m;

	// Figure out the default zone.
	m = addr.match(/^([0-9]+):/);
	if (m===null)
		return undefined;
	return parseInt(m[1], 10);
}

function forward_tic(tic)
{
	var seenbys={};
	var links={};
	var cfg;
	var link;
	var tf;
	var ff;
	var bf;
	var defzone;
	var addr;
	var outb;
	var flobase;
	var pw;
	var i;
	var addrs = [];
	var saddr;

	log(LOG_INFO, "Forwarding...");
	defzone = get_zone(system.fido_addr_list[0]);
	if (defzone === undefined) {
		log(LOG_ERROR, "Unable to detect fido_addr_list default zone!");
		return false;
	}

	for (saddr in system.fido_addr_list)
		addrs.push(FIDO.parse_addr(system.fido_addr_list[saddr]));

	// Populate seenbys from TIC file
	for (i=0; i<tic.seenby.length; i++)
		seenbys[tic.seenby[i]]='';

	// Add all our addresses...
	system.fido_addr_list.forEach(function(addr) {
		seenbys[addr]='';
	});

	// Calculate links
	if (tickit.gcfg.links !== undefined)
		add_links(seenbys, links, tickit.gcfg.links);
	cfg = tickit.acfg[tic.area.toLowerCase()];
	if (cfg !== undefined && cfg.links !== undefined)
		add_links(seenbys, links, cfg.links);

	// Add links to seenbys
	for (i in links)
		tic.seenby.push(i);

	// Now, start generating the TIC/FLO files...
	for (link in links) {
		if (!sbbsecho.is_flo) {
			log(LOG_ERROR, "TickIT doesn't support non-FLO mailers.");
			return false;
		}
		log(LOG_INFO, "... to "+link+".");

		// let's get this link's ticpassword from the sbbsecho.ini file
		// for now, we get to play with adding/removing the @domain part
		// to find the link in our configs. this is because we can list
		// them in echocfg with or without the @domain and the lookup
		// routine sbbsecho.get_ticpwd() requires an exact address match
		// - wk42
		log(LOG_DEBUG, "Searching for "+link+"'s password...");
		pw = sbbsecho.get_ticpw(link);
		if (pw===undefined || pw === '') {
			// if pw is undefined or empty, try looking up alink - wk42
			var alink = FIDO.parse_addr(link).toString();
			log(LOG_DEBUG, "Searching for password with domain this time: "+alink+"...");
			pw = sbbsecho.get_ticpw(alink);
			if (pw===undefined || pw === '') {
				// if we get here and pw is undefined or empty, there really
				// must not be one in sbbsecho.ini. log a warning about it. - wk42
				log(LOG_WARNING, "No TIC password defined for linked system "+link+" or "+alink+".");
				log(LOG_WARNING, "No Pw line will be written to this TIC file.");
			} else
				log(LOG_DEBUG, "Found "+alink+"'s password.");
		} else
			log(LOG_DEBUG, "Found "+link+"'s password.");

		// Figure out the outbound dir...
		addr = FIDO.parse_addr(link, defzone);
		if (addr.zone === undefined)
			addr.zone = defzone;

		if (addr.zone === undefined || addr.net === undefined || addr.node === undefined) {
			log(LOG_ERROR, "Address '"+link+"' is invalid!");
			continue;
		}

		outb = sbbsecho.outbound.replace(/[\\\/]+$/g, '');
		if (addr.zone !== defzone)
			outb += format(".%03x", addr.zone);
		outb = fullpath(outb);
		outb = backslash(outb);
		if (addr.point > 0) {
			outb += format("%04x%04x.pnt", addr.net, addr.node);
			outb = backslash(outb);
		}
		if (!mkpath(outb)) {
			log(LOG_ERROR, "Unable to create outbound directory '"+outb+"' for link "+link);
			continue;
		}

		// Create TIC file first...
		tf = new File(outb+tickit.get_next_tic_filename());
		if(!tf.open("wb")) {
			log(LOG_ERROR, "Unable to create TIC file for "+link+".  They will not get file '"+tic.file+"'!");
			continue;
		}

		// now let's figure out which address we're going to use in this
		// new TIC we are creating. this address will be used in the From
		// and Path lines, below.
		//
		// current selection order:
		//   1. per area AKA Match
		//   2. per area source address
		//   3. global AKA Match
		//   4. global source address
		//   5. main FTN address
		//
		// NOTE: we're slicing off @domain for now... we should be able
		// to use it since it is part of the definition of a FTN address
		// and should be OK to use in TICs but to prevent possible
		// problems with non-5D-aware TIC processors, we're slicing it
		// off for now. - wk42
		var whichaddress = "";
		if (cfg !== undefined && cfg.akamatching === true) {
			var dreks = [];
			var drek;
			for (drek in system.fido_addr_list)
				dreks.push(FIDO.parse_addr(system.fido_addr_list[drek]));
			drek = FIDO.parse_addr(link);
			FIDO.distance_sort(dreks, drek);
			whichaddress = dreks[0].toString();
			if (whichaddress.indexOf("@") > -1)
				whichaddress = whichaddress.slice(0,whichaddress.indexOf("@"));
			log(LOG_INFO, "Using '"+tic.area.toUpperCase()+"' area AKA match: "+whichaddress);
		} else if (cfg !== undefined && cfg.sourceaddress !== undefined) {
			whichaddress = cfg.sourceaddress.toString();
			if (whichaddress.indexOf("@") > -1)
				whichaddress = whichaddress.slice(0,whichaddress.indexOf("@"));
			log(LOG_INFO, "Using '"+tic.area.toUpperCase()+"' area source address: "+whichaddress);
		} else if (tickit.gcfg.akamatching) {
			var dreks = [];
			var drek;
			for (drek in system.fido_addr_list)
				dreks.push(FIDO.parse_addr(system.fido_addr_list[drek]));
			drek = FIDO.parse_addr(link);
			FIDO.distance_sort(dreks, drek);
			whichaddress = dreks[0].toString();
			if (whichaddress.indexOf("@") > -1)
				whichaddress = whichaddress.slice(0,whichaddress.indexOf("@"));
			log(LOG_INFO, "Using global AKA match: "+whichaddress);
		} else if (tickit.gcfg.sourceaddress) {
			whichaddress = tickit.gcfg.sourceaddress.toString();
			if (whichaddress.indexOf("@") > -1)
				whichaddress = whichaddress.slice(0,whichaddress.indexOf("@"));
			log(LOG_INFO, "Using global source address: "+whichaddress);
		} else if (addrs[0]) {
			whichaddress = addrs[0].toString();
			if (whichaddress.indexOf("@") > -1)
				whichaddress = whichaddress.slice(0,whichaddress.indexOf("@"));
			log(LOG_INFO, "Using main FTN address: "+whichaddress);
		} else {
			// we should not end up here because there should always be at
			// least one FTN address in the system.fido_addr_list if a
			// system is processing TIC files which are a FTN thing.
			// log it and get out. we've already deleted the TIC file we
			// were working from so now we have an abandoned file with
			// no TIC and we cannot create a proper TIC since we have no
			// FTN addresses defined :( - wk42
			log(LOG_ERROR, "No FTN system address defined!");
			return false;
		}

		// the TIC "Created" keyword identifies the software that created
		// this TIC file. there is no formal format for this optional
		// keyword. when passing files to other systems, new TICs are
		// created so we'll identify this tickit created one. we will
		// always add this line first when creating the new TIC file. - wk42
		tf.write("Created by "+tickitVersion+"\r\n");
		tf.write(tic[' forward'].join("\r\n"));
		tf.write('\r\n');
		// TODO: Use BinkpSourceAddress?
		// DONE: global and per area source address overrides and AKA Matching - wk42
		//		   source address override: SourceAddress=zone:net/node[.point][@domain] in tickit.ini
		//		   AKA Matching on/off: AKAMatching=[true|false] in tickit.ini
		//		 use the address selected above.
		tf.write('From '+whichaddress+'\r\n');
		tf.write('To '+link+'\r\n');
		// put in the password line only if we have a password for this link - wk42
		if (pw !== undefined && pw !== "") {
			tf.write('Pw '+pw+'\r\n');
		}
		// write existing Path lines to the new TIC...
		for (i=0; i<tic.path.length; i++)
			tf.write('Path '+tic.path[i]+'\r\n');
		// now generate our Path line with date/time stamp in UTC per
		// http://ftsc.org/docs/fts-5006.001 and write it to the TIC - wk42
		//
		// this required keyword has a format.
		//   Path zone:net/node[.point][@domain] time [time string] [tic processor data]
		// -----
		// This specifies the node number and the date and time of a
		// system that has processed the file.
		// The time parameter is expressed as a unix style timestamp,
		// optionally followed by the date and time in human readable
		// form. Unix timestamp is UTC. Human readable format may be
		// local time (including UTC offset) or UTC.
		//
		// File processors may add additional information. For example a
		// signature identifying the software generating the path line.
		//
		// Example:  Path 2:2/20 1415567298 Sun Nov 9 21:08:18 2014 UTC
		// -----
		//
		var d = new Date();
		var utstamp = Math.round(d.getTime()/1000);
		log(LOG_DEBUG, "Path "+whichaddress+" "+utstamp+" "+d.toUTCString()+"; "+tickitVersion);
		tf.write('Path '+whichaddress+' '+utstamp+' '+d.toUTCString()+'; '+tickitVersion+'\r\n');

		for (i=0; i<tic.seenby.length; i++)
			tf.write('Seenby '+tic.seenby[i]+'\r\n');
		tf.close();

		// Create bsy file...
		if (addr.point > 0)
			flobase = outb+format("%08x", addr.point);
		else
			flobase = outb+format("%04x%04x", addr.net, addr.node);
		bf = new File(flobase+'.bsy');
		while (!bf.open('wxb+')) {
			// TODO: This waits forever...
			log(LOG_WARNING, "Waiting for BSY file '"+bf.name+"'...");
			mswait(1000);
		}

		var fext = 'flo'; // normal
		if(sbbsecho.status[link] == 'crash')
			fext = 'clo';
		else if(sbbsecho.status[link] == 'hold')
			fext = 'hlo';
		else if(sbbsecho.direct[link] == true)
			fext = 'dlo';
		// Append to FLO file...
		ff = new File(flobase + '.' + fext);
		if (!ff.open('ab+')) {
			log(LOG_ERROR, "Unable to append to '"+ff.name+"' for "+link+".  They will not get file '"+tic.file+"'!");
			bf.close();
			bf.remove();
			continue;
		}
		ff.writeln(tic.full_path);
		ff.writeln('^'+tf.name);
		ff.close();
		bf.close();
		bf.remove();
	}

	return true;
}

function parse_ticfile(fname)
{
	var crc32;
	var key;
	var val;
	var m;
	var line;
	var tic={seenby:[], path:[], tic_filename:fname, desc:''};
	var outtic=[];
	var f=new File(fname);
	var dir = fullpath(fname).replace(/([\/\\])[^\/\\]*$/,'$1');
	var i;

	log(LOG_INFO, "Parsing "+fname);
	if (!f.open("r")) {
		log(LOG_ERROR, "Unable to open '"+f.name+"'");
		return false;
	}
	while ((line = f.readln(65535)) != undefined) {
		m = line.match(/^\s*([^\s]+)\s(.*)$/);
		if (m !== null) {
			key = m[1].toLowerCase();
			val = m[2];

			if (key !== 'desc' && key !== 'ldesc')
				key = key.replace(/^\s*/,'');
			if (key == 'desc' && tic.desc) {
				if (!tic.ldesc)
					tic.ldesc = tic.desc;
				key = 'ldesc';
			}
			switch(key) {
				// These are not passed unmodified.
				// Single value, single line...
				case 'from':
				case 'to':
				case 'pw':
					tic[key] = val;
					break;
				// Multiple values
				case 'seenby':
					tic[key].push(val);
					break;

				case 'path':
					// log the path lines for informational purposes before we apply
					// the circular path detection. - wk42
					log(LOG_DEBUG, "Path "+val);
					// Circular path detection...
					for (i=0; i<system.fido_addr_list.length; i++) {
						if (val === system.fido_addr_list[i]) {
							log(LOG_ERROR, "Circular path detected on address "+val+"!");
							return false;
						}
					}
					tic[key].push(val);
					break;

				case 'created':
					// the TIC "Created" keyword identifies the software that created
					// this TIC file. there is no formal format for this keyword.
					// when passing files to other systems, new TICs are created so
					// we'll log this one for informational purposes and throw it
					// away so we can create our own later in forward_tic() with our
					// tickit.js revision line. - wk42
					log(LOG_DEBUG, "Created "+val);
					break;

				// All the rest are passed through unmodified
				// Single value, single line...
				case 'area':
				case 'areadesc':
				case 'origin':
				case 'file':
				case 'lfile':
				case 'size':
				case 'date':
				case 'magic':
				case 'replaces':
				case 'crc':
				case 'desc':
					outtic.push(line);
					tic[key] = val;
					break;

				case 'fullname':
					outtic.push(line);
					tic.lfile = val;
					break;

				// Multi-line values
				case 'ldesc':
					outtic.push(line);
					if (tic[key] === undefined)
						tic[key] = '';
					tic[key] += val+"\r\n";
					break;

				default:
					outtic.push(line);
					log(LOG_WARNING, "Unhandled keyword '"+key+"'");
					break;
			}
		}
	}
	if (tic.desc !== undefined) {
		if(tic.desc.length > LEN_FDESC && !tic.ldesc)
			tic.ldesc = tic.desc.replace("  ", "\r\n");
		tic.desc = format("%.*s", LEN_FDESC, tic.desc.trim());
	}
	f.close();
	f = new File(dir+tic.file);
	if (!f.exists) {
		log(LOG_WARNING, "File '"+f.name+"' doesn't exist.");
		return false;
	}
	tic.full_path = f.name;
	if (tic.size !== undefined && f.length !== parseInt(tic.size, 10)) {
		log(LOG_WARNING, "File '"+f.name+"' length mismatch. File is "+f.length+", expected "+tic.size+".");
		return false;
	}
	tic.size = f.length;
	if (tic.crc !== undefined) {
		// File needs to be open to calculate the CRC32.
		if (!f.open("rb")) {
			log(LOG_WARNING, "Unable to open file '"+f.name+"'.");
			return false;
		}
		crc32 = crc32_calc(f.read());
		f.close();
		if (crc32 !== parseInt(tic.crc, 16)) {
			log(LOG_WARNING, "File '"+f.name+"' CRC mismatch. File is "+format("%08x", crc32)+", expected "+tic.crc+".");
			return false;
		}
	}

	if (tickit.gcfg.ignorepassword)
		log(LOG_DEBUG, "Global ignore password enabled.");
	else {
		// there may or may not be @domain on the from line in the
		// TIC file. look for both address forms. - wk42
		log(LOG_DEBUG, "Verifying password for sender: "+tic.from);
		if (!sbbsecho.match_ticpw(tic.from, tic.pw)) {
			var alink = FIDO.parse_addr(tic.from).toString();
			log(LOG_DEBUG, "Verifying password with domain this time: "+alink);
			if (!sbbsecho.match_ticpw(alink, tic.pw)) {
				// if we get here and there is no match, then we have
				// defined the wrong password somewhere... - wk42
				log(LOG_WARNING, "No password matched for sender "+tic.from+" or "+alink+".");
				log(LOG_WARNING, "Please check the defined password for this sender.");
				return false;
			} else
				log(LOG_INFO, "Matched "+alink+"'s password.");
		} else
			log(LOG_INFO, "Matched "+tic.from+"'s password.");
	}

	tic[' forward'] = outtic;

	return tic;
}

function import_file_list(dir, list, uploader)
{
	log(LOG_INFO, "Importing file list into: " + dir);
	var fb = new FileBase(dir);
	if(!fb.open())
		return "Error " + fb.last_error + " opening filebase: " + dir;
	for(var i = 0; i < list.length; i++) {
		var file = list[i];
		file.from = uploader;
		log(LOG_INFO, "Adding file (" + file.name + ") to: " + dir);
		if(!fb.add(file, use_diz_always)) {
			fb.close();
			return "Error " + fb.last_error + " adding file to: " + dir;
		} else
			files_imported++;
	}
	fb.close();
	return true;
}

// need the tic for some more processing
function import_files(tic)
{
	log(LOG_INFO, "Importing...");
	var i;
	var cmd;

	for (i in file_list) {
		if (file_area.dir[i] === undefined) {
			log(LOG_ERROR, "Invalid directory "+i+" when importing!");
			continue;
		}

		// figure out the uploader name if there is an override in place
		// globally or per area. - wk42
		var uploader = "";
		var cfg = tickit.acfg[tic.area.toLowerCase()];
		if (cfg !== undefined && cfg.uploader !== undefined) {
			uploader = cfg.uploader.toString();
			log(LOG_DEBUG, "Using '"+tic.area.toUpperCase()+"' area uploader: "+uploader);
		} else if (tickit.gcfg.uploader !== undefined) {
			uploader = tickit.gcfg.uploader.toString();
			log(LOG_DEBUG, "Using global uploader: "+uploader);
		}
		import_file_list(i, file_list[i], uploader);
	}
	file_list = {};
}

function main() {
	var i, j;
	var ticfiles;
	var tic;

	// check source FTN address overrides and list for debugging purposes - wk42
	//
	// current selection order:
	//   1. per area AKA Match
	//   2. per area source address
	//   3. global AKA Match
	//   4. global source address
	//   5. main FTN address
	//
	var addrs = [];
	var saddr;
	for (saddr in system.fido_addr_list)
		addrs.push(FIDO.parse_addr(system.fido_addr_list[saddr]));
	if (tickit.gcfg.akamatching) {
		log(LOG_DEBUG, "Global address is nearest AKA match.");
	} else if (tickit.gcfg.sourceaddress) {
		saddr = tickit.gcfg.sourceaddress.toString();
		if (saddr.indexOf("@") > -1)
			saddr = saddr.slice(0,saddr.indexOf("@"));
		log(LOG_DEBUG, "Global address is global source address: "+saddr);
	} else if (addrs[0]) {
		saddr = addrs[0].toString();
		if (saddr.indexOf("@") > -1)
			saddr = saddr.slice(0,saddr.indexOf("@"));
		log(LOG_DEBUG, "Global address is main system address: "+saddr);
	} else {
		log(LOG_ERROR, "No FTN system address defined!");
	}
	var areas = Object.keys(tickit.acfg);
	for (var i = 0; i < areas.length; i++) {
		var cfg = tickit.acfg[areas[i]];
		if (cfg !== undefined && cfg.akamatching === true) {
			if(debug)
				log(LOG_DEBUG, areas[i].toUpperCase()+" using area AKA match.");
		} else if (cfg !== undefined && cfg.sourceaddress !== undefined) {
			var aaddr = cfg.sourceaddress.toString();
			if (aaddr.indexOf("@") > -1)
				aaddr = aaddr.slice(0,aaddr.indexOf("@"));
			if(debug)
				log(LOG_DEBUG, areas[i].toUpperCase()+" using area source address: "+aaddr);
		} else if (tickit.gcfg.akamatching) {
			if(debug)
				log(LOG_DEBUG, areas[i].toUpperCase()+" using global AKA match.");
		} else if (tickit.gcfg.sourceaddress) {
			if(debug)
				log(LOG_DEBUG, areas[i].toUpperCase()+" using global source address "+saddr);
		} else {
			if(debug)
				log(LOG_DEBUG, areas[i].toUpperCase()+" using main system address: "+saddr);
		}
	}

	for (i=0; i<sbbsecho.inb.length; i++) {
		if (tickit.gcfg.secureonly) {
			if (sbbsecho.inb[i] != sbbsecho.secure_inbound)
				continue;
		}
		if (system.platform === 'Win32')
			ticfiles = directory(sbbsecho.inb[i]+'*.tic');
		else
			ticfiles = directory(sbbsecho.inb[i]+'*.[Tt][Ii][Cc]');

		for (j=0; j<ticfiles.length; j++) {
			tic = parse_ticfile(ticfiles[j]);
			if (tic !== false) {
				if (process_tic(tic)) {
					// check if we actually were able to forward the file to the link -- wk42
					if (!forward_tic(tic))
						log(LOG_WARNING, tic.file+" has not been forwarded.");
					import_files(tic);
				} else {
					log(LOG_WARNING, "Abandoning "+tic.tic_filename);
				}
			}
		}
	}
	if(files_imported > 0)
		log(LOG_INFO, files_imported + " files imported successfully");
}

main();
