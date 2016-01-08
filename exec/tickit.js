load("sbbsdefs.js");

var inb=[];
var pktpass = {};
var files_bbs={};
var outbound;
var gcfg;
var acfg={};
var is_flo=false;
const cset='0123456789abcdefghijklmnopqrstuvwxyz0123456789-_';

function basefn_to_num(num)
{
	var base = cset.length;
	var part;
	var ret=0;
	var i;

	for (i=0; i<num.length; i++) {
		ret *= base;
		ret += cset.indexOf(num[i]);
	}
	return ret;
	
}

function num_to_basefn(num)
{
	var base = cset.length;
	var part;
	var ret='';

	while(num) {
		part = num % base;
		ret = cset.charAt(part) + ret;
		num = parseInt((num/base), 10);
	}
	return ret;
}

/*
 * Returns a filename in the format "ti_XXXXX.tic" where
 * XXXXX is replaced by an incrementing base-48 number.
 * This provides 254,803,967 unique filenames.
 */
function get_next_tic_filename()
{
	var f;
	var val;
	var ret;

	// Get previous value or -1 if there is no previous value.
	f = new File(system.data_dir+'tickit.seq');

	if (f.open("r+b")) {
		val = f.readBin();
	}
	else {
		if (!f.open("web+")) {
			log(LOG_ERROR, "Unable to open file "+f.name+"!");
			return undefined;
		}
		val = -1;
	}

	// Increment by 1...
	val++;

	// Check for wrap...
	if (val > basefn_to_num('_____'))
		val = 0;

	// Write back new value.
	f.truncate();
	f.writeBin(val);
	f.close();

	// Left-pad to five digits.
	ret = ('00000'+num_to_basefn(val)).substr(-5);

	// Add pre/suf-fix
	return 'ti_'+ret+'.tic';
}

function get_pw(node)
{
	var n = node;

	while(n) {
		if (pktpass[n] !== undefined)
			return pktpass[n];
		if (n === 'ALL')
			break;
		if (n.indexOf('ALL') !== -1)
			n = n.replace(/[0-9]+[^0-9]ALL$/, 'ALL');
		else
			n = n.replace(/[0-9]+$/, 'ALL');
	}
	return undefined;
}

function match_pw(node, pw)
{
	var pktpw = get_pw(node);

	if (pktpw === undefined || pktpw == '') {
		if (pw === '' || pw === undefined)
			return true;
	}
	if (pw.toUpperCase() === pktpw.toUpperCase())
		return true;

	log(LOG_WARNING, "Incorrect password "+pw+" (expected "+pktpw+")");
	return false;
}

function process_tic(tic)
{
	var dir;
	var path;
	var ld;
	var i;
	var cfg;

	if (gcfg.path !== undefined)
		path = backslash(gcfg.path);
	if (gcfg.dir !== undefined)
		dir = gcfg.dir.toLowerCase();

	cfg = acfg[tic.area.toLowerCase()];
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

	log(LOG_DEBUG, "Moving file from "+tic.full_path+" to "+path+".");
	if (file_rename(tic.full_path, path+tic.file))
		tic.full_path = path+tic.file;
	else {
		log(LOG_ERROR, "Cannot move '"+tic.full_path+"' to '"+path+tic.file+"'!");
		return false;
	}

	if (dir !== undefined) {
		if (files_bbs[dir] === undefined)
			files_bbs[dir] = '';

		files_bbs[dir] += format("%-11s %10s ", tic.file, tic.size);
		if (tic.ldesc === undefined || tic.ldesc.length <= tic.desc)
			files_bbs[dir] += tic.desc + "\r\n";
		else {
			ld = tic.ldesc.split(/\r?\n/);
			for (i=0; i<ld.length; i++) {
				if (i)
					files_bbs[dir] += "                        ";
				files_bbs[dir] += ld[i]+"\r\n";
			}
		}
	}
	log(LOG_DEBUG, "Deleting TIC file '"+tic.tic_filename+"'.");
	file_remove(tic.tic_filename);

	return true;
}

function add_links(seenbys, list)
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

