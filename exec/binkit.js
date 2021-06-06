// vi: tabstop=8 softtabstop=8 shiftwidth=8 noexpandtab
/*
 * Intentionally simple "Advanced BinkleyTerm Style Outbound"
 * mailer.
 *
 * Known limitations:
 * 1) Does NOT support upper-case filenames in derived files/paths.
 *    This means that all .flo files must be in lower-case, all hex
 *    filename/path components must be in lower-case, and the ".pnt"
 *    directory extension must be in lower case.
 * 2) Will not check in a zone-specified directory for the default
 *    zone.  That is, if the default zone is zone 1, and the outbound
 *    is "/path/to/outbound", it will not correctly handle the case
 *    where there is a "/path/to/outbound.001" directory.
 *
 * See FTS-5005 for details.
 */

load('fido.js');
load('fidocfg.js');
load('binkp.js');
load('freqit_common.js');

var REVISION = 2.41;
var version_notice = "BinkIT/" + REVISION;
var semaphores = [];
// data/binkstats.ini
var stats = { inbound: { true: {}, false: {} }, callout: { true: {}, false: {} }, totals: {} };
var cfgfname;

function update_stats(stats, addr, bp, host)
{
	stats[addr] = {};
	if(bp.remote_operator)
		stats[addr].oper = bp.remote_operator;
	if(bp.remote_addrs)
		stats[addr].AKAs = bp.remote_addrs;
	if(bp.remote_capabilities)
		stats[addr].caps = bp.remote_capabilities;
	if(bp.remote_ver)
		stats[addr].vers = bp.remote_ver;
	if(bp.connect_host)
		stats[addr].host = bp.connect_host;
	else if(host)
		stats[addr].host = host;
	if(bp.connect_port)
		stats[addr].port = bp.connect_port;
	if(bp.connect_error !== undefined)
		stats[addr].connect_error = bp.connect_error;
	for(var i in bp.remote_info)
		stats[addr]['info.' + i.toLowerCase()] = bp.remote_info[i];

	stats[addr].localtime = new Date();
	if(bp.sent_files.length)
		stats[addr].sent_files = bp.sent_files;
	if(bp.failed_sent_files.length)
		stats[addr].failed_sent_files = bp.failed_sent_files.map(function(i) { return i.path; });
	if(bp.received_files.length)
		stats[addr].received_files = bp.received_files;
	if(bp.failed_received_files.length)
		stats[addr].failed_received_files = bp.failed_received_files;
}

function update_totals(stats, addr, bp, callout, success)
{
	if(!stats[addr])
		stats[addr] = {};
	var counter = format("%s_%s", success ? "successful" : "failed", callout ? "callouts":"inbounds");
	if(!stats[addr][counter])
		stats[addr][counter] = 0;
	stats[addr][counter]++;
	if(!stats[addr].sent_files)
		stats[addr].sent_files = 0;
	stats[addr].sent_files += bp.sent_files.length;
	if(!stats[addr].received_files)
		stats[addr].received_files = 0;
	stats[addr].received_files += bp.received_files.length;
	if(!stats[addr].failed_sent_files)
		stats[addr].failed_sent_files = 0;
	stats[addr].failed_sent_files += bp.failed_sent_files.length;
	if(!stats[addr].failed_received_files)
		stats[addr].failed_received_files = 0;
	stats[addr].failed_received_files += bp.failed_received_files.length;
}

FREQIT.add_file = function(filename, bp, cfg)
{
	if (filename === undefined)
		return;
	if (FREQIT.files >= cfg.maxfiles)
		return;
	if (FREQIT.added[filename] !== undefined)
		return;
	bp.addFile(filename);
	FREQIT.files++;
	FREQIT.added[filename]='';
};

function lock_flow(file)
{
	var ret = {
		bsy:new File(file.replace(/\.[^\.]*?$/, '.bsy')),
	};

	// Takes ownership of a lockfile if it's more than six hours old.
	/*
	 * Race-safe version...
	 * 1) If date is "too old", pick a random time in the future.
	 * 2) Set the file date to that random time.
	 * 3) Wait one second (there's still an unlikely race here)
	 * 4) If the file date is the random time you chose, set the date
	 *    to now and take "ownership" of the file.
	 */
	function take_lockfile(f)
	{
		var orig_date;
		var future;
		var remain;
		var now = time();

		if (!f.exists)
			return false;
		// TODO: This is hacked for a signed 32-bit time_t... watch out in 2038!
		orig_date = f.date;
		if (orig_date > (now - 60*60*6)) {
			log(LOG_INFO, format("Lock is too recent to override now (%ld) > six hours ago (%ld)", orig_date, now-60*60*6));
			return false;
		}
		remain = 0x80000000 - now;
		future = now + random(remain);
		f.date = future;
		mswait(1000);
		if (f.date != future) {
			log(LOG_WARNING, format("Failed to set date in the future fdate (%ld) != future (%ld)", f.date, future));
			return false;
		}
		if (!f.open("wb")) {
			f.date = orig_date;
			log(LOG_WARNING, "Error " + f.error + " opening lock file: " + f.name);
			return false;
		}
		f.date = now;
		return true;
	}
	log(LOG_DEBUG, "Locking "+ret.bsy.name);
	if(!mkpath(ret.bsy.name.slice(0, -file_getname(ret.bsy.name).length)))
		log(LOG_WARNING, "MKPATH ERROR " + errno + " (" + errno_str + "): " + ret.bsy.name);
	if (!ret.bsy.open("wb")) {	// Used to include 'e' mode flag (which never worked)
		log(LOG_WARNING, "Error " + ret.bsy.error + " creating lock file: " + ret.bsy.name);
		if (!take_lockfile(ret.bsy)) {
			log(LOG_NOTICE, "Lock on "+ret.bsy.name+" failed.");
			return undefined;
		}
	}
	ret.bsy.writeln(version_notice + " args: " + argv.join(' '));
	ret.bsy.flush();
	log(LOG_DEBUG, "Lock successful.");
	return ret;
}

