require('fido_syscfg.js', 'FTNDomains');

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
 * 		inet_host	- Internet hostname at the binkp.net domain or listed
 * 					  in nodelist.
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
 * FIDO.parse_nodelist(path, warn[, domain])
 * 		Parses a nodelist and returns an object with an entries object in
 * 		it containing all the nodelist entries (4D address is the key
 * 		unless domain is specified in which case 5D address is the key).
 * 		If warn is true, will warn on "illegal" values (per FTS-0005).
 */

var FIDO = {
	FTNDomains:new FTNDomains(),
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
			if (this.FTNDomains.domainMap[parseInt(zone)] !== undefined)
				domain = this.FTNDomains.domainMap[parseInt(zone)];
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
		if (this.FTNDomains.domainMap[parseInt(zone, 16)] !== undefined)
			domain = this.FTNDomains.domainMap[parseInt(zone, 16)];
		return new FIDO.Addr(parseInt(m[2], 16), parseInt(m[3], 16), parseInt(zone, 16), parseInt(point, 16), domain);
	},
	Node:function(addr, hub) {
		this.addr = addr.str;
		this.hub = hub.str;
		this.private = this.hold = this.down = false;
	},
	parse_nodelist:function(filename, warn, domain)
	{
		var f = new File(filename);
		var ret = {};

		if (warn === undefined)
			warn = false;
		if (domain === undefined)
			domain = '';

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
		node.domain = domain;
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
	},
	PackedMessage:function(packet, offset)
	{
		this.packet = packet;
		this.offset = offset;

		this.topt = packet.packet_txt.search(/\r\n*\x01TOPT:/);
		if (this.topt > this.offset+this.length)
			this.topt = -1;

		this.fmpt = packet.packet_txt.search(/\r\n*\x01FMPT:/);
		if (this.fmpt > this.offset+this.length)
			this.fmpt = -1;

		this.intl = packet.packet_txt.search(/\r\n*\x01INTL:/);
		if (this.intl > this.offset+this.length)
			this.intl = -1;

		this.domain = packet.packet_txt.search(/\r\n*\x01DOMAIN /);
		if (this.domain > this.offset+this.length)
			this.domain = -1;

		this.fromUserOffset = this.offset + 34 + this.packet.getStr(this.offset+34, 36).length + 1;
		this.subjectOffset = this.fromUserOffset + this.packet.getStr(this.fromUserOffset, 36).length + 1;
		this.textOffset = this.subjectOffset + this.packet.getStr(this.subjectOffset, 36).length + 1;
		this.length = packet.packet_txt.indexOf('\x00', this.textOffset) - offset;
	},
	Packet:function(init)
	{
		this.type = FIDO.Packet.type.TWO;
		this.messages = [];

		if (init === undefined)
			init = FIDO.Packet.type.TWO_PLUS;

		if (init === undefined)
			init = FIDO.Packet.type.TWO_PLUS;
		if (typeof init === 'number') {
			this.type = init;
			this.packet_txt = '\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'+
						'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'+
						'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'+
						'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00';
			this.setBin(18, 2, 2);
			switch (this.type) {
				case FIDO.Packet.type.TWO_PLUS:
					this.setBin(44, 2, 1);
					this.setBin(40, 2, 0x0100);
					break;
				case FIDO.Packet.type.TWO_TWO:
					this.setBin(16, 2, 2);
					break;
			}
		}
		else {
			this.packet_txt = init;
			if (this.baud === 2)
				this.type = FIDO.Packet.type.TWO_TWO;
			else {
				if ((this.getBin(44, 1) & 0x7f) == (this.getBin(41, 1)) &&
						this.getBin(45, 1) == this.getBin(40, 1))
					this.type = FIDO.Packet.type.TWO_PLUS;
			}
			for (var offset = 58; ascii(this.packet_txt[offset]) !== 0; offset += this.messages[this.messages.length-1].length + 1) {
				this.messages.push(new FIDO.PackedMessage(this, offset));
			}
		}
	}
	/*
	 * Suggested kludge lines:
	 * INTL: (FTS-0001)
	 * TOPT: (FTS-0001)
	 * FMPT: (FTS-0001)
	 * TZUTC: (FTS-4008)
	 * PID: (FTS-
	 */
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

			// TODO: These don't need to be loaded into different objects since we're doing 5D
			if (FIDO.FTNDomains.nodeListFN[this.domain] !== undefined && file_exists(FIDO.FTNDomains.nodeListFN[this.domain])) {
				if (FIDO.FTNDomains.nodeList[this.domain] === undefined)
					FIDO.FTNDomains.nodeList[this.domain] = FIDO.parse_nodelist(FIDO.FTNDomains.nodeListFN[this.domain], false, this.domain);
				if (FIDO.FTNDomains.nodeList[this.str] !== undefined) {
					// TODO: Maybe support non-IBN stuff as well...
					ret = FIDO.FTNDomains.nodeList[this.str].binkp_host;
					if (ret !== undefined)
						return ret;
				}
			}

			if (this.point !== undefined)
				ret += format("p%d", this.point);
			ret += format("f%d.n%d.z%d.%s", this.node, this.net, this.zone, FIDO.FTNDomains.domainDNSMap[this.domain] === undefined ? '.example.com' : FIDO.FTNDomains.domainDNSMap[this.domain]);
			return ret;
		}
	},
	'binkp_port': {
		get: function() {
			// TODO: Use default zone from system.fido_addr_list[0]?
			if (this.zone === undefined)
				throw('zone is undefined');

			// TODO: These don't need to be loaded into different objects since we're doing 5D
			if (FIDO.FTNDomains.nodeListFN[this.domain] !== undefined && file_exists(FIDO.FTNDomains.nodeListFN[this.domain])) {
				if (FIDO.FTNDomains.nodeList[this.domain] === undefined)
					FIDO.FTNDomains.nodeList[this.domain] = FIDO.parse_nodelist(FIDO.FTNDomains.nodeListFN[this.domain], false, this.domain);
				if (FIDO.FTNDomains.nodeList[this.str] !== undefined)
					return FIDO.FTNDomains.nodeList[this.str].binkp_port;
			}

			return 24554;
		}
	},
});
FIDO.Packet.type = {
	TWO:0,		// FTS-0001
	TWO_PLUS:1,	// FSC-0039, FSC-0048
	TWO_TWO:2,	// FSC-0045
};
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
					return this.flags.IBN.replace(/\.$/,'');
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
				return this.flags[iflags[i]].replace(/\.$/,'');
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
FIDO.Packet.prototype.setStr = function(offset, len, val) {
	var str = val;
	while (str.length < len)
		str += '\x00';
	this.packet_txt = this.packet_txt.substr(0, offset)+str+this.packet_txt.substr(offset+len);
};
FIDO.Packet.prototype.getStr = function(offset, len) {
	var ret = this.packet_txt.substr(offset, len);
	var term = ret.indexOf('\x00');
	return ret.substr(0, term);
};
FIDO.Packet.prototype.setBin = function(offset, len, val) {
	var i;
	var str = '';

	if (typeof(val) !== 'number')
		throw('Invalid setBin value type');
	for (i=0; i<len; i++) {
		str += ascii(val & 0xff);
		val >>= 8;
	}
	this.packet_txt = this.packet_txt.substr(0, offset)+str+this.packet_txt.substr(offset+len);
};
FIDO.Packet.prototype.getBin = function(offset, len) {
	var ret = 0;
	var i;

	for (i=0; i<len; i++)
		ret |= (ascii(this.packet_txt[offset+i]) << (i*8));
	return ret;
};
FIDO.Packet.prototype.adjust_offsets = function(offset, adjust) {
	var i;

	for (i=0; i<this.messages.length; i++) {
		if (this.messages[i].offset >= offset) {
			this.messages[i].offset += offset;
			this.messages[i].fromUserOffset += adjust;
			this.messages[i].subjectOffset += adjust;
			this.messages[i].textOffset += adjust;
		}
	}
};
Object.defineProperties(FIDO.Packet.prototype, {
	'origNode': {
		get: function() { return this.getBin(0, 2); },
		set: function(val) { return this.setBin(0, 2, val); }
	},
	'destNode': {
		get: function() { return this.getBin(2, 2); },
		set: function(val) { return this.setBin(2, 2, val); }
	},
	'year': {
		get: function() {
			if (this.type === FIDO.Packet.type.TWO_TWO)
				return undefined;
			return this.getBin(4, 2);
		},
		set: function(val) {
			if (this.type === FIDO.Packet.type.TWO_TWO)
				return;
			return this.setBin(4, 2, val);
		}
	},
	'month': {
		get: function() {
			if (this.type === FIDO.Packet.type.TWO_TWO)
				return undefined;
			return this.getBin(6, 2);
		},
		set: function(val) {
			if (this.type === FIDO.Packet.type.TWO_TWO)
				return;
			return this.setBin(6, 2, val);
		}
	},
	'day': {
		get: function() {
			if (this.type === FIDO.Packet.type.TWO_TWO)
				return undefined;
			return this.getBin(8, 2);
		},
		set: function(val) {
			if (this.type === FIDO.Packet.type.TWO_TWO)
				return;
			return this.setBin(8, 2, val);
		}
	},
	'hour': {
		get: function() {
			if (this.type === FIDO.Packet.type.TWO_TWO)
				return undefined;
			return this.getBin(10, 2);
		},
		set: function(val) {
			if (this.type === FIDO.Packet.type.TWO_TWO)
				return;
			return this.setBin(10, 2, val);
		}
	},
	'minute': {
		get: function() {
			if (this.type === FIDO.Packet.type.TWO_TWO)
				return undefined;
			return this.getBin(12, 2);
		},
		set: function(val) {
			if (this.type === FIDO.Packet.type.TWO_TWO)
				return;
			return this.setBin(12, 2, val);
		}
	},
	'second': {
		get: function() {
			if (this.type === FIDO.Packet.type.TWO_TWO)
				return undefined;
			return this.getBin(14, 2);
		},
		set: function(val) {
			if (this.type === FIDO.Packet.type.TWO_TWO)
				return;
			return this.setBin(14, 2, val);
		}
	},
	'baud': {
		get: function() {
			if (this.type === FIDO.Packet.type.TWO_TWO)
				return undefined;
			return this.getBin(16, 2);
		},
		set: function(val) {
			if (this.type === FIDO.Packet.type.TWO_TWO)
				return;
			return this.setBin(16, 2, val);
		}
	},
	'pktVer': {
		get: function() { return this.getBin(18, 2); },
		set: function(val) { return this.setBin(18, 2, val); }
	},
	'origNet': {
		get: function() { return this.getBin(20, 2); },
		set: function(val) { return this.setBin(20, 2, val); }
	},
	'destNet': {
		get: function() { return this.getBin(22, 2); },
		set: function(val) { return this.setBin(22, 2, val); }
	},
	'prodCode': {
		get: function() {
			switch (this.type) {
				case FIDO.Packet.type.TWO:
				case FIDO.Packet.type.TWO_TWO:
					return this.getBin(24, 1);
				case FIDO.Packet.type.TWO_PLUS:
					return this.getBin(24, 1) | (this.getBin(42, 1) << 8);
			}
		},
		set: function(val) {
			switch (this.type) {
				case FIDO.Packet.type.TWO:
				case FIDO.Packet.type.TWO_TWO:
					this.setBin(24, 1, val);
					break;
				case FIDO.Packet.type.TWO_PLUS:
					this.setBin(24, 1, val & 0xff);
					this.setBin(42, 1, val >> 8);
					break;
			}
		},
	},
	'serialNo': {
		get: function() { return this.getBin(25, 2); },
		set: function(val) { return this.setBin(25, 2, val); }
	},
	'prodRevision': {
		get: function() { return this.getBin(25, 2); },
		set: function(val) { return this.setBin(25, 2, val); }
	},
	'prodMinorVersion': {
		get: function() {
			if (this.type === FIDO.Packet.type.TWO_PLUS)
				return this.getBin(43, 1);
			return undefined;
		},
		set: function(val) {
			if (this.type === FIDO.Packet.type.TWO_PLUS)
				this.setBin(43, 1, val);
		}
	},
	'password': {
		get: function() { return this.getStr(26, 8); },
		set: function(val) { return this.setStr(26, 8, val); }
	},
	'origZone': {
		get: function() {
			switch (this.type) {
				case FIDO.Packet.type.TWO:
				case FIDO.Packet.type.TWO_TWO:
					return this.getBin(34, 2);
				case FIDO.Packet.type.TWO_PLUS:
					var zone = this.getBin(46, 2);
					if (zone == 0) {
						zone = this.getBin(34, 2);
						this.setBin(46, 2, zone);
					}
					return zone;
			}
		},
		set: function(val) {
			switch (this.type) {
				case FIDO.Packet.type.TWO:
				case FIDO.Packet.type.TWO_TWO:
					this.setBin(34, 2, val);
					break;
				case FIDO.Packet.type.TWO_PLUS:
					this.setBin(34, 2, val);
					this.setBin(46, 2, val);
					break;
			}
		},
	},
	'destZone': {
		get: function() {
			switch (this.type) {
				case FIDO.Packet.type.TWO:
				case FIDO.Packet.type.TWO_TWO:
					return this.getBin(36, 2);
				case FIDO.Packet.type.TWO_PLUS:
					var zone = this.getBin(48, 2);
					if (zone == 0) {
						zone = this.getBin(36, 2);
						this.setBin(48, 2, zone);
					}
					return zone;
			}
		},
		set: function(val) {
			switch (this.type) {
				case FIDO.Packet.type.TWO:
				case FIDO.Packet.type.TWO_TWO:
					this.setBin(36, 2, val);
					break;
				case FIDO.Packet.type.TWO_PLUS:
					this.setBin(36, 2, val);
					this.setBin(48, 2, val);
					break;
			}
		}
	},
	'origPoint': {
		get: function() {
			switch (this.type) {
				case FIDO.Packet.type.TWO_TWO:
					return this.getBin(4, 2);
				case FIDO.Packet.type.TWO_PLUS:
					return this.getBin(50, 2);
			}
			return undefined;
		},
		set: function(val) {
			switch (this.type) {
				case FIDO.Packet.type.TWO_TWO:
					this.setBin(4, 2, val);
					break;
				case FIDO.Packet.type.TWO_PLUS:
					this.setBin(50, 2, val);
					break;
			}
		}
	},
	'destPoint': {
		get: function() {
			switch (this.type) {
				case FIDO.Packet.type.TWO_TWO:
					return this.getBin(6, 2);
				case FIDO.Packet.type.TWO_PLUS:
					return this.getBin(52, 2);
			}
			return undefined;
		},
		set: function(val) {
			switch (this.type) {
				case FIDO.Packet.type.TWO_TWO:
					this.setBin(6, 2, val);
					break;
				case FIDO.Packet.type.TWO_PLUS:
					this.setBin(52, 2, val);
					break;
			}
		}
	},
	'origDomain': {
		get: function() {
			if (this.type == FIDO.Packet.type.TWO_TWO)
				return this.getStr(38, 8);
			return undefined;
		},
		set: function(val) {
			if (this.type == FIDO.Packet.type.TWO_TWO)
				this.setStr(38, 8, val);
		}
	},
	'destDomain': {
		get: function() {
			if (this.type == FIDO.Packet.type.TWO_TWO)
					return this.getStr(46, 8);
			return undefined;
		},
		set: function(val) {
			if (this.type == FIDO.Packet.type.TWO_TWO)
				this.setStr(46, 8, val);
		}
	},
	'prodData': {
		get: function() {
			switch (this.type) {
				case FIDO.Packet.type.TWO_PLUS:
				case FIDO.Packet.type.TWO_TWO:
					return this.getBin(54, 4);
			}
			return undefined;
		},
		set: function(val) {
			switch (this.type) {
				case FIDO.Packet.type.TWO_TWO:
				case FIDO.Packet.type.TWO_PLUS:
					this.setBin(54, 2, val);
					break;
			}
		}
	},
	'origAddr': {
		get: function() {
			return new FIDO.Addr(this.origNet, this.origNode, this.origZone, this.origPoint, this.origDomain);
		},
		set: function(val) {
			var addr = new FIDO.Addr(val);
			this.origNet = addr.net;
			this.origNode = addr.node;
			this.origZone = addr.zone;
			this.origPoint = addr.point;
			this.origDomain = addr.domain;
		}
	},
	'destAddr': {
		get: function() {
			return new FIDO.Addr(this.destNet, this.destNode, this.destZone, this.destPoint, this.destDomain);
		},
		set: function(val) {
			var addr = new FIDO.Addr(val);
			this.destNet = addr.net;
			this.destNode = addr.node;
			this.destZone = addr.zone;
			this.destPoint = addr.point;
			this.destDomain = addr.domain;
		}
	},
});
Object.defineProperties(FIDO.PackedMessage.prototype, {
	'origNode': {
		get: function() { return this.packet.getBin(this.offset+2, 2); },
		set: function(val) { return this.packet.setBin(this.offset+2, 2, val); }
	},
	'destNode': {
		get: function() { return this.packet.getBin(this.offset+4, 2); },
		set: function(val) { return this.packet.setBin(this.offset+4, 2, val); }
	},
	'origNet': {
		get: function() { return this.packet.getBin(this.offset+6, 2); },
		set: function(val) { return this.packet.setBin(this.offset+6, 2, val); }
	},
	'destNet': {
		get: function() { return this.packet.getBin(this.offset+8, 2); },
		set: function(val) { return this.packet.setBin(this.offset+8, 2, val); }
	},
	'Attribute': {
		get: function() { return this.packet.getBin(this.offset+10, 2); },
		set: function(val) { return this.packet.setBin(this.offset+10, 2, val); }
	},
	'cost': {
		get: function() { return this.packet.getBin(this.offset+12, 2); },
		set: function(val) { return this.packet.setBin(this.offset+12, 2, val); }
	},
	'DateTime': {
		get: function() { return this.packet.getStr(this.offset+14, 20); },
		set: function(val) { return this.packet.setStr(this.offset+14, 20, val); }
	},
	'toUserName': {
		get: function() { return this.packet.getStr(this.offset+34, 36); },
		set: function(val) {
			var adjust = val.length - this.toUserName.length;

			this.packet.packet_txt = this.packet.packet_txt.substr(0, this.offset+34)+val+'\x00'+this.packet.packet_txt.substr(this.fromUserOffset);
			this.length += adjust;
			this.fromUserOffset += adjust;
			this.subjectOffset += adjust;
			this.testOffset += adjust;
			this.packet.adjust_offsets(this.offset+this.length, adjust);
		}
	},
	'fromUserName': {
		get: function() { return this.packet.getStr(this.fromUserOffset, 36); },
		set: function(val) {
			var adjust = val.length - this.fromUserName.length;

			this.packet.packet_txt = this.packet.packet_txt.substr(0, this.fromUserOffset)+val+'\x00'+this.packet.packet_txt.substr(this.subjectOffset);
			this.length += adjust;
			this.subjectOffset += adjust;
			this.testOffset += adjust;
			this.packet.adjust_offsets(this.offset+this.length, adjust);
		}
	},
	'subject': {
		get: function() { return this.packet.getStr(this.subjectOffset, 72); },
		set: function(val) {
			var adjust = val.length - this.subject.length;

			this.packet.packet_txt = this.packet.packet_txt.substr(0, this.subjectOffset)+val+'\x00'+this.packet.packet_txt.substr(this.textOffset);
			this.length += adjust;
			this.testOffset += adjust;
			this.packet.adjust_offsets(this.offset+this.length, adjust);
		}
	},
	'text': {
		get: function() { return this.packet.packet_txt.substr(this.textOffset, this.length-(this.textOffset-this.offset)); },
		set: function(val) {
			var adjust = val.length - this.subject.length;

			this.packet.packet_txt = this.packet.packet_txt.substr(0, this.textOffset)+val+'\x00'+this.packet.packet_txt.substr(this.offset+this.length);
			this.length += adjust;
			this.packet.adjust_offsets(this.offset+this.length, adjust);
		}
	}
});
