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
 * 3) The domain is always 'fidonet'
 * 
 * See FTS-5005 for details.
 */

load("binkp.js");
load("fidocfg.js");

function lock_flow(file, csy)
{
	var ret = {
		bsy:new File(file.replace(/\.*?$/, '.bsy')),
		csy:new File(file.replace(/\.*?$/, '.csy'))
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

		// TODO: This is hacked for a signed 32-bit time_t... watch out in 2038!
		orig_date = f.date;
		if (orig_date > (now - 60*60*6))
			return false;
		remain = 0x80000000 - now;
		future = now + random(remain);
		f.date = future;
		mswait(1000);
		if (f.date != future)
			return false;
		if (!f.open("wb")) {
			f.date = orig_date;
			return false;
		}
		f.date = now;
		return true;
	}

	if (!ret.bsy.open("web")) {
		if (!take_lockfile(ret.bsy))
			return undefined;
	}
	if (csy) {
		if (!ret.csy.open("web")) {
			if (!take_lockfile(ret.csy)) {
				ret.bsy.close();
				ret.bsy.remove();
				return undefined;
			}
		}
	}
	ret.bsy.writeln("BinkIT");
	if (csy)
		ret.csy.writeln("BinkIT");
	return ret;
}

function unlock_flow(locks)
{
	if (locks.csy !== undefined) {
		locks.csy.close();
		locks.csy.remove();
	}
	if (locks.bsy !== undefined) {
		locks.bsy.close();
		locks.bsy.remove();
	}
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
	addrs.forEach(function(addr) {
		log(LOG_DEBUG, "Adding outbound files for "+addr);
		// Find all possible flow files for the remote.
		var allfiles = directory(bp.cb_data.binkit_scfg.outbound.replace(/[\\\/]$/)+addr.flo_outbound(bp.default_zone, bp.default_domain)+'*');
		// Parse flow files and call addFile() tracking what to do on success.
		allfiles.forEach(function(file) {
			var flo;
			var line;
			var action;
			var i;
			var fnchars = '0123456789abcdefghijklmnopqrstuvwxyz';
			var fname;

			switch(file_getext(file).toLowerCase()) {
				case '.flo':
				case '.ilo':
				case '.hlo':
				case '.clo':
				case '.dlo':
					flo = new File(file);
					if (!flo.open("r")) {
						log(LOG_ERROR, "Unable to open FLO file '"+flo.name+"'.");
						break;
					}
					if (bp.cb_data.binkit_flow_contents[flo.name] === undefined)
						bp.cb_data.binkit_flow_contents[flo.name] = [];
					while((line = flo.readln(2048))) {
						switch(line.charAt(0)) {
							case '#':
								if (bp.addFile(line.substr(1)))
									bp.cb_data.binkit_file_actions[flo.name] = 'TRUNCATE';
								bp.cb_data.binkit_flow_contents[flo.name].push(line.substr(1));
								break;
							case '^':
							case '-':
								if (bp.addFile(line.substr(1)))
									bp.cb_data.binkit_file_actions[flo.name] = 'DELETE';
								bp.cb_data.binkit_flow_contents[flo.name].push(line.substr(1));
								break;
							case '~':
							case '!':
								break;
							case '@':
								line = line.substr(1);
								if (bp.addFile(line))
									bp.cb_data.binkit_flow_contents[flo.name].push(line.substr(1));
								break;
							default:
								if (bp.addFile(line))
									bp.cb_data.binkit_flow_contents[flo.name].push(line.substr(1));
								break;
						}
					}
					flo.close();
					break;
				case '.out':
				case '.iut':
				case '.hut':
				case '.cut':
				case '.dut':
					fname = '';
					for (i=0; i<8; i++)
						fname += fnchars[random(fnchars.length)];
					fname += '.pkt';
					if (bp.addFile(file, fname))
						bp.cb_data.binkit_file_actions[flo.name] = 'DELETE';
					break;
				case '.rep':
					fname = '';
					for (i=0; i<8; i++)
						fname += fnchars[random(fnchars.length)];
					fname += '.rep';
					if (bp.addFile(file, fname))
						bp.cb_data.binkit_file_actions[flo.name] = 'DELETE';
					break;
				default:
					log(LOG_WARNING, "Unsupported flow file type '"+file+"'.");
					break;
			}
		});
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

	if (bp.cb_data.binkitpw === undefined)
		addrs.push(bp.binkit_to_addr);
	else {
		bp.remote_addrs.forEach(function(addr) {
			if (bp.cb_data.binkitcfg.node[addr] !== undefined) {
				if (bp.cb_data.binkitcfg.node[addr].pw === bp.cb_data.binkitpw)
					addrs.push(addr);
			}
			else
				log(LOG_DEBUG, "Unconfigured address "+addr);
		});
	}

	add_outbound_files(addrs);
}

/*
 * Delete completed flo files.
 */
function callout_tx_callback(fname, bp)
{
	var j;

	Object.keys(bp.bp.cb_data.binkit_flow_contents).forEach(function(flo) {
		if (file_exists(flo)) {
			while ((j = bp.cb_data.binkit_flow_contents[flo].indexOf(fname)) !== -1)
				bp.cb_data.binkit_flow_contents[flo].splice(j, 1);
			if (bp.cb_data.binkit_flow_contents[flo].length == 0)
				file_remove(flo);
		}
	});
}

function callout_rx_callback(fname, bp)
{
	// TODO: Handle received files.
}

function callout_want_callback(fobj, fsize, fdate, offset, bp)
{
	/*
	 * TODO: Currently a copy/paste from binkp.js...
	 * Likely we'll want magical handling for control files (*.TIC,
	 * *.REQ, and *.PKT, Bundle Files, and the default handling for the
	 * rest.
	 */

	// Reject duplicate filenames... a more robust callback would rename them.
	// Or process the old ones first.
	if (this.received_files.indexOf(fobj.name) != -1)
		return this.file.REJECT;
	// Skip existing files.
	if (file_exists(fobj.name))
		return this.file.SKIP;
	// Accept everything else
	return this.file.ACCEPT;
}

function callout_done(bp)
{
	var f;

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
					if (file_remove(file))
						log(LOG_INFO, "Removed '"+file+"'.");
					else
						log(LOG_ERROR, "Unable to remove '"+file+"'.");
			}
		}
	});

	// TODO: update incomplete flow files...
}

