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
 * 
 * FIDO.parse_addr(string, default_zone, default_domain)
 * 		Parses an address string filling in the default zone and domain
 * 		if necessary and returns a FIDO.Addr object.
 * 		I could have done a magic constructor that accepts this, but then
 * 		Magic would be involved and I don't really like magic.
 * 		
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
			domain = orig_domain.toString();
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
						domain = val.toString();
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

		m = addr.match(/^(?:([0-9]+):)?([0-9]+)\/([0-9]+)(?:\.([0-9]+))?(@.*)?$/);
		if (m===null)
			throw('invalid address '+addr);
		zone = m[1];
		domain = m[5];
		if (zone == undefined)
			zone = default_zone;
		if (domain == undefined)
			domain = default_domain;
		return new FIDO.Addr(m[2], m[3], zone, m[4], domain);
	},
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
			ret += format("f%d.n%d.z%d.binkp.net", this.node, this.net, this.zone);
			return ret;
		}
	}
});
FIDO.Addr.prototype.toString = function()
{
	return this.str;
};
FIDO.Addr.prototype.flo_outbound = function(default_zone)
{
	// backslash() doesn't work on an empty string
	var ret = '_';

	if (this.zone !== undefined && this.zone !== default_zone)
		ret += format(".%03x", this.zone);
	ret = backslash(ret);
	if (this.point !== undefined)
		ret += backslash(format("%08x.pnt", this.point));
	ret += format("%04x%04x.", this.net, this.node);
	return ret.substr(1);
};
