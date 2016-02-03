/*
 * Public stuff:
 * new FIDO.Addr(net, node, zone, point, domain)
 * 		Returns a FIDO.Addr object with the following properties:
 * 		net			- REQUIRED
 * 		node		- REQUIRED
 * 		zone
 * 		point
 * 		domain
 * 
 * 		These are type and value checked when assigned and throw an
 * 		exception when illegal.
 * 
 * 		inet_host	- Internet hostname at the binkp.net domain
 * 		str			- Address as a string in the most possible dimensions
 * 		toString()	- Overrides the default... same as str
 * 		flo_outbound(default_zone, default_domain)
 * 					- Generates the flo outbound path and filename from
 * 					  the end of the base outbound directory to the
 * 					  dot before the extension... generates something like:
 * 					  "/00670011." or ".001/00670011.pnt/00000001."
 * FIDO.parse_addr(string, default_zone, default_domain)
 * 		Parses an address string filling in the default zone and domain
 * 		if necessary and returns a FIDO.Addr object.
 * 		I could have done a magic constructor that accepts this, but then
 * 		Magic would be involved and I don't really like magic.
 * 		
 * FIDO.parse_flo_file_path(path, default_zone, domain)
 * 		Parses a flo filename and path into the returned FIDO.Addr object.
 * 
 * FIDO.parse_nodelist(path, warn)
 * 		Parses a nodelist and returns an object with an entries object in
 * 		it containing all the nodelist entries (3D address is the key).
 * 		If warn is true, will warn on "illegal" values (per FTS-0005).
 */

