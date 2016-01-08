load("sbbsdefs.js");

var inb=[];
var pktpass = {};
var files_bbs={};
var gcfg;
var acfg={};

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
	file_rename(tic.full_path, path+tic.file);
	log(LOG_DEBUG, "Deleting TIC file '"+tic.tic_filename+"'.");
	file_remove(tic.tic_filename);
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
				case 'path':
				case 'seenby':
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
				if (process_tic(tic))
					processed++;
			}
		}
	}
	if (processed)
		import_files();
}

main();