function callout(addr, scfg)
{
	var myaddr = FIDO.parse_addr(system.fido_addr_list[0], 1, 'fidonet');
	var bp = new BinkP('BinkIT/'+("$Revision$".split(' ')[1]), undefined, callout_rx_callback, callout_tx_callback);
	var port;
	var f;

	log(LOG_INFO, "Callout to "+addr+" started.");
	bp.cb_data = {
		binkitcfg:new BinkITCfg(),
		binkit_to_addr:addr,
		binkit_scfg:scfg,
		binkit_file_actions:{},
		binkit_flow_contents:{},
	};
	if (bp.cb_data.binkitcfg.node[addr] !== undefined) {
		bp.cb_data.binkitpw = bp.cb_data.binkitcfg.node[addr].pass;
		port = bp.cb_data.binkitcfg.node[addr].port;
		bp.require_md5 = !(bp.cb_data.binkitcfg.node[addr].nomd5);
	}
	// TODO: Force debug mode for now...
	bp.debug = true;
	bp.default_zone = myaddr.zone;
	bp.default_domain = myaddr.domain;
	bp.want_callback = callout_want_callback;

	// We won't add files until the auth finishes...
	bp.connect(addr, bp.cb_data.binkitpw, callout_auth_cb, port);

	callout_done(bp);

	// TODO: Some real .try information...
	f = new File(bp.cb_data.binkit_scfg.outbound.replace(/[\\\/]$/)+addr.flo_outbound(bp.default_zone, bp.default_domain)+'.try');
	if (f.open("w")) {
		f.writeln("Callout complete.");
		f.close();
	}
	else {
		log(LOG_ERROR, "Unable to create .try file '"+f.name+"'.");
	}
}

