// To parse file.cnf

function mycmdstr(instr, fpath, fspec)
{
	instr = instr.replace(/%%/i, "\x00");
	instr = instr.replace(/%f/i, fpath);
	instr = instr.replace(/%g/i, system.temp_dir);
	instr = instr.replace(/%j/i, system.data_dir);
	instr = instr.replace(/%k/i, system.ctrl_dir);
	instr = instr.replace(/%n/i, system.node_dir);
	instr = instr.replace(/%o/i, system.operator);
	instr = instr.replace(/%q/i, system.qwk_id);
	instr = instr.replace(/%s/i, fspec);
	instr = instr.replace(/%!/, system.exec_dir);
	instr = instr.replace(/%@/, system.platform == 'Win32' ? system.exec_dir : '');
	instr = instr.replace(/%#/, '0');		// TODO: Can't get ENV variables...
	instr = instr.replace(/%\*/, '000');	// TODO: Can't get ENV variables...
	instr = instr.replace(/%\./, system.platform == 'Win32' ? '.exe' : '');
	instr = instr.replace(/%\?/, system.platform);
	instr = instr.replace(/\x00/i, "%");
	return instr;
}

function Handle_TIC(tic, ctx, arg)
{
	var cfg;
	var unpack;
	var dir;
	var rc;
	var fl;

	// Parse the config...
	try {
		cfg = JSON.parse(arg);
	}
	catch (e) {
		log(LOG_ERROR, "Unable to parse '"+arg+"' as JSON, aborting");
		return false;
	}

	// Validate arguments
	if (cfg.domain === undefined) {
		log(LOG_ERROR, "'"+arg+"' doesn't define the domain property");
		return false;
	}
	if (ctx.FIDO.FTNDomains.nodeListFN[cfg.domain] === undefined) {
		log(LOG_ERROR, "Domain '"+cfg.domain+"' does not have a nodelist");
		return false;
	}

	if (cfg.match === undefined)
		cfg.match = '*.*';

	if (cfg.nlmatch === undefined)
		cfg.nlmatch = '*.*';

	if (!wildmatch(tic.file, cfg.match)) {
		log(LOG_DEBUG, "TIC file "+tic.file+" doesn't match '"+cfg.match+"'");
		return false;
	}

	var f = new File(tic.full_path);
	if (!f.open("rb", true)) {
		log(LOG_DEBUG, "Unable to open file '"+tic.full_path+"'");
		return false;
	}
	Object.keys(ctx.sbbsecho.packer).forEach(function(key) {
		var i;
		var sig = '';

		f.position = ctx.sbbsecho.packer[key].offset;
		for (i=0; i<ctx.sbbsecho.packer[key].sig.length; i+=2) {
			sig += format("%02X", f.readBin(1));
			if (f.eof)
				break;
		}
		if (sig === ctx.sbbsecho.packer[key].sig)
			unpack = ctx.sbbsecho.packer[key].unpack;
	});
	f.close();

	if (unpack == undefined) {
		log(LOG_DEBUG, "Unable to identify packer for '"+tic.file+"'");
		return false;
	}

	// Create a directory to extract to...
	dir = backslash(system.temp_dir)+'nodelist_handler';
	if (!file_isdir(dir)) {
		if (!mkdir(dir)) {
			log(LOG_DEBUG, "Unable to create temp directory '"+dir+"'");
			return false;
		}
	}
	else {
		// Empty the directory
		directory(backslash(dir) + (system.platform == 'Win32' ? '*.*' : '*')).forEach(function(fname) {
			if (!file_remove(fname))
				log(LOG_ERROR, "Unable to delete file '"+fname+"'");
		});
		if (directory(backslash(dir) + (system.platform == 'Win32' ? '*.*' : '*')).length) {
			log(LOG_ERROR, "Unable to empty directory '"+dir+"'");
			return false;
		}
	}

	unpack = mycmdstr(unpack, tic.full_path, dir);
	if ((rc = system.exec(unpack)) != 0) {
		log(LOG_ERROR, "Unable to extract '"+tic.file+"' return value "+rc);
		return false;
	}

	fl = directory(backslash(dir)+cfg.nlmatch);
	if (fl.length == 0) {
		log(LOG_ERROR, "No file matching '"+cfg.nlmatch+"' in archive '"+tic.full_path+"'");
		return false;
	}
	if (fl.length > 1) {
		log(LOG_ERROR, "Multiple files matching '"+cfg.nlmatch+"' in archive '"+tic.full_path+"' ("+fl.join(', ')+")");
		return false;
	}

	log(LOG_DEBUG, "Copying "+fl[0]+" to "+ctx.FIDO.FTNDomains.nodeListFN[cfg.domain]+" from "+tic.file);
	file_copy(fl[0], ctx.FIDO.FTNDomains.nodeListFN[cfg.domain]);

	// Return false so it is still moved into the appropriate dir
	return false;
}

0;