function unlock_flow(locks)
{
	log(LOG_DEBUG, "Unlocking "+locks.bsy.name+".");
	if (locks.bsy !== undefined) {
		locks.bsy.close();
		locks.bsy.remove();
	}
}

function outbound_root(addr, scfg)
{
	if (FIDO.FTNDomains.outboundMap[addr.domain] === undefined)
		return fullpath(scfg.outbound.replace(/[\\\/]$/, ''));
	return fullpath(FIDO.FTNDomains.outboundMap[addr.domain]);
}

/*
 * Given a list of addresses to rescan, calls
 * bp.addFile() for any pending file transfers.
 *
 * TODO: Call this after sending a M_EOB to rescan per FSP-1024?  We
 * 		 hold the lock files, so nothing should be changing the flow
 * 		 files (per FTS-5005) though.  This is mostly for REQ handling,
 * 		 so if we do integrate freqit.js, we should be fine to ignore
 * 		 the spec on this point.
 */
function add_outbound_files(addrs, bp)
{
	function has_lock(addr) {
		var bsy = outbound_root(addr, bp.cb_data.binkit_scfg)+addr.flo_outbound(bp.default_zone, bp.default_domain)+'bsy';
		var i;

		for (i=0; i<bp.cb_data.binkit_locks.length; i++) {
			if (bp.cb_data.binkit_locks[i].bsy.name === bsy)
				return true;
		}
		return false;
	}

	addrs.forEach(function(addr) {
		var lock_files;

		log(LOG_DEBUG, "Adding outbound files for "+addr + " (" + typeof(addr) + ")");
		// Find all possible flow files for the remote.
		var allfiles = directory(outbound_root(addr, bp.cb_data.binkit_scfg)+addr.flo_outbound(bp.default_zone, bp.default_domain)+'*');
		// Parse flow files and call addFile() tracking what to do on success.
		if (allfiles.length > 0) {
			if (!has_lock(addr)) {
				lock_files = lock_flow(outbound_root(addr, bp.cb_data.binkit_scfg)+addr.flo_outbound(bp.default_zone, bp.default_domain));
				if (lock_files === undefined)
					return;
				bp.cb_data.binkit_locks.push(lock_files);
			}
			allfiles.forEach(function(file) {
				var flo;
				var line;
				var action;
				var i;
				var fnchars = '0123456789abcdefghijklmnopqrstuvwxyz';
				var fname;

				if(file_isdir(file) || !file_exists(file))
					return;
				var ext = file_getext(file);
				if (ext !== undefined)
					ext = ext.toLowerCase();

				switch(ext) {
					case '.clo':
					case '.dlo':
					case '.flo':
					case '.hlo':
					case '.ilo':
						flo = new File(file);
						if (!flo.open("r")) {
							log(LOG_ERROR, "Error " + flo.error + " opening FLO file: " + flo.name);
							break;
						}
						if (bp.cb_data.binkit_flow_contents[flo.name] === undefined)
							bp.cb_data.binkit_flow_contents[flo.name] = [];
						while((line = flo.readln(2048))) {
							switch(line.charAt(0)) {
								case '#':
									if (bp.addFile(line.substr(1)))
										bp.cb_data.binkit_file_actions[line.substr(1)] = 'TRUNCATE';
									bp.cb_data.binkit_flow_contents[flo.name].push(line.substr(1));
									break;
								case '^':
								case '-':
									if (bp.addFile(line.substr(1)))
										bp.cb_data.binkit_file_actions[line.substr(1)] = 'DELETE';
									bp.cb_data.binkit_flow_contents[flo.name].push(line.substr(1));
									break;
								case '~':
								case '!':
									break;
								case '@':
									bp.addFile(line.substr(1));
									bp.cb_data.binkit_flow_contents[flo.name].push(line.substr(1));
									break;
								default:
									bp.addFile(line);
									bp.cb_data.binkit_flow_contents[flo.name].push(line);
									break;
							}
						}
						flo.close();
						break;
					case '.cut':
					case '.dut':
					case '.hut':
					case '.iut':
					case '.out':
						fname = '';
						for (i=0; i<8; i++)
							fname += fnchars[random(fnchars.length)];
						fname += '.pkt';
						if (bp.addFile(file, fname))
							bp.cb_data.binkit_file_actions[file] = 'DELETE';
						break;
					case '.req':
						fname = '';
						for (i=0; i<8; i++)
							fname += fnchars[random(fnchars.length)];
						fname += '.req';
						if (bp.addFile(file, fname))
							bp.cb_data.binkit_file_actions[file] = 'DELETE';
						break;
					case '.bsy':
						break;
					default:
						log(LOG_WARNING, "Unsupported flow file type '"+file+"'.");
						break;
				}
			});
		}
		if(bp.cb_data.binkitcfg.node[addr] === undefined
			|| !bp.cb_data.binkitcfg.node[addr].outbox)
			return;
		var boxfiles = directory(backslash(bp.cb_data.binkitcfg.node[addr].outbox) + '*');
		for(var f in boxfiles) {
			var fname = boxfiles[f];
			log("outbox file: " + fname);
			if (bp.addFile(fname))
				bp.cb_data.binkit_file_actions[fname] = 'DELETE';
		}
	});
}

