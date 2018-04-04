/*
 * An intentionally simple TIC handler for Synchronet.
 * $Id$
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

load("sbbsdefs.js");
load("fidocfg.js");
load("fido.js");

var sbbsecho = new SBBSEchoCfg();
var tickit = new TickITCfg();
var files_bbs={};
var force_replace = false;

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

/*
 * TODO: Copied from binkit.js
 */
function rename_or_move(src, dst)
{
	var sf;
	var df;
	var buf;
	var remain;

	if (file_rename(src, dst))
		return true;
	sf = new File(src);
	if (!sf.open("rb"))
		return false;
	df = new File(dst);
	if (!df.open("web")) {
		sf.close();
		return false;
	}
	while (!sf.eof) {
		// Read 2MB at a time...
		remain = sf.length - sf.position;
		if (remain === 0)
			break;
		if (remain > 0x200000)
			remain = 0x200000;
		buf = sf.read(remain);
		if (!df.write(buf)) {
			df.close();
			df.remove();
			sf.close();
			return false;
		}
	}
	df.close();
	sf.close();
	df.date = sf.date;
	sf.remove();
	return true;
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

	if (tickit.gcfg.path !== undefined)
		path = backslash(tickit.gcfg.path);
	if (tickit.gcfg.dir !== undefined)
		dir = tickit.gcfg.dir.toLowerCase();
	if (tickit.gcfg.handler !== undefined) {
		handler = tickit.gcfg.handler;
		handler_arg = tickit.gcfg.handlerarg;
	}

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
		if (rename_or_move(tic.full_path, path+tic.file))
			tic.full_path = path+tic.file;
		else {
			log(LOG_ERROR, "Cannot move '"+tic.full_path+"' to '"+path+tic.file+"'!");
			return false;
		}
		return true;
	}

	log(LOG_DEBUG, "Moving file from "+tic.full_path+" to "+path+".");
	// TODO: optionally delete replaced files even if it's not an overwrite
	if (file_exists(path+tic.file) && !force_replace) {
		if (tic.replaces === undefined || !wildmatch(tic.file, tic.replaces)) {
			log(LOG_ERROR, "'"+tic.full_path+"' already exists in '"+path+"' and TIC does not have matching Replaces line.");
			return false;
		}
		else {
			if (!do_move(path, tic))
				return false;
		}
	}
	else {
		if (!do_move(path, tic))
			return false;
	}

	if (dir !== undefined) {
		if (files_bbs[dir] === undefined)
			files_bbs[dir] = '';

		files_bbs[dir] += format("%-12s %10s ", tic.file, tic.size);
		ld = tic.ldesc.split(/\r?\n/);
		for (i=0; i<ld.length; i++) {
			if (i)
				files_bbs[dir] += " ".repeat(24);
			files_bbs[dir] += ld[i]+"\r\n";
		}
	}
	log(LOG_DEBUG, "Deleting TIC file '"+tic.tic_filename+"'.");
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
			log(LOG_DEBUG, "Node "+l[i]+" has already seen this.");
			continue;
		}
		links[l[i]]='';
	}
}

