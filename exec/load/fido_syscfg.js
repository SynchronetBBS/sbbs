/*
 * Parse as much as needed from the SBBSecho configuration.
 * v3+ uses sbbsecho.ini.
 *
 * SBBSEchoCfg Properties:
 * inbound			non-secure default inbound directory path
 *					TODO: SBBSecho supports per-node inbound via fileboxes... a method to get the inbound for a node should exist
 * secure_inbound	secure inbound directory path
 *					TODO: SBBSecho supports per-node inbound via fileboxes... a method to get the inbound for a node should exist
 * outbound			default outbound path, may end with a path separator
 *					TODO: SBBSecho supports per-node outbound via fileboxes... a method to get the outbound for a node should exist
 * is_flow			boolean indicating it is a FLO style mailer... most things will require this to be true
 * pktpass{}		object with a separate property for each node address (wildcards included as "ALL").
 *					Should be accessed using get_pw() and match_pw() methods.
 *
 * SBBSEchoCfg Methods:
 * get_pw(node)		node is a address string to look up a password for.  matches against wildcards.
 * match_pw(node, pw)	checks that the specified password string (pw) matches the password for the given node address (node).
 */
function SBBSEchoCfg ()
{
	var line;
	var m;

	this.inb = [];
	this.pktpass = {};
	this.ticpass = {};
	this.packer = {};
	this.is_flo = false;
	this.outbound = undefined;
	var packer = undefined;

	ecfg = new File(file_cfgname(system.ctrl_dir, 'sbbsecho.ini'));
	if (!ecfg.open("r"))
		throw("Unable to open '"+ecfg.name+"'");

	this.inbound = backslash(ecfg.iniGetValue(null, "Inbound", "../fido/nonsecure"));
	if (this.inbound !== null)
		this.inb.push(this.inbound);
	this.secure_inbound = backslash(ecfg.iniGetValue(null, "SecureInbound", "../fido/inbound"));
	if (this.secure_inbound !== null)
		this.inb.push(this.secure_inbound);
	this.outbound = ecfg.iniGetValue(null, "Outbound", "../fido/outbound");
	this.is_flo = ecfg.iniGetValue(null, "BinkleyStyleOutbound", false);
	ecfg.iniGetSections('node:').forEach(function(section) {
		this.pktpass[section.replace(/^node:/,'')] = ecfg.iniGetValue(section, 'PacketPwd', '');
	}, this);
	ecfg.iniGetSections('node:').forEach(function(section) {
		this.ticpass[section.replace(/^node:/,'')] = ecfg.iniGetValue(section, 'TicFilePwd', '');
	}, this);
	ecfg.iniGetSections('archive:').forEach(function(packer) {
		this.packer[packer] = {};
		this.packer[packer].offset = ecfg.iniGetValue(packer, 'SigOffset', 0);
		this.packer[packer].sig = ecfg.iniGetValue(packer, 'Sig', '');
		this.packer[packer].pack = ecfg.iniGetValue(packer, 'Pack', '');
		this.packer[packer].unpack = ecfg.iniGetValue(packer, 'Unpack', '');
	}, this);

	ecfg.close();
}
SBBSEchoCfg.prototype.get_ticpw = function(node)
{
	var n = node;

	while(n) {
		if (this.ticpass[n] !== undefined)
			return this.ticpass[n];
		if (n === 'ALL')
			break;
		if (n.indexOf('ALL') !== -1)
			n = n.replace(/[0-9]+[^0-9]ALL$/, 'ALL');
		else
			n = n.replace(/[0-9]+$/, 'ALL');
	}
	return undefined;
};
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

SBBSEchoCfg.prototype.match_ticpw = function(node, pw)
{
	var ticpw = this.get_ticpw(node);

	if (ticpw === undefined || ticpw == '') {
		if (pw === '' || pw === undefined)
			return true;
		log(LOG_WARNING, "Configured TicFilePwd is empty, but TIC file has a password, node: " + node);
		return false;
	}
	if (pw === undefined || pw === '') {
		log(LOG_WARNING, "TicFilePwd ("+ticpw+") configured, but TIC file has no password, node: " + node);
		return false;
	}
	if (pw.toUpperCase() === ticpw.toUpperCase())
		return true;

	log(LOG_WARNING, "Incorrect TIC password "+pw+" (expected "+ticpw+"), node: " + node);
	return false;
};
SBBSEchoCfg.prototype.match_pw = function(node, pw)
{
	var pktpw = this.get_pw(node);

	if (pktpw === undefined || pktpw == '') {
		if (pw === '' || pw === undefined)
			return true;
		log(LOG_WARNING, "Configured packet password is empty, but packet has a password, node: " + node);
		return false;
	}
	if (pw === undefined || pw === '') {
		log(LOG_WARNING, "Configured packet password ("+pktpw+") configured, but packet has no password, node: " + node);
		return false;
	}
	if (pw.toUpperCase() === pktpw.toUpperCase())
		return true;

	log(LOG_WARNING, "Incorrect packet password "+pw+" (expected "+pktpw+"), node: " + node);
	return false;
};

function FTNDomains()
{
	var f = new File(file_cfgname(system.ctrl_dir, 'sbbsecho.ini'));
	var used_zones = {};
	var ecfg = new SBBSEchoCfg();
	var domains;

	if (f.open("r")) {
		this.domainMap = {};
		this.domainDNSMap = {};
		this.outboundMap = {};
		this.nodeListFN = {};
		this.nodeList = {};
		domains = f.iniGetSections("domain:").forEach(function(domain) {
			var d = domain.toLowerCase().substr(7);
			var zones = f.iniGetValue(domain, 'Zones', '');
			if (zones != undefined) {
				zones.split(/\s*,\s*/).forEach(function(zone) {
					var z = parseInt(zone, 10);

					if (isNaN(z))
						return;
					// Not a 1:1 mapping... delete from domainMap
					if (used_zones[z] !== undefined)
						delete this.domainMap[z];
					else {
						used_zones[z]=1;
						this.domainMap[parseInt(zone, 10)] = d;
					}
				}, this);
			}
			this.domainDNSMap[d] = f.iniGetValue(domain, 'DNSSuffix', 'example.com');
			this.nodeListFN[d] = f.iniGetValue(domain, 'NodeList', undefined);
			this.outboundMap[d] = f.iniGetValue(domain, 'OutboundRoot', ecfg.outbound.replace(/[\\\/]$/, '')).replace(/[\\\/]$/, '');
		}, this);
		f.close();
	}
	if(!domains) {
		this.outboundMap = {
			'fidonet':ecfg.outbound.replace(/[\\\/]$/, '')
		};
		this.domainMap={
			1:'fidonet',
			2:'fidonet',
			3:'fidonet',
			4:'fidonet',
			5:'fidonet',
			6:'fidonet'
		};
		this.domainDNSMap={
			'fidonet':'binkp.net'
		};
		this.nodeListFN = {};
		this.nodeList = {};
	}
}