function parse_addr(addr)
{
	var m;
	var ret={};

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
	tic.path.push(system.system.fido_addr_list[0]);

	// Populate seenbys from TIC file
	for (i=0; i<tic.seenby.length; i++)
		seenbys[tic.seenby[i]]='';

	// Calculate links
	if (gcfg.links !== undefined)
		add_links(seenbys, gcfg.links);
	cfg = acfg[tic.area.toLowerCase()];
	if (cfg !== undefined && cfg.links !== undefined)
		add_links(seenbys, cfg.links);

	// Add links to seenbys
	for (i in links)
		tic.seenby.push(i);

	// Now, start generating the TIC/FLO files...
	for (link in links) {
		if (!is_flo) {
			log(LOG_ERROR, "TickIT doesn't support non-FLO mailers.");
			return false;
		}

		pw = get_pw(link);
		if (pw===undefined)
			pw = '';

		// Figure out the outbound dir...
		addr = parse_addr(link);
		if (addr.zone === undefined)
			addr.zone = defzone;

		if (addr.zone === undefined || addr.net === undefined || addr.node === undefined) {
			log(LOG_ERROR, "Address '"+link+"' is invalid!");
			continue;
		}

		outb = outbound.replace(/[\\\/]+$/g, '');;
		if (addr.zone !== defzone)
			outb += format(".%03x", addr.zone);
		outb = file_getcase(outb);
		backslash(outb);

		// Create TIC file first...
		tf = new File(outb+get_next_tic_filename());
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
			tf.write('Path '+tic.seenby[i]+'\r\n');
		tf.close;

		// Create bsy file...
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
		bg.remove();
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
	var tic={seenby:[], path:[], tic_filename:fname};
	var outtic=[];
	var f=new File(fname);
	var dir = fullpath(fname).replace(/([\/\\])[^\/\\]*$/,'$1');
	var i;

	log(LOG_INFO, "Parsing "+fname);
	if (!f.open("r")) {
		log(LOG_ERROR, "Unable to open '"+f.name+"'");
		return false;
	}
	while (line = f.readln(65535)) {
		m = line.match(/^\s*([^\s]+)\s+(.*)$/);
		if (m !== null) {
			key = m[1].toLowerCase();
			val = m[2];

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
				case 'desc':
				case 'created':
				case 'magic':
				case 'replaces':
				case 'crc':
					outtic.push(line);
					tic[key] = val;
					break;

				case 'filename':
					outtic.push(line);
					tic[lfile] = val;
					break;

				// Multi-line values
				case 'ldesc':
					outtic.push(line);
					tic[key] += val+"\r\n"
					break;

				default:
					outtic.push(line);
					log(LOG_WARNING, "Unhandled keyword '"+key+"'");
					break;
			}
		}
	}

	if (tic.desc.length > 56) {
		if (tic.ldesc === undefined || tic.ldesc.length <= tic.desc.length)
			tic.ldesc = word_wrap(tic.desc, 56, 65535, false);
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
	if (!match_pw(tic.from, tic.pw))
		return false;

	tic[' forward'] = outtic;

	return tic;
}

function parse_ecfg()
{
	var ecfg = new File(system.ctrl_dir+'sbbsecho.cfg');
	var line;
	var m;

	if (!ecfg.open("r")) {
		log(LOG_ERROR, "Unable to open '"+ecfg.name+"'");
		return false;
	}
	while (line=ecfg.readln(65535)) {
		m = line.match(/^\s*(?:secure_)?inbound\s+(.*)$/i);
		if (m !== null) {
			inb.push(backslash(m[1]));
		}
		m = line.match(/^\s*pktpwd\s+(.*?)\s+(.*)\s*$/i);
		if (m !== null) {
			pktpass[m[1].toUpperCase()] = m[2].toUpperCase();
		}
		m = line.match(/^\s*outbound\s+(.*?)\s*$/i);
		if (m !== null) {
			outbound = m[1];
		}
		m = line.match(/^\s*flo_mailer\s*$/i);
		if (m !== null) {
			is_flo = true;
		}
	}
	ecfg.close();
	return true;
}

function lcprops(obj)
{
	var i;

	for (i in obj) {
		if (i.toLowerCase() !== i) {
			if (obj[i.toLowerCase()] === undefined)
				obj[i.toLowerCase()] = obj[i];
			if (typeof(obj[i]) == 'Object')
				lcprops(obj[i]);
		}
	}
}

function parse_tcfg()
{
	var tcfg = new File(system.ctrl_dir+'tickit.ini');
	var sects;
	var i;

	if (!tcfg.open("r")) {
		log(LOG_ERROR, "Unable to open '"+tcfg.name+"'");
		tcfg.close();
		return false;
	}
	gcfg = tcfg.iniGetObject();
	lcprops(gcfg);
	sects = tcfg.iniGetSections();
	for (i=0; i<sects.length; i++) {
		acfg[sects[i].toLowerCase()] = tcfg.iniGetObject(sects[i]);
		lcprops(acfg[sects[i].toLowerCase()]);
	}
	tcfg.close();

	return true;
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

		cmd = system.exec_dir+"addfiles "+i+" -zd +"+f.name+" 12 23";
		log(LOG_DEBUG, "Executing: '"+cmd+"'.");
		system.exec(cmd);
	}
}

function main() {
	var i, j;
	var ticfiles;
	var tic;
	var processed = 0;

	if (!parse_ecfg())
		return false;
	if (!parse_tcfg())
		return false;

	for (i=0; i<inb.length; i++) {
		if (system.platform === 'Win32')
			ticfiles = directory(inb[i]+'/*.tic');
		else
			ticfiles = directory(inb[i]+'/*.[Tt][Ii][Cc]');

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
