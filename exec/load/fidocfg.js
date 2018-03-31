require('fido.js', 'FIDO');

/*
 * TickIT configuration object
 *
 * TickITCfg Properties:
 * gcfg{}		global config all keys converted to lower-case
 * acfg{}{}		per-address config objects all keys converted to lower-case
 * 				Each object supports 'Links', 'Dir', 'Path', and 'Handler'
 * 				properties.
 * 
 * A handler is a load() path to a script which must define a
 * Handle_TIC(tic, obj) method.  This method takes two arguments, the
 * tic object and the "this" context of the caller.  If Handle_TIC()
 * returns true, it is assumed to have performed all processing necessary,
 * so normal processing does not occur.  The tic.full_path property *MUST*
 * be updated to point to where an unmodified copy of the file can be
 * found to transferring to downlinks.  If no such copy can be kept, the
 * tic.seenby array can be stuffed with the contents of obj.tickit.gcfg.links
 * and obj.tickit.acfg[tic.area.toLowerCase()].links (if any) to prevent
 * sending to any of the configured links.  Failing to do this will result
 * in TIC files without the corresponding attachment being send to downlinks.
 * Further, the load file must not have a null last statement.
 * 
 * cset			character set used in base-X file naming
 * 
 * TickITCfg Methods:
 * get_next_tick_filename()		returns a string representing the next
 * 								sequential unique filename for a tic file
 * basefn_to_num(str)			converts base-X string to an integer
 * num_to_basefn(num)			converts an integer to a base-X string
 * save()						save the configuration
 */
function TickITCfg() {
	this.gcfg = undefined;
	this.acfg = {};
	var tcfg = new File(system.ctrl_dir+'tickit.ini');
	var sects;
	var i;
	var tmp;

	this.cfgfile = tcfg.name;
	function get_bool(val) {
		if (val === undefined)
			return false;
		switch(val.toUpperCase()) {
		case 'YES':
		case 'TRUE':
		case 'ON':
			return true;
		}
		return false;
	}

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
	if (this.gcfg.handler !== undefined) {
		tmp = this.gcfg.handler;
		this.gcfg.handler = {};
		if (require(this.gcfg.handler, tmp, "Handle_TIC") == null)
			delete this.gcfg.handler;
	}
	sects = tcfg.iniGetSections();
	for (i=0; i<sects.length; i++) {
		this.acfg[sects[i].toLowerCase()] = tcfg.iniGetObject(sects[i]);
		lcprops(this.acfg[sects[i].toLowerCase()]);
		if (this.acfg[sects[i].toLowerCase()].handler !== undefined) {
			tmp = this.acfg[sects[i].toLowerCase()].handler;
			this.acfg[sects[i].toLowerCase()].handler = {};
			if (require(this.acfg[sects[i].toLowerCase()].handler, tmp, "Handle_TIC") == null)
				delete this.acfg[sects[i].toLowerCase()].handler;
		}
	}
	tcfg.close();
	this.gcfg.ignorepassword = get_bool(this.gcfg.ignorepassword);
	this.gcfg.secureonly = get_bool(this.gcfg.secureonly);
}
TickITCfg.prototype.cset = '0123456789abcdefghijklmnopqrstuvwxyz-_';
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

/*
 * FREQITCfg configuration object
 * 
 * FREQITCfg properties
 * dirs[]			Array of directories that can be FREQed from
 * securedirs[]		Array of seucrely FREQable directories
 * magic{}			Magic FREQ aliases
 * maxfiles			Max files per FREQ
 *
 * FREQITCfg methods
 * save()			Saves the freqit config to the INI file
 */
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
			f.close();
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

function BinkITCfg()
{
	var f=new File(file_cfgname(system.ctrl_dir, 'sbbsecho.ini'));
	var sects;

	this.node = {};
	if (!f.open('r')) {
		log(LOG_ERROR, "Unable to open '"+f.name+"'");
	}
	else {
		this.caps = f.iniGetValue('BinkP', 'Capabilities');
		this.sysop = f.iniGetValue('BinkP', 'Sysop', system.operator);
		sects = f.iniGetSections('node:');
		sects.forEach(function(section) {
			var addr = section.substr(5);
			try {
			var sec = new FIDO.parse_addr(section.substr(5).toLowerCase(), 1, 'fidonet');
			} catch(e) {
				return;		// Ignore addresses with wildcards (e.g. 'ALL')
			}
			this.node[sec] = {};
			this.node[sec].pass = f.iniGetValue(section, 'SessionPwd');
			this.node[sec].nomd5 = f.iniGetValue(section, 'BinkpAllowPlainAuth', false);
			this.node[sec].nocrypt = f.iniGetValue(section, 'BinkpAllowPlainText', false);
			this.node[sec].poll = f.iniGetValue(section, 'BinkpPoll', false);
			this.node[sec].port = f.iniGetValue(section, 'BinkpPort');
			this.node[sec].src = f.iniGetValue(section, 'BinkpSourceAddress');
			this.node[sec].host = f.iniGetValue(section, 'BinkpHost');
		}, this);
		f.close();
	}
}
