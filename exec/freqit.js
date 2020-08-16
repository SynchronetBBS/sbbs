// $Id: freqit.js,v 1.6 2016/01/19 06:58:36 deuce Exp $
/*
 * Intentionally simple FREQ processor.
 * Configure in binkd with the line:
 * exec "/sbbs/exec/jsexec freqit *S" *.req *.REQ
 */

load("fidocfg.js");
load("freqit_common.js");

FREQIT.add_file = function(filename, resp, cfg)
{
	if (filename === undefined)
		return;
	if (FREQIT.files >= cfg.maxfiles)
		return;
	if (FREQIT.added[filename] !== undefined)
		return;
	resp.writeln('+'+filename);
	FREQIT.files++;
	FREQIT.added[filename]='';
};

function parse_srif(fname)
{
	var f=new File(fname);
	var srif={};
	var l;
	var m;

	if (!f.open("r")) {
		log(LOG_ERROR, "Unable to find SRIF file '"+f.name+"'");
		return undefined;
	}
	while((l = f.readln(65535))) {
		if ((m=l.match(/^\s*([^ ]+)\s+(.*)$/)) !== null)
			srif[m[1].toLowerCase()] = m[2];
	}
	f.close();
	return srif;
}

function handle_srif(srif)
{
	var req=new File(srif.requestlist);
	var resp=new File(srif.responselist);
	var m;
	var fname;
	var pw;
	var cfg = new FREQITCfg();

	if (!req.open("r"))
		return;
	if (!resp.open("a"))
		return;
	resp.position = resp.length;

	next_file:
	while((fname=req.readln())) {
		if ((m=fname.match(/^(.*) !(.*?)$/))!==null) {
			pw=m[2];
			fname=m[1];
		}
		// First, check for magic!
		for (m in cfg.magic) {
			if (m == fname.toLowerCase()) {
				FREQIT.handle_magic(cfg.magic[m], resp, srif.remotestatus.toLowerCase() === 'protected', pw, cfg);
				continue next_file;
			}
		}

		// Now, check for the file...
		FREQIT.handle_regular(fname, resp, srif.remotestatus.toLowerCase() === 'protected', pw, cfg);
	}
}

argv.forEach(function(fname) {
	var srif = parse_srif(fname);

	if (srif !== undefined)
		handle_srif(srif);
});