function callout_auth_cb(mode, bp)
{
	/*
	 * Loop through remote addresses, building a list of the ones with
	 * the same password (if we used an empty password, no other nodes
	 * are allowed)
	 */
	var addrs = [];

	if (!bp.cb_data.binkitpw || bp.cb_data.binkitpw === '-')
		addrs.push(bp.cb_data.binkit_to_addr);
	else if(bp.remote_addrs !== undefined) {
		bp.remote_addrs.forEach(function(addr) {
			if (bp.cb_data.binkitcfg.node[addr] !== undefined) {
				if (bp.cb_data.binkitcfg.node[addr].pass === bp.cb_data.binkitpw)
					addrs.push(addr);
			}
			else
				log(LOG_DEBUG, "Unconfigured address "+addr);
		});
	}

	add_outbound_files(addrs, bp);
}

function remove_file(fname)
{
	if (file_remove(fname))
		log(LOG_INFO, "Deleted file: " + fname);
	else if(file_exists(fname))
		log(LOG_ERROR, "Error " + errno + " (" + errno_str + ") deleting file: " + fname);
}


/*
 * Delete completed flo files.
 */
function tx_callback(fname, bp)
{
	var j;

	// Remove flow files that have been completely processed.
	Object.keys(bp.cb_data.binkit_flow_contents).forEach(function(flo) {
		if (file_exists(flo)) {
			while ((j = bp.cb_data.binkit_flow_contents[flo].indexOf(fname)) !== -1)
				bp.cb_data.binkit_flow_contents[flo].splice(j, 1);
			if (bp.cb_data.binkit_flow_contents[flo].length == 0)
				remove_file(flo);
		}
	});
}

function handle_freq(reqfname, bp)
{
	var req=new File(reqfname);
	var m;
	var fname;
	var pw;
	var cfg = new FREQITCfg();

	if (!req.open("r"))
		return;
	FREQIT.reset();

	next_file:
	while((fname=req.readln())) {
		if ((m=fname.match(/^(.*) !(.*?)$/))!==null) {
			pw=m[2];
			fname=m[1];
		}
		// First, check for magic!
		for (m in cfg.magic) {
			if (m == fname.toLowerCase()) {
				FREQIT.handle_magic(cfg.magic[m], bp, bp.authenticated === 'secure', pw, cfg);
				continue next_file;
			}
		}

		// Now, check for the file...
		FREQIT.handle_regular(fname, bp, bp.authenticated === 'secure', pw, cfg);
	}
	req.close();
}

