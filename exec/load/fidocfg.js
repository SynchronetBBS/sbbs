function SBBSEchoCfg ()
{
	var ecfg = new File(system.ctrl_dir+'sbbsecho.cfg');
	var line;
	var m;

	this.inb = [];
	this.pktpass = {};
	this.is_flo = false;
	this.outboud = undefined;

	if (!ecfg.open("r"))
		throw("Unable to open '"+ecfg.name+"'");

	while ((line=ecfg.readln(65535)) != undefined) {
		m = line.match(/^\s*(?:secure_)?inbound\s+(.*)$/i);
		if (m !== null)
			this.inb.push(backslash(m[1]));

		m = line.match(/^\s*pktpwd\s+(.*?)\s+(.*)\s*$/i);
		if (m !== null)
			this.pktpass[m[1].toUpperCase()] = m[2].toUpperCase();

		m = line.match(/^\s*outbound\s+(.*?)\s*$/i);
		if (m !== null)
			this.outbound = m[1];

		m = line.match(/^\s*flo_mailer\s*$/i);
		if (m !== null)
			this.is_flo = true;
	}
	ecfg.close();
}
SBBSEchoCfg.prototype.get_pw = function(node)
{
	var n = node;

	while(n) {
		if (this.pktpass[n] !== undefined)
			return this.pktpass[n];
		if (n === 'ALL')
			break;
		if (n.indexOf('ALL') !== -1)
			n = n.replace(/[0-9]+[^0-9]ALL$/, 'ALL');
		else
			n = n.replace(/[0-9]+$/, 'ALL');
	}
	return undefined;
};
SBBSEchoCfg.prototype.match_pw = function(node, pw)
{
	var pktpw = this.get_pw(node);

	if (pktpw === undefined || pktpw == '') {
		if (pw === '' || pw === undefined)
			return true;
	}
	if (pw.toUpperCase() === pktpw.toUpperCase())
		return true;

	log(LOG_WARNING, "Incorrect password "+pw+" (expected "+pktpw+")");
	return false;
};

function TickITCfg() {
	this.gcfg = undefined;
	this.acfg = {};
	var tcfg = new File(system.ctrl_dir+'tickit.ini');
	var sects;
	var i;

	function lcprops(obj)
	{
		var i;
		var keys = Object.keys(obj);

		for (i=0; i<keys.length; i++) {
			if (keys[i].toLowerCase() !== keys[i]) {
				if (obj[keys[i].toLowerCase()] === undefined)
					obj[keys[i].toLowerCase()] = obj[keys[i]];
				delete obj[keys[i]];
				if (typeof(obj[keys[i].toLowerCase()]) == 'Object')
					lcprops(obj[keys[i].toLowerCase()]);
			}
		}
	}

	if (!tcfg.open("r"))
		throw("Unable to open '"+tcfg.name+"'");
	this.gcfg = tcfg.iniGetObject();
	lcprops(this.gcfg);
	sects = tcfg.iniGetSections();
	for (i=0; i<sects.length; i++) {
		this.acfg[sects[i].toLowerCase()] = tcfg.iniGetObject(sects[i]);
		lcprops(this.acfg[sects[i].toLowerCase()]);
	}
	tcfg.close();
}
TickITCfg.prototype.cset = '0123456789abcdefghijklmnopqrstuvwxyz0123456789-_';
TickITCfg.prototype.basefn_to_num = function(num)
{
	var base = this.cset.length;
	var part;
	var ret=0;
	var i;

	for (i=0; i<num.length; i++) {
		ret *= base;
		ret += this.cset.indexOf(num[i]);
	}
	return ret;
};
TickITCfg.prototype.num_to_basefn = function(num)
{
	var base = this.cset.length;
	var part;
	var ret='';

	while(num) {
		part = num % base;
		ret = this.cset.charAt(part) + ret;
		num = parseInt((num/base), 10);
	}
	return ret;
};
/*
 * Returns a filename in the format "ti_XXXXX.tic" where
 * XXXXX is replaced by an incrementing base-48 number.
 * This provides 254,803,967 unique filenames.
 */
