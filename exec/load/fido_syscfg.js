/*
 * Parse as much as needed from the SBBSEcho conifguration.
 * v2 uses sbbsecho.cfg and v3 uses sbbsecho.ini.
 *
 * SBBSEchoCfg Properties:
 * inbound			non-secure default inbound directory path
 *					TODO: SBBSEcho supports per-node inbound via fileboxes... a method to get the inbound for a node should exist
 * secure_inbound	secure inbound directory path
 *					TODO: SBBSEcho supports per-node inbound via fileboxes... a method to get the inbound for a node should exist
 * outbound			default oubound path, may end with a path separator
 *					TODO: SBBSEcho supports per-node outbound via fileboxes... a method to get the outbound for a node should exist
 * is_flow			boolean indicating it is a FLO style mailer... most things will require this to be true
 * pktpass{}		object with a separate property for each node addres (wildcards included as "ALL").
 *					Should be accessed using get_pw() and match_pw() methods.
 *
 * SBBSEchoCfg Methods:
 * get_pw(node)		node is a address string to look up a password for.  matches against wildcards.
 * match_pw(node, pw)	checks that the specified password string (pw) matches the password for the given node address (node).
 */
function SBBSEchoCfg ()
{
	var ecfg = new File(system.ctrl_dir+'sbbsecho.cfg');
	var line;
	var m;

	this.inb = [];
	this.pktpass = {};
	this.is_flo = false;
	this.outbound = undefined;

	if (!ecfg.open("r")) {
		ecfg = new File(system.ctrl_dir+'sbbsecho.ini');
		if (!ecfg.open("r"))
			throw("Unable to open '"+ecfg.name+"'");

		this.inbound = ecfg.iniGetValue(null, "Inbound", "../fido/nonsecure");
		if (this.inbound !== null)
			this.inb.push(this.inbound);
		this.secure_inbound = ecfg.iniGetValue(null, "SecureInbound", "../fido/inbound");
		if (this.secure_inbound !== null)
			this.inb.push(this.secure_inbound);
		this.outbound = ecfg.iniGetValue(null, "Outbound", "../fido/outbound");
		this.is_flo = ecfg.iniGetValue(null, "BinkleyStyleOutbound", false);
		ecfg.iniGetSections('node:').forEach(function(section) {
			this.pktpass[section.replace(/^node:/,'')] = ecfg.iniGetValue(section, 'PacketPwd', '');
		}, this);
	}
	else {
		while ((line=ecfg.readln(65535)) != undefined) {
			m = line.match(/^\s*(secure_|)inbound\s+(.*)$/i);
			if (m !== null) {
				this.inb.push(backslash(m[2]));
				this[m[1].toLowerCase()+'inbound'] = m[2];
			}

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
		return false;
	}
	if (pw.toUpperCase() === pktpw.toUpperCase())
		return true;

	log(LOG_WARNING, "Incorrect password "+pw+" (expected "+pktpw+")");
	return false;
};

function FTNDomains()
{
	var f = new File(system.ctrl_dir+'ftn_domains.ini');
	var used_zones = {};
	var ecfg = new SBBSEchoCfg();

	if (f.open("r")) {
		this.domainMap = {};
		this.domainDNSMap = {};
		this.outboundMap = {};
		var domains = f.iniGetSections().forEach(function(domain) {
			var d = domain.toLowerCase().substr(0,8);
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
			this.outboundMap[d] = f.iniGetValue(domain, 'OutboundRoot', ecfg.outbound.replace(/[\\\/]$/, '')).replace(/[\\\/]$/, '');
		}, this);
		f.close();
	}
	else {
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
	}
}