function rename_or_move(src, dst_dir, dst_fname)
{
	var sf;
	var df;
	var buf;
	var remain;

	if (!mkpath(dst_dir)) {
		log(LOG_ERR, "Error " + errno + " making directory: " + dst_dir);
		return false;
	}
	var dst = dst_dir + dst_fname;
	if (file_rename(src, dst))
		return true;
	sf = new File(src);
	if (!sf.open("rb")) {
		log(LOG_ERR, "Error " + sf.error + " opening source file for move: " + sf.name);
		return false;
	}
	df = new File(dst);
	if (!df.open("wb")) {		// Used to include 'e' mode flag (which never worked)
		log(LOG_ERR, "Error " + df.error + " opening destination file for move: " + df.name);
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
			log(LOG_ERR, "Error " + df.error + " writing to " + df.name);
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

function rx_callback(fname, bp)
{
	var semname;
	var secure_inbound = bp.cb_data.binkit_scfg.secure_inbound;
	var inbound = bp.cb_data.binkit_scfg.inbound;
	if (secure_inbound == undefined)
		secure_inbound = inbound;

	log(LOG_INFO, "Received file: " + fname + format(" (%1.1fKB)", file_size(fname) / 1024.0));
	if (fname.search(/\.(?:pkt|su.|mo.|tu.|we.|th.|fr.|sa.)$/i) !== -1) {
		semname = system.data_dir + 'fidoin.now';
		if (semaphores.indexOf(semname) == -1)
			semaphores.push(semname);
	}
	else if (fname.search(/\.tic$/i) !== -1) {
		semname = system.data_dir + 'tickit.now';
		if (semaphores.indexOf(semname) == -1)
			semaphores.push(semname);
	}

	if (fname.search(/\.req$/i) !== -1) {
		handle_freq(fname, bp);
		remove_file(fname);
	}
	else {
		if (bp.authenticated === 'secure') {
			if (secure_inbound === undefined)
				log(LOG_ERROR, "No secure inbound configured in sbbsecho!  Leaving secure file as '"+fname+"'.");
			else {
				log(LOG_INFO, "Moving '"+fname+"' to '"+secure_inbound+file_getname(fname)+"'.");
				if (!rename_or_move(fname, secure_inbound, file_getname(fname)))
					return false;
			}
		}
		else {
			log(LOG_WARNING, "Non-secure session type: " + format("'%s'", bp.authenticated));
			if (inbound === undefined)
				log(LOG_ERROR, "No inbound configured in sbbsecho!  Leaving insecure file as '"+fname+"'.");
			else {
				log(LOG_INFO, "Moving '"+fname+"' to '"+inbound+file_getname(fname)+"'.");
				if (!rename_or_move(fname, inbound, file_getname(fname)))
					return false;
			}
		}
	}
	return true;
}

function callout_want_callback(fobj, fsize, fdate, offset, bp)
{
	/*
	 * TODO: Currently a copy/paste from binkp.js...
	 * Likely we'll want magical handling for control files (*.TIC,
	 * *.REQ, and *.PKT, Bundle Files, and the default handling for the
	 * rest.
	 *
	 * We should lower-case incoming filenames here too...
	 *
	 * Also, for partial files, we can cancel the remote and do an M_GET resume.
	 *
	 * Ok, so put active transfers into a temp directory somewhere with some way
	 * of tying it back to a set of address (we don't actually know which address it
	 * comes from, just a list of addresses it may come from) and store enough
	 * info to resume it.  This means the partially transferred file and some sort
	 * of bookkeeping info file need to be tied together somewhere... maybe resume
	 * support is simply "too hard" to bother with?
	 *
	 * Once the file is completely received, move it to the final resting place
	 * (in rx_callback) and do any required processing then.
	 */

	// Reject duplicate filenames... a more robust callback would rename them.
	// Or process the old ones first.
	if (this.received_files.indexOf(fobj.name) != -1)
		return this.file.REJECT;
	// Reject or skip existing files.
	if (file_size(fobj.name) > 0) {
		log(LOG_WARNING, "Inbound file already exists: " + fobj.name);
		// If the size and date are the same, reject it.
		if (fsize == file_size(fobj.name) && fdate == file_date(fobj.name))
			return this.file.REJECT;
		// Otherwise, skip it.
		return this.file.SKIP;
	}
	// Accept everything else
	return this.file.ACCEPT;
}

function callout_done(bp)
{
	var f;
	var lines;
	var semname;

	bp.sent_files.forEach(function(file) {
		if (bp.cb_data.binkit_file_actions[file] !== undefined) {
			switch(bp.cb_data.binkit_file_actions[file]) {
				case 'TRUNCATE':
					f = new File(file);
					if (f.truncate())
						log(LOG_INFO, "Truncated '"+f.name+"'.");
					else
						log(LOG_ERROR, "Unable to truncate '"+f.name+"'.");
					break;
				case 'DELETE':
					remove_file(file);
					break;
			}
		}
	});

	Object.keys(bp.cb_data.binkit_flow_contents).forEach(function(key) {
		if (bp.cb_data.binkit_flow_contents[key].length > 0) {
			// We have some unsent files in here... re-write the flo file...
			f = new File(key);
			if (!f.open("r+")) {
				log(LOG_ERROR, "Error " + f.error + " opening flow file for update: " + key);
				return;
			}
			lines = f.readAll(2048);
			f.truncate(0);
			lines.forEach(function(line) {
				switch(line[0]) {
					case '#':
					case '^':
					case '-':
					case '@':
						if (bp.cb_data.binkit_flow_contents[key].indexOf(line.substr(1)) == -1)
							f.writeln('~'+line.substr(1));
						else
							f.writeln(line);
						break;
					// Already skipped...
					case '~':
					case '!':
						f.writeln(line);
						break;
					default:
						if (bp.cb_data.binkit_flow_contents[key].indexOf(line) == -1)
							f.writeln('~'+line);
						else
							f.writeln(line);
						break;
				}
			});
			f.close();
		}
	});

	// Remove flow files that have been completely processed.
	Object.keys(bp.cb_data.binkit_flow_contents).forEach(function(flo) {
		if (file_exists(flo)) {
			if (bp.cb_data.binkit_flow_contents[flo].length == 0)
				remove_file(flo);
		}
	});
}

function callout(addr, scfg, locks, bicfg)
{
	var myaddr = FIDO.parse_addr(system.fido_addr_list[0], 1, 'fidonet');
	var bp = new BinkP(version_notice, undefined, rx_callback, tx_callback);
	var port;
	var host;
	var tls = false;
	var f;
	var success = false;
	var src_addr;

	log(LOG_INFO, format("%s callout to %s started", bp.revision, addr));
	if (bicfg === undefined)
		bicfg = new BinkITCfg(cfgfname);
	bp.system_operator = bicfg.sysop;
	bp.plain_auth_only = bicfg.plain_auth_only;
	bp.crypt_support = bicfg.crypt_support;
	bp.cb_data = {
		binkitcfg:bicfg,
		binkit_to_addr:FIDO.parse_addr(addr, myaddr.zone, myaddr.domain),
		binkit_scfg:scfg,
		binkit_file_actions:{},
		binkit_flow_contents:{},
		binkit_locks:locks
	};
	if (bp.cb_data.binkitcfg.node[addr] !== undefined) {
		bp.cb_data.binkitpw = bp.cb_data.binkitcfg.node[addr].pass;
		port = bp.cb_data.binkitcfg.node[addr].port;
		host = bp.cb_data.binkitcfg.node[addr].host;
		tls = bp.cb_data.binkitcfg.node[addr].tls;
		if (bp.plain_auth_only) {
			bp.require_md5 = false;
			bp.require_crypt = false;
		} else {
			bp.require_md5 = !(bp.cb_data.binkitcfg.node[addr].nomd5);
			bp.require_crypt = bp.crypt_support && !(bp.cb_data.binkitcfg.node[addr].nocrypt);
			bp.plain_auth_only = bp.cb_data.binkitcfg.node[addr].plain_auth_only;
		}
	}
	// TODO: Force debug mode for now...
	bp.debug = true;
	bp.default_zone = myaddr.zone;
	bp.default_domain = myaddr.domain;
	bp.want_callback = callout_want_callback;
	if (bp.cb_data.binkitcfg.caps !== undefined)
		bp.capabilities = bp.cb_data.binkitcfg.caps;

	/*
	 * We can't use the defaults since the defaults are only 4D addresses,
	 * and the address we're calling from needs to be first...
	 */
	bp.addr_list = [];
	system.fido_addr_list.forEach(function(faddr){
		bp.addr_list.push(FIDO.parse_addr(faddr, this.default_zone));
	}, this);
	/*
	 * Set src_addr to the node we want first in our list.  If
	 * SourceAddress is set for this node, use that.  Otherwise, use the
	 * address we are contacting.
	 */
	if (bp.cb_data.binkitcfg.node[addr] !== undefined && bp.cb_data.binkitcfg.node[addr].src !== undefined)
		src_addr = FIDO.parse_addr(bp.cb_data.binkitcfg.node[addr].src, 1);
	else
		src_addr = addr;
	// Sort by "distance" from src_addr address.
	FIDO.distance_sort(bp.addr_list, src_addr);

	log(LOG_DEBUG, format("connecting to %s at %s", addr, host));
	// We won't add files until the auth finishes...
	success = bp.connect(addr, bp.cb_data.binkitpw, callout_auth_cb, port, host, tls);
	// Statistics
	update_stats(stats.callout[success], addr, bp, host);
	update_totals(stats.totals, addr, bp, true, success);
	callout_done(bp);
}

function check_held(addr, scfg, myaddr)
{
	var until;
	var f = new File(outbound_root(addr, scfg)+addr.flo_outbound(myaddr.zone)+'.hld');

	if (!f.exists)
		return false;
	if (!f.open("r")) {
		log(LOG_ERROR, "Error " + f.error + " opening hold file: " + f.name);
		return true;
	}
	until = f.readln();
	if (until.search(/^[0-9]+$/) !== 0) {
		log(LOG_WARNING, "First line of '"+f.name+"' invalid ("+until+").  Should be a positive integer.");
		return false;
	}
	f.close();
	until = parseInt(until, 10);
	if (until < time()) {
		f.remove();
		log(LOG_INFO, "Removed stale ("+system.timestr(until)+") hold file '"+f.name+"'.");
		return false;
	}
	log(LOG_INFO, addr+" held until "+system.timestr(until)+".");
	return true;
}

function run_one_outbound_dir(dir, scfg, ran)
{
	var myaddr = FIDO.parse_addr(system.fido_addr_list[0], 1, 'fidonet');
	var locks = [];
	var addr;
	var lock_files;
	var ext;
	var i;
	var flow_files;

	log(LOG_DEBUG, "Running outbound dir "+dir);

	flow_files = directory(dir+'*.?ut').concat(directory(dir+'*.?lo'));

	function flow_order(a, b) {
		var flavour_order=['i','c','d','o','f','h'];
		var type_order=['ut', 'lo'];

		var aext = flavour_order.indexOf(a.substr(-3, 1).toLowerCase());
		var bext = flavour_order.indexOf(b.substr(-3, 1).toLowerCase());

		if (aext !== bext)
			return aext - bext;

		aext = type_order.indexOf(a.substr(-2, 2).toLowerCase());
		bext = type_order.indexOf(b.substr(-2, 2).toLowerCase());

		if (aext !== bext)
			return aext - bext;

		if (a < b)
			return -1;
		if (b < a)
			return 1;
		return 0;
	}

	while (!js.terminated) {
		if (flow_files.length == 0)
			break;
		flow_files.sort(flow_order);
		flow_file_loop:
		for (i=0; i<flow_files.length; i++) {
			try {
				addr = FIDO.parse_flo_file_path(flow_files[i], myaddr.zone);
			}
			catch(addr_e) {
				log(LOG_WARNING, addr_e+" when checking '"+flow_files[i]+"' (default zone: "+myaddr.zone+")");
				continue;
			}
			if (ran[addr] !== undefined)
				continue;
			ext = file_getext(flow_files[i]);
			if (ext === undefined) {
				log(LOG_WARNING, "Unknown flow file flavour '"+flow_files[i]+"'.");
				continue;
			}

			// Ensure this is the "right" outbound (file case, etc)
			if (flow_files[i] !== outbound_root(addr, scfg)+addr.flo_outbound(myaddr.zone)+ext.substr(1)) {
				log(LOG_WARNING, "Unexpected file path '"+flow_files[i]+"' expected '"+outbound_root(addr, scfg)+addr.flo_outbound(myaddr.zone)+ext.substr(1)+"' (skipped)");
				continue;
			}

			switch(ext.substr(0, 2)) {
				case '.h':
					log(LOG_DEBUG, "Skipping hold flavoured flow file '"+flow_files[i]+"'.");
					continue;
				case '.c':
				case '.d':
				case '.i':
					break;
				case '.f':
					if (ext === '.flo')
						break;
					log(LOG_WARNING, "Unknown flow file flavour '"+flow_files[i]+"'.");
					continue;
				case '.o':
					if (ext === '.out')
						break;
					log(LOG_WARNING, "Unknown flow file flavour '"+flow_files[i]+"'.");
					continue;
				default:
					log(LOG_WARNING, "Unknown flow file flavour '"+flow_files[i]+"'.");
					continue;
			}

			if ((lock_files = lock_flow(flow_files[i]))!==undefined) {
				if (check_held(addr, scfg, myaddr)) {
					unlock_flow(lock_files);
					continue;
				}
				break;
			}
		}
		if (i<flow_files.length) {
			log(LOG_INFO, "Attempting callout for " + addr + ", file: " + flow_files[i]);
			locks.push(lock_files);
			// Use a try/catch to ensure we clean up the lock files.
			callout(addr, scfg, locks);
			ran[addr] = true;
			locks.forEach(unlock_flow);
		}
		else {
			log(LOG_DEBUG, "No flow files to be processed.");
			break;
		}
	}

	log(LOG_DEBUG, "Done checking in "+dir+".");
}

function run_one_outbox_dir(addr, scfg, bicfg, ran)
{
	var dir = bicfg.node[addr].outbox;
	if(!dir)
		return;
	dir = backslash(dir);
	log(LOG_DEBUG, "Running outbox dir for (" + addr + "): " + dir);
	var files = directory(dir + '*');
	if(files.length) {
		log(LOG_INFO, "Attempting callout for " + addr + ", outbox files: " + files);
		var locks = [];
		callout(addr, scfg, locks, bicfg);
		ran[addr] = true;
		locks.forEach(unlock_flow);
	}
}

function touch_semaphores()
{
	semaphores.forEach(function(semname) {
		log(LOG_DEBUG, "Touching semaphore file: " + semname);
		file_touch(semname);
	});
}

function run_outbound(ran)
{
	var bicfg;
	var scfg;
	var outbound_dirs=[];
	var outbound_roots=[];
	var scfg_ob;

	log(LOG_DEBUG, "Running outbound");
	scfg = new SBBSEchoCfg(cfgfname);
	bicfg = new BinkITCfg(cfgfname);

	if (!scfg.is_flo) {
		log(LOG_ERROR, "sbbsecho not configured for FLO-style mailers.");
		return false;
	}
	Object.keys(FIDO.FTNDomains.outboundMap).forEach(function(key) {
		outbound_roots.push(FIDO.FTNDomains.outboundMap[key]);
	});
	scfg_ob = scfg.outbound.replace(/[\\\/]$/, '');
	if (outbound_roots.indexOf(scfg_ob) == -1)
		outbound_roots.push(scfg_ob);
	log(LOG_DEBUG, "Outbound roots: " + JSON.stringify(outbound_roots, null, 0));
	outbound_roots.forEach(function(oroot) {
		var dirs;

		function addDir(dir) {
			var bdir = backslash(dir);
			bdir = fullpath(bdir);
			if (outbound_dirs.indexOf(bdir) == -1) outbound_dirs.push(bdir);
		}

		function addPoints(dir) {
			var pnts = directory(backslash(dir) + '*.pnt', false);
			pnts.forEach(function (pdir) {
				if (pdir.search(/[\\\/][0-9a-z]{8}.pnt$/) >= 0 && file_isdir(pdir)) {
					addDir(pdir);
				} else {
					log(LOG_WARNING, "Unhandled/Unexpected point path '"+pdir+"'.");
				}
			});
		}

		if (file_isdir(oroot)) {
			addDir(oroot);
			addPoints(oroot);
		} else {
			log(LOG_NOTICE, "Skipping non-existent outbound directory: " + oroot);
			return;
		}
		dirs = directory(oroot+'.*', 0);
		dirs.forEach(function(dir) {
			var ext = file_getext(dir);
			if (ext === undefined)
				return;
			if (ext.search(/^\.[0-9a-f]+$/) == 0) {
				if (file_isdir(dir)) {
					addDir(dir);
					addPoints(dir);
				} else {
					log(LOG_WARNING, "Unexpected file in outbound '"+dir+"'.");
				}
			} else {
				log(LOG_WARNING, "Unhandled outbound '"+dir+"'.");
			}
		});
	});
	log(LOG_DEBUG, "Outbound dirs: " + JSON.stringify(outbound_dirs, null, 0));
	outbound_dirs.forEach(function(dir) {
		run_one_outbound_dir(dir, scfg, ran);
	});
	for(var addr in bicfg.node) {
		run_one_outbox_dir(addr, scfg, bicfg, ran);
	}
}

function inbound_auth_cb(pwd, bp)
{
	/*
	 * Loop through remote addresses, building a list of the ones with
	 * the same password that we can send mail for.
	 */
	var addrs = [];
	var ret = '-';
	var nocrypt;
	var invalid=false;

	function check_nocrypt(node) {
		if (node.nocrypt) {
			if (nocrypt === undefined)
				nocrypt = true;
		}
		else {
				nocrypt = false;
		}
	}

	log(LOG_INFO, "Remote addresses: " + bp.remote_addrs.join(' '));
	bp.remote_addrs.forEach(function(addr) {
		var cpw;
		if (bp.cb_data.binkitcfg.node[addr] !== undefined) {
			log(LOG_INFO, "Inbound session for: " + addr);
			cpw = bp.cb_data.binkitcfg.node[addr].pass;
			if (!cpw)
				cpw = '-';
			if (pwd[0].substr(0, 9) === 'CRAM-MD5-') {
				var expected = bp.getCRAM('MD5', cpw);
				if (expected === pwd[0]) {
					log(LOG_INFO, "CRAM-MD5 password match for " + addr);
					addrs.push(addr);
					check_nocrypt(bp.cb_data.binkitcfg.node[addr]);
					ret = cpw;
				} else {
					log(LOG_WARNING, "CRAM-MD5 password mismatch for " + addr
						+ format(" (expected: %s, received: %s)", expected, pwd[0]));
					invalid = true;
				}
			}
			else {
				// TODO: Deal with arrays of passwords?
				if (!bp.plain_auth_only								// [BinkP] PlainAuthOnly=false
					&& !bp.cb_data.binkitcfg.node[addr].nomd5) {	// [node:] BinkpAllowPlainAuth=false
					log(LOG_WARNING, "CRAM-MD5 required (and not provided) by " + addr);
					invalid = "CRAM-MD5 authentication required";
				}
				else if (bp.cb_data.binkitcfg.node[addr].pass === pwd[0]) {
					log(LOG_INFO, "Plain-text password match for " + addr);
					addrs.push(addr);
					check_nocrypt(bp.cb_data.binkitcfg.node[addr]);
					ret = cpw;
				}
				else {
					log(LOG_WARNING, "Plain-text password mismatch for " + addr);
					invalid = true;
				}
			}
		}
		else
			log(LOG_DEBUG, "Unconfigured address "+addr);
	});
	if (addrs.length === 0) {
		if (invalid) {
			bp.sendCmd(bp.command.M_ERR, typeof invalid == "string" ? invalid : "Password mismatch");
		}
		else {
			// If we have NONE of their nodes configured, we can send them files for ALL of them.
			addrs = bp.remote_addrs;
			// And allow unencrypted sessions.
			nocrypt = true;
		}
	}
	else {
		// If we have SOME of their nodes configured, only send them files for authenticated ones.
		bp.remote_addrs = addrs;
		// Require encryption unless all configured nodes say otherwise.
		if (nocrypt === undefined)
			nocrypt = false;
	}
	bp.require_crypt = bp.crypt_support && !bp.plain_auth_only && !nocrypt;

	add_outbound_files(addrs, bp);
	return ret;
}

function run_inbound(sock)
{
	var myaddr = FIDO.parse_addr(system.fido_addr_list[0], 1, 'fidonet');
	var bp = new BinkP(version_notice, undefined, rx_callback, tx_callback);
	var port;
	var f;
	var success = false;
	var locks = [];

	log(LOG_INFO, bp.revision + " inbound connection from " +sock.remote_ip_address+":"+sock.remote_port);
	bp.cb_data = {
		binkitcfg:new BinkITCfg(cfgfname),
		binkit_scfg:new SBBSEchoCfg(cfgfname),
		binkit_file_actions:{},
		binkit_flow_contents:{},
		binkit_locks:locks
	};
	bp.system_operator = bp.cb_data.binkitcfg.sysop;
	bp.plain_auth_only = bp.cb_data.binkitcfg.plain_auth_only;
	bp.crypt_support = bp.cb_data.binkitcfg.crypt_support;

	// TODO: Force debug mode for now...
	bp.debug = true;
	bp.default_zone = myaddr.zone;
	bp.default_domain = myaddr.domain;
	bp.want_callback = callout_want_callback;
	if (bp.cb_data.binkitcfg.caps !== undefined)
		bp.capabilities = bp.cb_data.binkitcfg.caps;

	// We can't use the defaults since the defaults are only 4D addresses.
	bp.addr_list = [];
	system.fido_addr_list.forEach(function(faddr){bp.addr_list.push(FIDO.parse_addr(faddr, this.default_zone));}, this);

	// We won't add files until the auth finishes...
	success = bp.accept(sock, inbound_auth_cb);
	
	// Statistics
	if(bp.remote_addrs !== undefined && bp.remote_addrs.length) {
		var addr = bp.remote_addrs[0];
		if(addr) {
			update_stats(stats.inbound[success], addr, bp, sock.remote_ip_address);
			update_totals(stats.totals, addr, bp, false, success);
		}
	}
	callout_done(bp);

	locks.forEach(function(lock) {
		unlock_flow(lock);
	});
}

function poll_node(addr_str, scfg, bicfg, myaddr)
{
	var lock_files;
	var locks = [];

	if (scfg === undefined)
		scfg = new SBBSEchoCfg(cfgfname);

	if (myaddr === undefined)
		myaddr = FIDO.parse_addr(system.fido_addr_list[0], 1, 'fidonet');

	var addr = FIDO.parse_addr(addr_str, 1, 'fidonet');

	var outbound_dir = outbound_root(addr, scfg);
	log(LOG_DEBUG, "poll_node " + addr_str + ", outbound_dir: " + outbound_dir);
	if(file_isdir(outbound_dir)) {
		if ((lock_files = lock_flow(outbound_dir + addr.flo_outbound(myaddr.zone))) === undefined)
			return;
		else {
			if (check_held(addr, scfg, myaddr)) {
				unlock_flow(lock_files);
				return;
			}
		}
	}
	log(LOG_INFO, "Attempting poll for node "+addr);
	if(lock_files !== undefined)
		locks.push(lock_files);
	// Use a try/catch to ensure we clean up the lock files.
	callout(addr, scfg, locks, bicfg);
	locks.forEach(unlock_flow);
}

function run_polls(ran)
{
	var scfg;
	var bicfg;
	var myaddr;
	var locks = [];

	log(LOG_DEBUG, "Running polls");
	scfg = new SBBSEchoCfg(cfgfname);
	bicfg = new BinkITCfg(cfgfname);
	myaddr = FIDO.parse_addr(system.fido_addr_list[0], 1, 'fidonet');

	Object.keys(bicfg.node).forEach(function(addr_str) {
		if (!bicfg.node[addr_str].poll)
			return;

		var addr = FIDO.parse_addr(addr_str, 1, 'fidonet');

		if (ran[addr] !== undefined)
			return;
		poll_node(addr_str, scfg, bicfg, myaddr);
		ran[addr] = true;
	});
}

// Upgrade from separate binkit.ini/sbbsecho.ini/ftn_domains.ini to a combined sbbsecho.ini
function upgrade()
{
	var sbbsecho_ini = new File(file_cfgname(system.ctrl_dir, "sbbsecho.ini"));
	if (!sbbsecho_ini.open('r+'))
		return sbbsecho_ini.name + " open error " + sbbsecho_ini.error;
	printf("Updating %s\r\n", sbbsecho_ini.name);

	var binkit_ini = new File(file_cfgname(system.ctrl_dir, "binkit.ini"));
	if(binkit_ini.open("r")) {

		sbbsecho_ini.iniSetValue("BinkP", "Capabilities", binkit_ini.iniGetValue(null, "Capabilities", ""));
		sbbsecho_ini.iniSetValue("BinkP", "Sysop", binkit_ini.iniGetValue(null, "Sysop", ""));

		var nodes = sbbsecho_ini.iniGetSections("node:");
		var links = binkit_ini.iniGetSections();
		for(var i in links) {
			var section = "node:" + links[i];
			for(var n in nodes) {
				if(nodes[n] + '@' == section.substr(0, nodes[n].length + 1)) {
					section = nodes[n];
					break;
				}
			}
			var obj = sbbsecho_ini.iniGetObject(section);
			if(obj == null)
				obj = {};
			sbbsecho_ini.iniRemoveSection(section);
			obj.SessionPwd = binkit_ini.iniGetValue(links[i], "Password", "");
			obj.BinkpHost = binkit_ini.iniGetValue(links[i], "host", "");
			obj.BinkpPort = binkit_ini.iniGetValue(links[i], "port", 24554);
			obj.BinkpPoll = binkit_ini.iniGetValue(links[i], "poll", false);
			obj.BinkpAllowPlainAuth = binkit_ini.iniGetValue(links[i], "AllowPlainPassword", false);
			obj.BinkpAllowPlainText = binkit_ini.iniGetValue(links[i], "AllowUnencrypted", false);
			obj.BinkpSourceAddress = binkit_ini.iniGetValue(links[i], "SourceAddress", "");
			print("Writing");
			print(JSON.stringify(obj, null, 4));
			sbbsecho_ini.iniSetObject("node:" + links[i], obj);
		}
		binkit_ini.close();

		file_rename(binkit_ini.name, binkit_ini.name + ".old");
	}

	/* Merge ftn_domains.ini -> sbbsecho.ini */
	var domains_ini = new File(file_cfgname(system.ctrl_dir, "ftn_domains.ini"));
	if(domains_ini.open("r")) {

		var domains = domains_ini.iniGetAllObjects("name");
		for(var d in domains) {
			var section = "domain:" + domains[d].name;
			delete domains[d].name;
			sbbsecho_ini.iniSetObject(section, domains[d]);
		}
		domains_ini.close();
		file_rename(domains_ini.name, domains_ini.name + ".old");
	}
	sbbsecho_ini.close();
	return true;
}

var sock;
try {
	sock = client.socket;
}
catch(e) {}
var ran = {};
var i;
var addr;
var stats_file = new File(system.data_dir + 'binkstats.ini');
if(stats_file.open("r")) {
	var sections = stats_file.iniGetSections("totals: ");
	for(i in sections) {
		stats.totals[sections[i].slice(8)] = stats_file.iniGetObject(sections[i]);
	}
	stats_file.close();
}

log(LOG_INFO, version_notice + " invoked with options: " + argv.join(' '));

if (system.fido_addr_list.length < 1) {
	alert("No system FidoNet address configured");
	exit(1);
}

for (i = 0; i < argv.length; i++) {
	if(file_getext(argv[i]) == ".ini")
		cfgfname = argv[i];
}

// If we're running as a service, call run_inbound().
if (sock !== undefined && sock.descriptor !== -1)
	run_inbound(sock);
else {
	if (argv.indexOf('install') !== -1) {
		var result = load({}, "install-binkit.js");
		if (result != true) {
			alert(result);
			exit(1);
		}
		exit(0);
	}
	if (argv.indexOf('upgrade') !== -1) {
		var result = upgrade();
		if (result != true) {
			alert(result);
			exit(1);
		}
		exit(0);
	}
	if (argv.indexOf('-l') !== -1) {
		for (i=0; i<argv.length; i++) {
			if (argv[i] === '-l') {
				i++;
				if (argv[i] === undefined) {
					log(LOG_ERROR, 'No link specified with -l');
					continue;
				}
				try {
					FIDO.parse_addr(argv[i]);
				}
				catch(e_addr) {
					log(LOG_ERROR, "Error parsing address '"+argv[i]+"'.");
					continue;
				}
				poll_node(argv[i]);
			}
		}
	}
	else {
		if (argv.indexOf('-P') !== -1)
			run_polls(ran);
		else {
			run_outbound(ran);
			if (argv.indexOf('-p') !== -1)
				run_polls(ran);
		}
	}
}

// Update binkstats.ini
if(stats_file.open(stats_file.exists ? 'r+':'w+')) {
	stats_file.ini_key_prefix = '\t';
	stats_file.ini_section_separator = '';
	stats_file.ini_value_separator = ' = ';
	stats_file.iniReplaceObject = function(sec, obj) { this.iniRemoveSection(sec); this.iniSetObject(sec, obj); };

	for(i in stats.totals)
		stats_file.iniSetObject('totals: ' + i, stats.totals[i]);
	for(i in stats.callout[true])
		stats_file.iniReplaceObject('callout success: ' + i, stats.callout[true][i]);
	for(i in stats.callout[false])
		stats_file.iniReplaceObject('callout failure: ' + i, stats.callout[false][i]);
	for(i in stats.inbound[true])
		stats_file.iniReplaceObject('inbound success: ' + i, stats.inbound[true][i]);
	for(i in stats.inbound[false])
		stats_file.iniReplaceObject('inbound failure: ' + i, stats.inbound[false][i]);
	stats_file.close();
}
touch_semaphores();
