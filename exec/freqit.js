// $Id$
/*
 * Intentionally simple FREQ processor.
 * Configure in binkd with the line:
 * exec "/sbbs/exec/jsexec freqit *S" *.req *.REQ
 */

load("filebase.js");
load("fidocfg.js");

var cfg = new FREQITCfg();

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
	while(l = f.readln(65535)) {
		if ((m=l.match(/^\s*([^ ]+)\s+(.*)$/)) !== null)
			srif[m[1].toLowerCase()] = m[2];
	}
	f.close();
	return srif;
}

var dircache={};
var added={};
var files=0;

function add_file(filename, resp)
{
	if (filename === undefined)
		return;
	if (files >= cfg.maxfiles)
		return;
	if (added[filename] !== undefined)
		return;
	resp.writeln('+'+filename);
	files++;
	added[filename]='';
}

/*
 * TODO: built-in magic names... FILES and NEW
 * FILES lists all FREQable files and
 * NEW lists ones newer than 10 days.
 */

function handle_magic(magic, resp, protected, pw)
{
	var file=undefined;

	if (magic.secure && !protected)
		return;
	if (dircache[magic.dir] === undefined)
		dircache[magic.dir] = FileBase(magic.dir);
	dircache[magic.dir].forEach(function(fent) {
		if (wildmatch(fent.name, magic.match, true)) {
			if (file === undefined || fent.uldate > file.uldate)
				file = fent;
		}
	});
	if (file !== undefined) {
		add_file(file.path, resp);
		return 1;
	}
	return 0;
}

function handle_regular(match, resp, protected, pw)
{
	var file=undefined;
	var count=0;

	function handle_list(list) {
		list.forEach(function(dir) {
			if (dircache[dir] === undefined)
				dircache[dir] = FileBase(dir);
			dircache[dir].forEach(function(fent) {
				if (wildmatch(fent.name, match, true))
					add_file(fent.path, resp);
			});
		});
	}

	if (protected)
		handle_list(cfg.securedirs);
	handle_list(cfg.dirs);
}

function handle_srif(srif)
{
	var req=new File(srif.requestlist);
	var resp=new File(srif.responselist);
	var m;
	var fname;
	var pw;

	if (!req.open("r"))
		return;
	if (!resp.open("a"))
		return;
	resp.position = resp.length;

	next_file:
	while(fname=req.readln()) {
		if ((m=fname.match(/^(.*) !(.*?)$/))!==null) {
			pw=m[2];
			fname=m[1];
		}
		// First, check for magic!
		for (m in cfg.magic) {
			if (m == fname.toLowerCase()) {
				handle_magic(cfg.magic[m], resp, srif.remotestatus.toLowerCase() === 'protected', pw);
				continue next_file;
			}
		}

		// Now, check for the file...
		handle_regular(fname, resp, srif.remotestatus.toLowerCase() === 'protected', pw);
	}
}

argv.forEach(function(fname) {
	var srif = parse_srif(fname);

	if (srif !== undefined)
		handle_srif(srif);
});