var FIDO = {
	Addr:function(orig_net, orig_node, orig_zone, orig_point, orig_domain)
	{
		var net = parseInt(orig_net, 10);
		var node = parseInt(orig_node, 10);
		var zone;
		var point;
		var domain;

		if (orig_zone !== undefined)
			zone = parseInt(orig_zone, 10);
		else
			zone = -1;
		// TODO: Use the system default zone in system.fido_addr_list[0]?
		if (orig_point !== undefined)
			point = parseInt(orig_point, 10);
		else
			point = 0;
		if (orig_domain !== undefined)
			domain = orig_domain.toString().toLowerCase().substr(0, 8);
		else
			domain = '';

		Object.defineProperties(this, {
			"net": {
				enumerable: true,
				get: function() { return net; },
				set: function(val) {
					net = parseInt(val, 10);
					if (typeof net !== 'number')
						throw('net is not a number!');
					if (net < 0 || net > 65535)
						throw('net out of range');
				}
			},
			"node": {
				enumerable: true,
				get: function() { return node; },
				set: function(val) {
					node = parseInt(val, 10);
					if (typeof node !== 'number')
						throw('node is not a number!');
					if (node < 0 || node > 65535)
						throw ('node out of range');
				}
			},
			"zone": {
				enumerable: true,
				get: function() {
					if (zone === -1)
						return undefined;
					return zone;
				},
				set: function(val) {
					if (val == undefined)
						zone = 0;
					else
						zone = parseInt(val, 10);
					if (typeof zone !== 'number')
						throw('zone is not a number!');
					if (zone < -1 || zone > 65535)
						throw('zone out of range');
				}
			},
			"point": {
				enumerable: true,
				get: function() {
					if (point === 0)
						return undefined;
					return point;
				},
				set: function(val) {
					if (val == undefined)
						point = 0;
					else
						point = parseInt(val, 10);
					if (typeof point !== 'number')
						throw('point is not a number!');
					if (point < 0 || point > 65535)
						throw ('point out of range');
				}
			},
			"domain": {
				enumerable: true,
				get: function() {
					if (domain === '')
						return undefined;
					return domain;
				},
				set: function(val) {
					if (val == undefined)
						domain = '';
					else
						domain = val.toString().toLowerCase().substr(0, 8);
					if (typeof domain !== 'string')
						throw('domain is not a string');
				}
			}
		});
	},
	parse_addr:function(addr, default_zone, default_domain)
	{
		var m;
		var zone;
		var domain;

		m = addr.toString().match(/^(?:([0-9]+):)?([0-9]+)\/([0-9]+)(?:\.([0-9]+))?(?:@(.*))?$/);
		if (m===null)
			throw('invalid address '+addr);
		zone = m[1];
		domain = m[5];
		if (zone == undefined)
			zone = default_zone;
		if (domain == undefined) {
			if (this.domainMap[parseInt(zone)] !== undefined)
				domain = this.domainMap[parseInt(zone)];
			else
				domain = default_domain;
		}
		return new FIDO.Addr(m[2], m[3], zone, m[4], domain);
	},
	parse_flo_file_path:function(path, default_zone, domain)
	{
		var m;
		var zone;
		var point;
		var ext;

		if (default_zone === undefined)
			throw("Default zone unspecified");
		m = path.match(/(?:\.([0-9a-f]{3,4})[\/\\])?([0-9a-f]{4})([0-9a-f]{4})\.(...)(?:[\/\\]([0-9a-f]{8})\.(...))?$/i);
		if (m === null)
			throw("Invalid flo file path");
		ext = m[4];
		if (m[5] != null) {
			if (m[4].toUpperCase() !== 'PNT')
				throw("Invalid flo file path");
			ext = m[6];
		}
		switch(ext.toLowerCase()) {
			case 'ilo':
			case 'clo':
			case 'dlo':
			case 'hlo':
			case 'flo':
			case 'iut':
			case 'cut':
			case 'dut':
			case 'hut':
			case 'out':
			case 'bsy':
			case 'csy':
			case 'try':
				break;
			default:
				throw("Invalid flo file path");
		}
		zone = m[1];
		if (zone == null)
			zone = default_zone;
		point = m[5];
		if(point == null)
			point = 0;
		if (this.domainMap[parseInt(zone)] !== undefined)
			domain = this.domainMap[parseInt(zone, 10)];
		return new FIDO.Addr(parseInt(m[2], 16), parseInt(m[3], 16), parseInt(zone, 16), parseInt(point, 16), domain);
	},
	domainMap:{
		1:'fidonet',
		2:'fidonet',
		3:'fidonet',
		4:'fidonet',
		5:'fidonet',
		6:'fidonet',
	},
	domainDNSMap:{
		'fidonet':'binkp.net'
	},
	ReadDomainMap:function() {
		var f = new File(system.ctrl_dir+'ftn_domains.ini');
		var used_zones = {};

		if (f.open("r")) {
			this.domainMap = {};
			this.domainDNSMap = {};
			var domains = f.iniGetSections().forEach(function(domain) {
				var d = domain.toLowerCase();
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
			}, this);
			f.close();
		}
	},
	Node:function(addr, hub) {
		this.addr = addr.str;
		this.hub = hub.str;
		this.private = this.hold = this.down = false;
	},
	parse_nodelist:function(filename, warn)
	{
		var f = new File(filename);
		var ret = {};

		if (warn === undefined)
			warn = false;

		if (!f.open("r"))
			throw("Unable to open '"+f.name+"'.");

		// Validate first line...
		var line = f.readln(2048);
		if (line == undefined)
			throw("Unable to read first line in '"+f.name+"'");
		var m;
		if ((m=line.match(/^;A (.*) Nodelist for (.*) -- Day number ([0-9]+) : ([0-9]{5})$/)) !== null) {
			ret.domain = m[1];
			ret.date = m[2];	// TODO: Parse date into a Date()
			ret.daynum = parseInt(m[3], 10);
			ret.crc = parseInt(m[4], 10);
			f.rewind();
			// TODO: Must exclude the EOF character...
			/* if (ret.crc !== f.crc16)
				log(LOG_WARNING, "CRC mismatch on '"+f.name+"'.  Got "+f.crc16+" expected "+ret.crc);
			*/
			f.rewind();
		}
		else
			f.rewind();

		var hub = new FIDO.Addr('0:0/0');
		var node = new FIDO.Addr('0:0/0');
		var fields;
		var lineno = 0;
		var entry;
		var flags;
		var flag;
		var value;
		ret.entries = {};

		while ((line = f.readln(2048))) {
			lineno++;
			if (line[0] === ';')
				continue;
			fields = line.split(/,/);
			switch(fields[0]) {
				case 'Zone':
					node.zone = parseInt(fields[1], 10);
					node.net = 0;
					node.node = 0;
					hub.zone = node.zone;
					hub.net = 0;
					hub.node = 0;
					break;
				case 'Region':
					node.net = parseInt(fields[1], 10);
					node.node = 0;
					hub.net = node.net;
					hub.node = 0;
					break;
				case 'Host':
					node.net = parseInt(fields[1], 10);
					node.node = 0;
					hub.net = node.net;
					hub.node = 0;
					break;
				case 'Hub':
					hub.node = parseInt(fields[1], 10);
					node.node = parseInt(fields[1], 10);
					break;
				case 'Pvt':
					node.node = parseInt(fields[1], 10);
					break;
				case 'Hold':
					node.node = parseInt(fields[1], 10);
					break;
				case 'Down':
					node.node = parseInt(fields[1], 10);
					break;
				case '':
					node.node = parseInt(fields[1], 10);
					break;
				case '\x1a':	// EOF
					continue;
				default:
					if (warn)
						log(LOG_WARNING, "Unhandled nodelist Keyword line "+lineno+" of '"+f.name+"'");
					continue;
			}
			entry = new FIDO.Node(node, hub);
			switch(fields[0]) {
				case 'Pvt':
					entry.private = true;
					break;
				case 'Hold':
					entry.hold = true;
					break;
				case 'Down':
					entry.down = true;
					break;
			}
			entry.name = fields[2].replace(/_/g, ' ');
			entry.location = fields[3].replace(/_/g, ' ');
			entry.sysop = fields[4].replace(/_/g, ' ');
			entry.phone = fields[5];
			entry.baud = parseInt(fields[6]);
			if (warn) {
				switch(fields[6]) {
					case '300':
					case '1200':
					case '2400':
					case '9600':
					case '19200':
					case '38400':
						break;
					default:
						log(LOG_WARNING, "Illegal nodelist baud rate '"+fields[6]+"' line "+lineno+" of '"+f.name+"'");
				}
			}
			flags = fields.slice(7);
			entry.flags = {};
			while(flags.length > 0) {
				flag = flags.shift();
				if (warn) {
					if (flag.length > 32)
						log(LOG_WARNING, "Illegal nodelist flag '"+flag+"' longer than 32 characters");
				}
				if ((m = flag.match(/^(.*?):(.*)$/))) {
					flag = m[1];
					value = m[2];
					entry.flags[flag] = value;
				}
				else
					value = true;
				entry.flags[flag] = value;
			}
			ret.entries[entry.addr] = entry;
		}
		return ret;
	}
};
Object.defineProperties(FIDO.Addr.prototype, {
	'str': {
		get: function() {
			var ret = format("%d/%d", this.net, this.node);

			if (this.zone !== undefined)
				ret = format("%d:%s", this.zone, ret);
			if (this.point !== undefined && this.point > 0)
				ret = format("%s.%d", ret, this.point);
			if (this.domain !== undefined)
				ret = ret+'@'+this.domain;
			return ret;
		}
	},
	'inet_host': {
		get: function() {
			var ret = '';

			// TODO: Use default zone from system.fido_addr_list[0]?
			if (this.zone === undefined)
				throw('zone is undefined');

			if (this.point !== undefined)
				ret += format("p%d", this.point);
			ret += format("f%d.n%d.z%d.%s", this.node, this.net, this.zone, FIDO.domainDNSMap[this.domain] === undefined ? '.example.com' : FIDO.domainDNSMap[this.domain]);
			return ret;
		}
	}
});
FIDO.Addr.prototype.toString = function()
{
	return this.str;
};
FIDO.Addr.prototype.flo_outbound = function(default_zone, default_domain)
{
	// backslash() doesn't work on an empty string
	var ret = '_';

	/*
	 * We need the zone suffix if:
	 * 1) This is not in the default zone.
	 * -OR-
	 * 2) This is not in the default domain.
	 *
	 * If default_* is undefined, assume we are in the default.
	 * Of course, if we don't have a zone, we surely can't put it in.
	 */

	if (this.zone !== undefined) {
		if ((default_zone !== undefined && this.zone !== default_zone) ||
				(this.domain !== undefined && default_domain !== undefined && this.domain !== default_domain.toLowerCase()))
			ret += format(".%03x", this.zone);
	}
	ret = backslash(ret);
	if (this.point !== undefined)
		ret += backslash(format("%08x.pnt", this.point));
	ret += format("%04x%04x.", this.net, this.node);
	return ret.substr(1);
};
Object.defineProperties(FIDO.Node.prototype, {
	'binkp_host': {
		get: function() {
			var iflags = ['INA', 'IFC', 'ITN', 'IVM', 'IFT', 'IP'];
			var i;

			if (this.flags.IBN === undefined)
				return undefined;
			if (this.flags.IBN !== true) {
				// Address or port number included here...
				if (this.flags.IBN.search(/^[0-9]+$/) == -1) {
					if (this.flags.IBN.indexOf(':' !== -1))
						return this.flags.IBN.replace(/:.*$/,'');
					return this.flags.IBN;
				}
			}
			for (i in iflags) {
				if (this.flags[iflags[i]] === undefined)
					continue;
				if (this.flags[iflags[i]] === true)
					continue;
				if (this.flags[iflags[i]].search(/^[0-9]+$/) == 0)
					continue;
				if (this.flags[iflags[i]].indexOf(':') !== -1)
					return this.flags[iflags[i]].replace(/:.*$/,'');
				return this.flags[iflags[i]];
			}
			if (this.name.indexOf('.') !== -1)
				return this.name.replace(/ /, '_');
		}
	},
	'binkp_port': {
		get: function() {
			if (this.flags.IBN === undefined)
				return undefined;
			if (this.flags.IBN !== true) {
				// Address or port number included here...
				if (this.flags.IBN.search(/^[0-9]+$/) == -1) {
					if (this.flags.IBN.indexOf(':') === -1)
						return 24554;
					return parseInt(this.flags.IBN.replace(/^.*:([0-9]+)$/, '$1'));
				}
				return parseInt(this.flags.IBN, 10);
			}
			return 24554;
		}
	}
});

FIDO.ReadDomainMap();