TickITCfg.prototype.get_next_tic_filename = function()
{
	var f;
	var val;
	var ret;

	// Get previous value or -1 if there is no previous value.
	f = new File(system.data_dir+'tickit.seq');

	if (f.open("r+b")) {
		val = f.readBin();
	}
	else {
		if (!f.open("web+")) {
			log(LOG_ERROR, "Unable to open file "+f.name+"!");
			return undefined;
		}
		val = -1;
	}

	// Increment by 1...
	val++;

	// Check for wrap...
	if (val > this.basefn_to_num('_____'))
		val = 0;

	// Write back new value.
	f.truncate();
	f.writeBin(val);
	f.close();

	// Left-pad to five digits.
	ret = ('00000'+this.num_to_basefn(val)).substr(-5);

	// Add pre/suf-fix
	return 'ti_'+ret+'.tic';
};
TickITCfg.prototype.save = function()
{
	var tcfg = new File(system.ctrl_dir+'tickit.ini');
	var sects;
	var i;
	var j;

	function writesect(section, obj) {
		if (obj.links === undefined)
			tcfg.iniRemoveKey(section, 'Links');
		else
			tcfg.iniSetValue(section, 'Links', obj.links);

		if (obj.dir === undefined)
			tcfg.iniRemoveKey(section, 'Dir');
		else
			tcfg.iniSetValue(section, 'Dir', obj.dir);

		if (obj.path === undefined)
			tcfg.iniRemoveKey(section, 'Path');
		else
			tcfg.iniSetValue(section, 'Path', obj.path);
	}

	if (!tcfg.open(tcfg.exists ? 'r+':'w+'))
		throw("Unable to open '"+tcfg.name+"'");

	writesect(null, this.gcfg);
	sects = tcfg.iniGetSections().map(function(v){return v.toLowerCase();});
	for (i in this.acfg) {
		writesect(i.toUpperCase(), this.acfg[i]);
		while ((j=sects.indexOf(i.toLowerCase())) >= 0) {
			sects.splice(j, 1);
		}
	}
	for (i=0; i<sects.length; i++)
		tcfg.iniRemoveSection(sects[i]);
	tcfg.close();
};

function FREQITCfg()
{
	var f=new File(system.ctrl_dir+'freqit.ini');
	var val;
	var i;
	var key;
	var sects;

	if (!f.open('r')) {
		log(LOG_ERROR, "Unable to open '"+f.name+"'");
		return;
	}
	// TODO: Support requiring passwords for specific files/dirs
	this.dirs = [];
	this.securedirs = [];
	this.magic = {};
	val = f.iniGetValue(null, 'Dirs');
	if (val != undefined)
		this.dirs = val.toLowerCase().split(/,/);
	val = f.iniGetValue(null, 'SecureDirs');
	if (val != undefined)
			this.securedirs = val.toLowerCase().split(/,/);
	this.maxfiles=f.iniGetValue(null, 'MaxFiles', 10);
	sects = f.iniGetSections();
	for (i=0; i<sects.length; i++) {
		key = sects[i];
		var dir = f.iniGetValue(key, 'Dir');

		if (dir == undefined) {
			log(LOG_ERROR, "Magic value '"+key+"' without a dir configured");
			return;
		}
		this.magic[key] = {};
		this.magic[key].dir=dir;
		this.magic[key].match=f.iniGetValue(key, 'Match', '*');
		this.magic[key].secure=f.iniGetValue(key, 'Secure', 'No');
		switch(this.magic[key].secure.toUpperCase()) {
			case 'TRUE':
			case 'YES':
			case 'ON':
				this.magic[key].secure = true;
				break;
			default:
				if (parseInt(this.magic[key].secure, 10)) {
					this.magic[key].secure = true;
					break;
				}
				this.magic[key].secure = false;
		}
	}
	f.close();
}
FREQITCfg.prototype.save = function()
{
	var fcfg = new File(system.ctrl_dir+'freqit.ini');
	var sects;
	var i;
	var j;

	function writesect(section, obj) {
		if (obj.dir === undefined)
			fcfg.iniRemoveKey(section, 'Dir');
		else
			fcfg.iniSetValue(section, 'Dir', obj.dir);

		if (obj.secure === undefined)
			fcfg.iniRemoveKey(section, 'Secure');
		else
			fcfg.iniSetValue(section, 'Secure', obj.secure ? 'Yes' : 'No');

		if (obj.match === undefined)
			fcfg.iniRemoveKey(section, 'Match');
		else
			fcfg.iniSetValue(section, 'Match', obj.match);
	}

	if (!fcfg.open(fcfg.exists ? 'r+':'w+'))
		throw("Unable to open '"+fcfg.name+"'");

	if (this.dirs.length > 0)
		fcfg.iniSetValue(null, 'Dirs', this.dirs.join(','));
	else
		fcfg.iniRemoveKey(null, 'Dirs');
	if (this.securedirs.length > 0)
		fcfg.iniSetValue(null, 'SecureDirs', this.securedirs.join(','));
	else
		fcfg.iniRemoveKey(null, 'SecureDirs');
	fcfg.iniSetValue(null, 'MaxFiles', this.maxfiles.toString());

	sects = fcfg.iniGetSections().map(function(v){return v.toLowerCase();});
	for (i in this.magic) {
		writesect(i, this.magic[i]);
		while ((j=sects.indexOf(i.toLowerCase())) >= 0)
			sects.splice(j, 1);
	}
	for (i=0; i<sects.length; i++)
		fcfg.iniRemoveSection(sects[i]);
	fcfg.close();
};