function parse_addr(addr, dz)
{
	var m;
	var ret={zone:dz, net:0, node:0, point:0};

	m = addr.match(/^([0-9]+):/);
	if (m !== null)
		ret.zone = parseInt(m[1], 10);

	m = addr.match(/([0-9]+)\//);
	if (m !== null)
		ret.net = parseInt(m[1], 10);

	m = addr.match(/\/([0-9]+)/);
	if (m !== null)
		ret.node = parseInt(m[1], 10);

	m = addr.match(/\.([0-9]+)/);
	if (m !== null)
		ret.point = parseInt(m[1], 10);

	m = addr.match(/@.+$/);
	if (m !== null)
		ret.domain = m[1];

	return ret;
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

	defzone = get_zone(system.fido_addr_list[0]);
	if (defzone === undefined) {
		log(LOG_ERROR, "Unable to detect default zone!");
		return false;
	}

	// Add us to the path...
	tic.path.push(system.fido_addr_list[0]);

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

		pw = sbbsecho.get_ticpw(link);
		if (pw===undefined)
			pw = '';

		// Figure out the outbound dir...
		addr = parse_addr(link, defzone);
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
			log(LOG_ERROR, "Unable to create TIC file for "+link+".  He will not get file '"+tic.file+"'!");
			continue;
		}
		tf.write(tic[' forward'].join("\r\n"));
		tf.write('\r\n');
		tf.write('From '+system.fido_addr_list[0]+'\r\n');
		tf.write('To '+link+'\r\n');
		tf.write('Pw '+pw+'\r\n');
		for (i=0; i<tic.path.length; i++)
			tf.write('Path '+tic.path[i]+'\r\n');
		for (i=0; i<tic.seenby.length; i++)
			tf.write('Seenby '+tic.seenby[i]+'\r\n');
		tf.close();

		// Create bsy file...
		if (addr.point > 0)
			flobase = outb+format("%08x", addr.point);
		else
			flobase = outb+format("%04x%04x", addr.net, addr.node);
		bf = new File(flobase+'.bsy');
		while (!bf.open('web+')) {
			// TODO: This waits forever...
			log(LOG_WARNING, "Waiting for BSY file '"+bf.name+"'...");
			mswait(1000);
		}

		// Append to FLO file...
		ff = new File(flobase+'.flo');
		if (!ff.open('ab+')) {
			log(LOG_ERROR, "Unable to append to '"+ff.name+"' for "+link+".  He will not get file '"+tic.file+"'!");
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
					// Circular path detection...
					for (i=0; i<system.fido_addr_list.length; i++) {
						if (val === system.fido_addr_list[i]) {
							log(LOG_ERROR, "Circular path detected on address "+val+"!");
							return false;
						}
					}
					tic[key].push(val);
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
				case 'created':
				case 'magic':
				case 'replaces':
				case 'crc':
					outtic.push(line);
					tic[key] = val;
					break;

				case 'fullname':
					outtic.push(line);
					tic.lfile = val;
					break;

				// Multi-line values
				case 'desc':
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

	if (tic.ldesc === undefined || tic.ldesc.length <= tic.desc.length)
		tic.ldesc = tic.desc;

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

	if (!tickit.gcfg.ignorepassword) {
		if (!sbbsecho.match_ticpw(tic.from, tic.pw))
			return false;
	}

	tic[' forward'] = outtic;

	return tic;
}

function import_files()
{
	var i;
	var cmd;
	var f=new File(system.temp_dir+"/tickit-files.bbs");

	for (i in files_bbs) {
		if (file_area.dir[i] === undefined) {
			log(LOG_ERROR, "Invalid directory "+i+" when importing!");
			continue;
		}

		if (!f.open("wb")) {
			log(LOG_ERROR, "Unable to create '"+f.name+"'.");
			return false;
		}
		f.write(files_bbs[i]);
		f.close();

		cmd = system.exec_dir+"addfiles "+i+" -zd +"+f.name+" 24 13";
		log(LOG_DEBUG, "Executing: '"+cmd+"'.");
		system.exec(cmd);
	}
}

function main() {
	var i, j;
	var ticfiles;
	var tic;
	var processed = 0;

	for (i in argv)
		if(argv[i] == "-force-replace")
			force_replace = true;

	for (i=0; i<sbbsecho.inb.length; i++) {
		if (tickit.gcfg.secureonly) {
			if (sbbsecho.inb[i] != sbbsecho.secure_inbound)
				continue;
		}
		if (system.platform === 'Win32')
			ticfiles = directory(sbbsecho.inb[i]+'/*.tic');
		else
			ticfiles = directory(sbbsecho.inb[i]+'/*.[Tt][Ii][Cc]');

		for (j=0; j<ticfiles.length; j++) {
			tic = parse_ticfile(ticfiles[j]);
			if (tic !== false) {
				if (process_tic(tic)) {
					processed++;

					forward_tic(tic);
				}
			}
		}
	}
	if (processed)
		import_files();
}

main();
