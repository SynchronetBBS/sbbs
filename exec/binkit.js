/*
 * Intentionally simple "Advanced BinkleyTerm Style Outbound"
 * mailer.
 * 
 * See FTS-5005 for details.
 */

load("binkp.js");
load("fidocfg.js");

function addr_from_path(path)
{
	// Convert a path into a 4D fidonet address
	// TODO: 5D based on root outbound name.
	
}

function insense(str)
{
	if (system.platform !== 'Win32')
		return str.replace(/[a-zA-Z]/g, function(c) {
			return '['+c.toUpperCase()+c.toLowerCase()+']';
		});
	return str;
}

function lock_flow(file, csy)
{
	var ret = {
		bsy:new File(file.replace(/\.*?$/, '.bsy')),
		cst:new File(file.replace(/\.*?$/, '.csy'))
	};

	if (!ret.bsy.open("web")) {
		// TODO: The suggested check if it's stale introduces a race condition.
		return undefined;
	}
	if (csy) {
		if (!ret.csy.open("web")) {
			bsy.close();
			bsy.remove();
			return undefined;
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

function callout_rx_callback(fname)
{
	// TODO: Handle received files.
}

function callout_want_callback(fobj, fsize, fdate, offset)
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

function callout(file, scfg)
{
	/*
	 * TODO: We can force 5D stuff here by adding a "fidonet" argument
	 * in parse_addr()
	 */
	var myaddr = FIDO.parse_addr(system.fido_addr_list[0], 1);
	var bp = new BinkP('BinkIT/'+("$Revision$".split(' ')[1]), undefined, callout_rx_callback);

	// Force debug mode for now...
	bp.debug = true;
	bp.default_zone = myaddr.zone;
	bp.default_domain = maddr.domain;
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
	var flow_files;
	var lock_files;
	var i;

	log(LOG_DEBUG, "Running outbound dir "+dir);

	// First, look for any node with pending netmail and handle that node.
	while (!js.terminated) {
		flow_files = directory(dir+insense('*.?ut'));
		if (flow_files.length == 0)
			break;
		for (i=0; i<flow_files.length; i++) {
			// Find one we can create a .bsy and .csy file for...
			if (file_getext(flow_files[i]).substr(0,2).search(/^\.[icdo]$/i) !== 0)
				continue;
			if ((lock_files = lock_flow(flow_files[i], true))!==undefined) {
				// TODO: Check hold file.
				break;
			}
		}
		if (i<flow_files.length) {
			log(LOG_DEBUG, "Attempting callout for file "+flow_files[i]);
			// Use a try/catch to ensure we clean up the lock files.
			try {
				callout(flow_files[i], scfg);
			}
			catch(e) {
				unlock_flow(lock_files);
				throw(e);
			}
			unlock_flow(lock_files);
		}
		else
			break;
	}
}

function run_outbound()
{
	var scfg;
	var outbound_base;
	var outbound_dirs=[];
	var tmp;

	log(LOG_DEBUG, "Running outbound");
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

		if (file_getext(dir).search(/^\.[0-9a-fA-F]+$/) == 0) {
			if (file_isdir(dir)) {
				outbound_dirs.push(backslash(dir));
				pnts = directory(backslash(dir)+insense(".pnt"), false);
				pnts.forEach(function(pdir) {
					if (pdir.search(/[0-9a-zA-Z]{8}.[Pp][Nn][Tt]$/) >= 0 && file_isdir(pdir))
						outbound_dirs.push(backslash(pdir));
				});
			}
		}
	});
	outbound_dirs.forEach(function(dir) {
		run_one_outbound_dir(dir, scfg);
	});
}

run_outbound();
