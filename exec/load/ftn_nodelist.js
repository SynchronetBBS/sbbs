// Test with gatornet, agoranet, dorenet and fidonet
require("load/fido.js", 'FIDO');

function NodeList(filename, warn)
{
	var f = new File(filename);

	if (warn === undefined)
		warn = false;

	function Node(addr, hub) {
		this.addr = addr.str;
		this.hub = hub.str;
		this.private = this.hold = this.down = false;
	}
	Object.defineProperties(Node.prototype, {
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

	if (!f.open("r"))
		throw("Unable to open '"+f.name+"'.");

	// Validate first line...
	var line = f.readln(2048);
	if (line == undefined)
		throw("Unable to read first line in '"+f.name+"'");
	var m;
	if ((m=line.match(/^;A (.*) Nodelist for (.*) -- Day number ([0-9]+) : ([0-9]{5})$/)) !== null) {
		this.domain = m[1];
		this.date = m[2];	// TODO: Parse date into a Date()
		this.daynum = parseInt(m[3], 10);
		this.crc = parseInt(m[4], 10);
		f.rewind();
		// TODO: Must exclude the EOF character...
		/* if (this.crc !== f.crc16)
			log(LOG_WARNING, "CRC mismatch on '"+f.name+"'.  Got "+f.crc16+" expected "+this.crc);
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
	this.entries = [];

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
		entry = new Node(node, hub);
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
		this.entries.push(entry);
	}
}
