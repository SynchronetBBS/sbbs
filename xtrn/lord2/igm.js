'use strict';

js.load_path_list.unshift(js.exec_dir+"dorkit/");
load("dorkit.js", "-l");
require("l2lib.js", "Player_Def");

function usage()
{
	lln('  Usage:  `0jsdoor igm.js crap.igm`2');
	sln('');
	lln('  If IGM has already been installed, it will be automatically uninstalled.');
	sln('');
	sln('');
}

function parse_igm(f)
{
	var l;
	var m;
	var msg = false;
	var ret = {name:'', replace:[], message:[]};

	while ((l = f.readln())!==null) {
		if (msg) {
			ret.message.push(l);
		}
		else {
			if (l.substr(0, 9).toLowerCase() === 'igmname: ')
				ret.name = l.substr(9);
			else if (l.toLowerCase() === 'installmessage:')
				msg = true;
			else {
				m = l.match(/^replacescreen: ([0-9]+) ([0-9]+) (show|noshow)$/i);
				if (m !== null) {
					ret.replace.push({block:parseInt(m[1], 10), offset:parseInt(m[2], 10), show:m[3].toLowerCase()==='show'});
				}
			}
		}
	}
	return ret;
}

function install_igm(fname, igm)
{
	var smap;
	var imap;
	var mrec;
	var srec;
	var irec;
	var i;

	lln('`%  Installing IGM.`2');
	sln('');
	// Create here for ref-only IGMs
	smap = new RecordFile(fname+'.sav', Map_Def);
	if (smap === null) {
		lln('  `bError:`4  Unable to open '+fname+'.sav');
		sln('');
		sln('');
		return false;
	}
	if (igm.replace.length > 0) {
		// First, make sure all the replacements are there...
		imap = new RecordFile(fname+'.dat', Map_Def);
		if (imap === null) {
			lln('  `bError:`4  Unable to open '+fname+'.dat');
			sln('');
			sln('');
			return false;
		}
		igm.replace.forEach(function(rep) {
			if (rep.offset > imap.length) {
				lln('  `bError:`4  Could not install screen - '+rep.offset+' doesn\'t exist');
				sln('');
				sln('');
				return false;
			}
			if (rep.block < 1 || rep.block > 1600) {
				lln('  `bError:`4  Could not install to block - '+rep.block+' doesn\'t exist');
				sln('');
				sln('');
				return false;
			}
		});
		for (i = 0; i < imap.length; i++) {
			smap.new();
		}
		igm.replace.forEach(function(rep) {
			mrec = load_map(rep.block);
			if (mrec === null) {
				// Add new map record...
				mrec = mfile.new();
				// TODO: Apparently previously blank maps were tagged somehow and left?
				lln('  `2Adding screen `0'+rep.block+'`2 with record `0'+rep.offset+' `2from `0'+imap.file.name+'`2.');
			}
			else {
				lln('  `2Replacing screen `0'+rep.block+'`2 with record `0'+rep.offset+' `2from `0'+imap.file.name+'`2.');
			}
			srec = smap.get(rep.offset - 1);
			copy_map(mrec, srec);
			srec.put();
			irec = imap.get(rep.offset - 1);
			copy_map(irec, mrec);
			mrec.put();
			// TODO: Current show/noshow state is not saved/restored...
			world.mapdatindex[rep.block - 1] = mrec.Record + 1;
			world.hideonmap[rep.block - 1] = rep.show ? 0 : 1;
			world.put();
		});
	}
	igm.message.forEach(function(l) {
		lln(l);
	});
	lw('`r0`2');
	return true;
}

