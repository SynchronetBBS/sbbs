/**
 * Copy files from inbound archives to arbitrary locations.
 * Based on nodelist_handler.js, but less ugly.
 * 
 * tickit.ini:
 * 
 * [FSX_INFO]
 * Dir = FSXNET_FSX_INFO
 * Handler = tickit/file_handler.js
 * HandlerArg = { "match": "fsxinfo.zip", "files": [{ "source": "FSXNET.NA", "destination": "/sbbs/fido/echolists/fsxnet.na" }]}
 * 
 * Where "files" is an array of { source, destination } objects, copy any
 * "source" file from the archive to "destination".
 * 
 * Wildcards are allowed in "match" and "source".
 * (Wildcards in "source" untested on Windows.)
 */

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

function Handle_TIC(tic, ctx, arg) {
	var cfg;
	var unpack;
	var dir;
	var rc;

	// Parse the config...
	try {
		cfg = JSON.parse(arg);
	} catch (e) {
		log(LOG_ERROR, 'Unable to parse "' + arg + '" as JSON, aborting');
		return false;
	}

	if (cfg.match === undefined) cfg.match = '*.*';

	if (!Array.isArray(cfg.files)) {
        log(LOG_ERROR, 'No "files" list specified.  Aborting.');
        return false;
    }

	if (!wildmatch(tic.file, cfg.match)) {
		log(LOG_DEBUG, 'TIC file ' + tic.file + " doesn't match " + cfg.match);
		return false;
	}

	var f = new File(tic.full_path);
	if (!f.open('rb', true)) {
		log(LOG_ERROR, 'Unable to open ' + tic.full_path);
		return false;
	}
	Object.keys(ctx.sbbsecho.packer).forEach(function(key) {
		var i;
		var sig = '';
		f.position = ctx.sbbsecho.packer[key].offset;
		for (i = 0; i < ctx.sbbsecho.packer[key].sig.length; i += 2) {
			sig += format('%02X', f.readBin(1));
			if (f.eof) break;
		}
		if (sig === ctx.sbbsecho.packer[key].sig) {
            unpack = ctx.sbbsecho.packer[key].unpack;
        }
	});
	f.close();

	if (unpack == undefined) {
		log(LOG_ERROR, 'Unable to identify packer for ' + tic.file);
		return false;
	}

	// Create a directory to extract to...
	dir = backslash(system.temp_dir) + 'tickit_file_copier';
	if (!file_isdir(dir)) {
		if (!mkdir(dir)) {
			log(LOG_ERROR, 'Unable to create temp directory ' + dir);
			return false;
		}
	} else {
		// Empty the directory
		directory(backslash(dir) + (system.platform == 'Win32' ? '*.*' : '*')).forEach(function(e) {
			if (!file_remove(e)) log(LOG_ERROR, 'Unable to delete file ' + e);
		});
		if (directory(backslash(dir) + (system.platform == 'Win32' ? '*.*' : '*')).length) {
			log(LOG_ERROR, 'Unable to empty directory ' + dir);
			return false;
		}
	}

	unpack = mycmdstr(unpack, tic.full_path, dir);
	if ((rc = system.exec(unpack)) != 0) {
		log(LOG_ERROR, 'Unable to extract "' + tic.file + '". Return value: ' + rc);
		return false;
	}

    cfg.files.forEach(function (e) {
        var sf = backslash(dir) + e.source;
        const df = e.destination;
        if (!file_exists(sf)) {
			log(LOG_ERROR, e.source + ' not found in ' + tic.file);
			return;
        }
        if (!file_copy(sf, df)) {
            log(LOG_ERROR, 'Failed to copy ' + sf + ' to ' + df);
        } else {
            log(LOG_DEBUG, 'Copied ' + sf + ' to ' + df);
        }
    });

	// Return false so it is still moved into the appropriate dir
	return false;
}

0; // Whaa?
