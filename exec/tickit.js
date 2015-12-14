var i,j;
var inb=[];
var line;
var m;
var ecfg;
var pktpass = {};
var ticfiles;

function match_pw(node, pw)
{
	var n = node;
	while(n) {
		if (pktpass[n] !== undefined) {
			if (pw.toUpperCase() === pktpass[n])
				return true;
			log(LOG_WARNING, "Incorrect password "+pw+" (expected "+pktpass[n]+")");
			return false;
		}
		if (n === 'ALL')
			return false;
		n = n.replace(/[0-9]+[^0-9](?:ALL|[0-9]+)?$/, 'ALL');
	}
	log(LOG_WARNING, "No PKTPWD found to match "+node+".");
	return false;
}

function parse_ticfile(fname)
{
	var crc32;
	var key;
	var val;
	var m;
	var line;
	var tic={seenby:[], path:[]};
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
				// Singlt value, single line...
				case 'area':
				case 'areadesc':
				case 'origin':
				case 'from':
				case 'to':
				case 'file':
				case 'lfile':
				case 'filename':
				case 'size':
				case 'date':
				case 'desc':
				case 'created':
				case 'magic':
				case 'replaces':
				case 'crc':
				case 'pw':
					tic[key] = val;
					break;

				// Multi-line values
				case 'ldesc':
					tic[key] += val+"\r\n"
					break;

				// Multiple values
				case 'path':
				case 'seenby':
					tic[key].push(val);
					break;

				default:
					log(LOG_WARNING, "Unhandled keyword '"+key+"'");
					break;
			}
		}
	}
	f.close();
	f = new File(dir+tic.file);
	if (!f.exists) {
		log(LOG_WARNING, "File '"+f.name+"' doesn't exist.");
		return false;
	}
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

	// TODO: Handle the thing.
	// Create directory, add file... etc.
	return true;
}

// First, we need to parse sbbsecho.cfg to get the paths and passwords

ecfg = new File(system.ctrl_dir+'sbbsecho.cfg');
if (!ecfg.open("r")) {
	log(LOG_ERROR, "Unable to open '"+ecfg.name+"'");
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

// Now, we look in the inbound directories for TIC files...
for (i=0; i<inb.length; i++) {
	if (system.platform === 'Win32')
		ticfiles = directory(inb[i]+'/*.tic');
	else
		ticfiles = directory(inb[i]+'/*.[Tt][Ii][Cc]');

	for (j=0; j<ticfiles.length; j++) {
		parse_ticfile(ticfiles[j]);
	}
}