function uninstall_igm(fname, igm)
{
	var smap;
	var mrec;
	var srec;
	var i;

	lln('  Replacing changed screens from backup made at install...');
	if (igm.replace.length > 0) {
		// First, make sure all the replacements are there...
		smap = new RecordFile(fname+'.sav', Map_Def);
		if (smap === null) {
			lln('  `bError:`4  Unable to open '+fname+'.sav');
			sln('');
			sln('');
			return false;
		}
		igm.replace.forEach(function(rep) {
			if (rep.offset > smap.length) {
				lln('  `bError:`4  Could not install screen - '+rep.offset+' doesn\'t exist');
				sln('');
				sln('');
				return false;
			}
			if (rep.block < 1 || rep.block > 1600) {
				lln('  `bError:`4  Could not install to block - '+rep.block+' doesn\'t exist');
				sln('');
				sln('');
				return false;
			}
		});
		igm.replace.forEach(function(rep) {
			mrec = load_map(rep.block);
			if (mrec === null) {
				// Add new map record...
				mrec = mfile.new();
				// TODO: Apparently previously blank maps were tagged somehow and left?
				lln('  `2Adding screen `0'+rep.block+'`2 with record `0'+rep.offset+' `2from `0'+smap.file.name+'`2.');
			}
			else {
				lln('  `2Replacing screen `0'+rep.block+'`2 with record `0'+rep.offset+' `2from `0'+smap.file.name+'`2.');
			}
			srec = smap.get(rep.offset - 1);
			copy_map(srec, mrec);
			mrec.put();
			world.mapdatindex[rep.block - 1] = mrec.Record + 1;
			// TODO: Current show/noshow state is not saved/restored...
			world.put();
		});
	}
	file_remove(smap.file.name);
	lw('`r0`2');
	return true;
}

sln('');
lln('`0  IGM `2install`0/`2uninstall utility for `0LORD2`2 by Seth A. Robinson.  `%JS`2');
sln('');

if (argv.length != 1) {
	usage();
	more();
	exit(0);
}

var igmf;
if (!file_exists(argv[0]))
	argv[0] = argv[0]+'.igm';
if (!file_exists(argv[0])) {
	lln('  `bError:`4  Cannot find '+argv[0]);
	sln('');
	sln('');
	more();
	exit(1);
}
igmf = new File(argv[0]);
if (!igmf.open('rb')) {
	lln('  `bError:`4  Unable to open '+argv[0]);
	sln('');
	sln('');
	more();
	exit(1);
}

var igm = parse_igm(igmf);
var fname = igmf.name.replace(/\.[^\.]*$/, '');
igmf.close();

if (igm.name === '') {
	lln('  `bError:`4  No IGM name found in '+argv[0]);
	sln('');
	sln('');
	more();
	exit(1);
}

var install = true;
// TODO: Case insensitive crap...
if (file_exists(fname + '.sav')) {
	lln('`2  Found '+fname+'.sav'+'...This means IGM is installed.  Uninstalling it.');
	install = false;
}


var tpdat;
var tplst;
var idx;
if (install) {
	if (install_igm(fname, igm)) {
		sln('');
		tpdat = new File(getfname('3rdparty.dat'));
		lln('  Adding IGM name to the text file `0'+tpdat.name+'`2...');
		if (!tpdat.open('a+b')) {
			lln('  `bError:`4  Unable to open '+tpdat.name);
			sln('');
			sln('');
			more();
			exit(1);
		}
		tpdat.position = 0;
		tplst = tpdat.readAll();
		tpdat.write('  `0'+igm.name+'\r\n');
		tpdat.close();
		lln('  `0IGM '+igm.name+'`r0`2 has been installed.');
		sln('');
		lln('  Type `0jsdoor igm.js '+fname+'.igm`2 to uninstall at any time.');
		sln('');
	}
}
else {
	if (uninstall_igm(fname, igm)) {
		tpdat = new File(getfname('3rdparty.dat'));
		lln('  Removing name from '+tpdat.name+', a text file...');
		if (!tpdat.open('r+b')) {
			lln('  `bError:`4  Unable to open '+tpdat.name);
			sln('');
			sln('');
			more();
			exit(1);
		}
		tplst = tpdat.readAll();
		idx = tplst.indexOf('  `0'+igm.name);
		if (idx !== -1) {
			tplst.splice(idx, 1);
		}
		tpdat.position = 0;
		tpdat.truncate();
		tplst.forEach(function(l) {
			tpdat.write(l+'\r\n');
		});
		tpdat.close();
		lln('  `0IGM '+igm.name+'`r0`2 has been `4removed`2.');
		sln('');
	}
}

more();
