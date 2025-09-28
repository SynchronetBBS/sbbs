require('http.js', 'HTTPRequest');
require('cterm_lib.js', 'cterm_version_supports_b64_fonts');

/*
 * This is mostly copied from cterm_lib.js just looking for something
 * different...
 */
function readAPC()
{
	var oldctrl = console.ctrlkey_passthru;
	var response = '';
	var lastch;

	console.ctrlkey_passthru=-1;

	// cterm_lib.js uses 3 seconds, so we do too...
	for (;;) {
		var ch = console.inkey(0, 3000);
		if (ch == '')
			break;
		if (!response.length) {
			if (lastch === '\x1b' && ch === '_')
				response = '\x1b_';
			else {
				if (lastch)
					log(LOG_DEBUG, "SyncTERM Discarding: " + format("'%c' (%02X)", lastch, ascii(lastch)));
				lastch = ch;
			}
			continue;
		}
		if (lastch == '\x1b' && ch == '\\')
			break;
		if ((ch >= '\x08' && ch <= '\x0d') || (ch >= '\x20' && ch <= '\x7e')) {
			response += ch;
			if (response.slice(-2) === '\x1b\\')
				break;
		}
		else {
			if (ch == '\x1b' && ((lastch >= '\x08' && lastch <= '\x0d') || (lastch >= '\x20' && lastch <= '\x7e')))
				/* Nothing */;
			else {
				if (lastch == '\x1b')
					log(LOG_DEBUG, "SyncTERM Discarding Invalid APC character: " + format("'%c' (%02X)", lastch, ascii(lastch)));
				log(LOG_DEBUG, "SyncTERM Discarding Invalid APC character: " + format("'%c' (%02X)", ch, ascii(ch)));
			}
		}
		lastch = ch;
	}
	console.ctrlkey_passthru = oldctrl;
	var printable = response.slice(2);
	log(LOG_DEBUG, "SyncTERM query response: "+printable);
	return printable;
}

function readVersionAPC()
{
	var ver = readAPC();
	return ver.slice(13);
}

/*
 * Good for cterm versions up to 1322 only
 */
function ctermToSyncTERMVer(ver)
{
	if (ver >= 1323)
		throw new Error("Version out of range (must be strictly < 1323)");
	if (console.cterm_version === undefined || console.cterm_version < 1125)
		return 'Unknown (Guessed)';
	if (console.cterm_version === 1125)
		return 'SyncTERM 0.9.4 (Guessed)';
	if (console.cterm_version < 1151)
		return 'SyncTERM 0.9.5b (Guessed)';
	if (console.cterm_version === 1151)
		return 'SyncTERM 0.9.5 (Guessed)';
	if (console.cterm_version < 1155)
		return 'SyncTERM 1.0b (Guessed)';
	if (console.cterm_version === 1155)
		return 'SyncTERM 1.0 (Guessed)';
	if (console.cterm_version < 1304)
		return 'SyncTERM 1.1b (Guessed)';
	if (console.cterm_version === 1304)
		return 'SyncTERM 1.1rc1 (Guessed)';
	if (console.cterm_version < 1312)
		return 'SyncTERM 1.1b (Guessed)';
	// rc2, rc3, rc4, and 1.1 are all 1.312
	if (console.cterm_version === 1312)
		return 'SyncTERM 1.1 (Guessed)';
	if (console.cterm_version < 1317)
		return 'SyncTERM 1.2b (Guessed)';
	// 1.2rc1, 1.2rc2, 1.2rc3, 1.2rc4, and both 1.2 and 1.3 releases are all 1.317
	if (console.cterm_version === 1317)
		return 'SyncTERM 1.3 (Guessed)';
	// 1.4 and 1.5 are both 1.318
	if (console.cterm_version === 1318)
		return 'SyncTERM 1.5 (Guessed)';
	if (console.cterm_version === 1319)
		return 'SyncTERM 1.6 (Guessed)';
	return 'SyncTERM 1.7b (Guessed)';
}

function getSyncTERMVerStr()
{
	if (console.cterm_version === undefined || console.cterm_version < 1323)
		return ctermToSyncTERMVer(console.cterm_version);
	console.write("\x1b_SyncTERM:VER\x1b\\");
	return readVersionAPC();
}

function getLatestVerStr()
{
	var last = 0;
	var today = parseInt(strftime("%Y%m%d"), 10);
	var ver = 'SyncTERM 1.7rc1';
	var f = new File(system.ctrl_dir + 'syncterm-ver.ini');
	if (f.open(f.exists ? "r+" : "w+")) {
		var last = f.iniGetValue(null, 'LastChecked', 0);
		ver = f.iniGetValue(null, 'LatestVersion', ver);
		if (last !== today) {
			try {
				var req = new HTTPRequest(undefined, undefined, undefined, 1 /* Receive Timeout */);
				var resp = req.Get('http://syncterm.bbsdev.net/');
				var m = resp.match(/card-title">(SyncTERM )v([^ ]+)/);
				if (m !== null) {
					ver = m[1] + m[2];
					f.iniSetValue(null, 'LatestVersion', ver);
					f.iniSetValue(null, 'LastChecked', today);
				}
			}
			catch(e) {
				log(LOG_DEBUG, 'Caught error "' + e.toString() + '" trying to fetch SyncTERM version');
			}
		}
		f.close();
	}
	return ver;
}

function basicVersionOnly(verStr)
{
	return verStr.replace(/^(SyncTERM [0-9.]+).*$/, '$1');
}

(function main()
{
	var curVerStr = getSyncTERMVerStr();
	var latestVerStr = getLatestVerStr();

	/*
	 * TODO: If there are non-SyncTERM terminals that properly set CTerm versions,
	 *       this will need to avoid guessed values
	 */
	if (curVerStr.slice(8) === 'SyncTERM') {
		if (curVerStr < latestVerStr) {
			console.newline();
			console.print("\x01H\x01Y" + latestVerStr + "\x01N is available!");
			console.newline();
			console.print("Get it at http://sf.net/p/syncterm/files/latest/download");
			console.newline(2);
			console.pause();
		}
	}
})();
