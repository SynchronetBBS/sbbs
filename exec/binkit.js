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
 *    where there is a "/path/to/outboud.001" directory.  The
 *    behaviour in this case is undefined and likely to be bad.
 * 
 * See FTS-5005 for details.
 */

load("binkp.js");
load("fidocfg.js");

function lock_flow(file, csy)
{
	var ret = {
		bsy:new File(file.replace(/\.*?$/, '.bsy')),
		cst:new File(file.replace(/\.*?$/, '.csy'))
	};

	// Takes ownership of a lockfile if it's more than six hours old.
	/*
	 * Race-safe version...
	 * 1) If date is "too old", pick a random time in the future.
	 * 2) Set the file date to that random time.
	 * 3) Wait one second.
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

function callout_auth_cb(mode, bp)
{
	/*
	 * TODO: Add all outgoing mail for all authenticated addresses.
	 * This is tricky since we need to only use addresses that are
	 * configured with the single password we sent.  The multiple
	 * password extension (FRL-1013) was withdrawn, so we can't do
	 * that.
	 */
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

function callout(addr, scfg)
{
	/*
	 * TODO: We can force 5D stuff here by adding a "fidonet" argument
	 * in parse_addr()
	 */
	var myaddr = FIDO.parse_addr(system.fido_addr_list[0], 1);
	var bp = new BinkP('BinkIT/'+("$Revision$".split(' ')[1]), undefined, callout_rx_callback);

	log(LOG_INFO, "Callout to "+addr+" started.");
	// Force debug mode for now...
	bp.debug = true;
	bp.default_zone = myaddr.zone;
	bp.default_domain = myaddr.domain;
	bp.want_callback = callout_want_callback;
	bp.rx_callback = callout_rx_callback;

	// We won't add files until the auth finishes...
	/*
	 * TODO: We're currently using the packet password... this is not
	 * only not a good idea, it's a BAD idea since the session password
	 * is not transmitted in the clear but packet passwords are.
	 */
}

function run_one_outbound_dir(dir, scfg)
{
	var myaddr = FIDO.parse_addr(system.fido_addr_list[0], 1);
	var flow_files;
	var lock_files;
	var addr;
	var ext;
	var i;

	log(LOG_DEBUG, "Running outbound dir "+dir);

	function check_held(addr)
	{
		var until;
		var f = new File(scfg.outboud.replace(/[\/\\]$/,'')+addr.flo_outbound(myaddr.zone)+'.hld');

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
		while (!js.terminated) {
			flow_files = directory(dir+wildcard);
			if (flow_files.length == 0)
				break;
			flow_file_loop:
			for (i=0; i<flow_files.length; i++) {
				try {
					addr = FIDO.parse_flo_file_path(flow_files[i], myaddr.zone);
				}
				catch(addr_e) {
					log(LOG_WARNING, addr_e+" when checking '"+flow_files[i]+"' (default zone: "+myaddr.zone+")");
					continue;
				}
				ext = file_getext(flow_files[i]);

				// Ensure this is the "right" outbound (file case, etc)
				if (flow_files[i] !== scfg.outboud.replace(/[\\\/]$/,'')+addr.flo_outbound+ext) {
					log(LOG_WARNING, "Unexpected file path '"+flow_files[i]+"' (skipped)");
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
						if (wildcard === '*.?lf')
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
				try {
					callout(flow_files[i], scfg);
				}
				catch(callout_e) {
					log(LOG_DEBUG, "Unlocking after exception in callout.");
					unlock_flow(lock_files);
					throw(callout_e);
				}
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