function run_one_outbound_dir(dir, scfg)
{
	var myaddr = FIDO.parse_addr(system.fido_addr_list[0], 1, 'fidonet');
	var flow_files;
	var lock_files;
	var ext;
	var i;
	var ran = {};

	log(LOG_DEBUG, "Running outbound dir "+dir);

	function check_held(addr)
	{
		var until;
		var f = new File(scfg.outbound.replace(/[\/\\]$/,'')+addr.flo_outbound(myaddr.zone)+'.hld');

		if (!f.exists)
			return false;
		if (!f.open("rb")) {
			log(LOG_ERROR, "Unable to open hold file '"+f.name+"'");
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

	function check_flavour(wildcard, typename)
	{
		var addr;

		while (!js.terminated) {
			flow_files = directory(dir+wildcard);
			if (flow_files.length == 0)
				break;
			flow_file_loop:
			for (i=0; i<flow_files.length; i++) {
				try {
					addr = FIDO.parse_flo_file_path(flow_files[i], myaddr.zone, 'fidonet');
				}
				catch(addr_e) {
					log(LOG_WARNING, addr_e+" when checking '"+flow_files[i]+"' (default zone: "+myaddr.zone+")");
					continue;
				}
				if (ran[addr] !== undefined)
					continue;
				ext = file_getext(flow_files[i]);

				// Ensure this is the "right" outbound (file case, etc)
				if (flow_files[i] !== scfg.outbound.replace(/[\\\/]$/,'')+addr.flo_outbound(myaddr.zone)+ext.substr(1)) {
					log(LOG_WARNING, "Unexpected file path '"+flow_files[i]+"' expected '"+scfg.outbound.replace(/[\\\/]$/,'')+addr.flo_outbound(myaddr.zone)+ext.substr(1)+"' (skipped)");
					continue;
				}

				switch(ext.substr(0, 2)) {
					case '.h':
						log(LOG_DEBUG, "Skipping hold flavourd flow file '"+flow_files[i]+"'.");
						continue;
					case '.c':
					case '.d':
					case '.i':
						break;
					case '.f':
						if (wildcard === '*.?lo')
							break;
						continue;
					case '.o':
						if (wildcard === '*.?ut')
							break;
						continue;
					default:
						log(LOG_WARNING, "Unknown flow file flavour '"+flow_files[i]+"'.");
						continue;
				}

				if ((lock_files = lock_flow(flow_files[i], true))!==undefined) {
					if (check_held(addr)) {
						unlock_flow(lock_files);
						continue;
					}
					break;
				}
			}
			if (i<flow_files.length) {
				log(LOG_DEBUG, "Attempting callout for file "+flow_files[i]);
				// Use a try/catch to ensure we clean up the lock files.
				callout(addr, scfg);
				ran[addr] = true;
				unlock_flow(lock_files);
			}
			else {
				log(LOG_DEBUG, "No "+typename+" typed flow files to be processed.");
				break;
			}
		}
	}

	// First, look for any node with pending netmail and handle that node.
	check_flavour('*.?ut', "netmail");
	log(LOG_DEBUG, "Done checking netmail in "+dir+", checking file references.");

	// Now check for pending pending file reference
	check_flavour('*.?lo', "file reference");
	log(LOG_DEBUG, "Done checking file references in "+dir+", checking file references.");
}

function run_outbound()
{
	var scfg;
	var outbound_base;
	var outbound_dirs=[];
	var tmp;

	log(LOG_INFO, "Running outbound");
	scfg = new SBBSEchoCfg();

	if (!scfg.is_flo) {
		log(LOG_ERROR, "sbbsecho not configured for FLO-style mailers.");
		return false;
	}
	outbound_dirs = [];
	if (file_isdir(scfg.outbound))
		outbound_dirs.push(scfg.outbound);
	tmp = directory(scfg.outbound.replace(/[\\\/]$/,'')+'.*', 0);
	tmp.forEach(function(dir) {
		var pnts;

		if (file_getext(dir).search(/^\.[0-9a-f]+$/) == 0) {
			if (file_isdir(dir)) {
				outbound_dirs.push(backslash(dir));
				pnts = directory(backslash(dir)+'.pnt', false);
				pnts.forEach(function(pdir) {
					if (pdir.search(/[\\\/][0-9a-z]{8}.pnt$/) >= 0 && file_isdir(pdir))
						outbound_dirs.push(backslash(pdir));
					else
						log(LOG_WARNING, "Unhandled/Unexpected point path '"+pdir+"'.");
				});
			}
			else
				log(LOG_WARNING, "Unexpected file in outbound '"+dir+"'.");
		}
		else
			log(LOG_WARNING, "Unhandled outbound '"+dir+"'.");
	});
	outbound_dirs.forEach(function(dir) {
		run_one_outbound_dir(dir, scfg);
	});
}

run_outbound();
