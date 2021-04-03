/* DNS protocol library
 * Uses events, so the calling script *MUST* set js.do_callbacks
 * Example usage:
 *
 * js.do_callbacks = true;
 * load('dns.js');
 *
 * function handle(resp)
 * {
 * 	log(LOG_ERROR, JSON.stringify(resp));
 * 	exit(0);
 * }
 *
 * dns.resolve('example.com', handle);
 *
 */

require('sockdefs.js', 'SOCK_DGRAM');

function DNS(servers) {
	if (servers === undefined)
		servers = system.name_servers;
	var nextid = 0;
	var outstanding = {};
	this.sockets = [];

	handle_response = function() {
		var resp;
		var id;
		var offset = 0;
		var queries;
		var answers;
		var nameservers;
		var arecords;
		var q;
		var i;
		var ret = {'queries':[], 'answers':[], 'nameservers':[], 'additional':[]};
		var rdata;
		var rdlen;
		var tmp;

		string_to_int16 = function(str) {
			return ((ascii(str[0])<<8) | (ascii(str[1])));
		}

		string_to_int32 = function(str) {
			return ((ascii(str[0])<<24) | (ascii(str[1]) << 16) | (ascii(str[1]) << 8) | (ascii(str[1])));
		}

		function get_string(resp, offset) {
			var len = ascii(resp[offset]);
			return {'len':len + 1, 'string':resp.substr(offset + 1, len)};
		}

		function get_name(resp, offset) {
			var len;
			var name = '';
			var ret = 0;
			var compressed = false;

			do {
				len = ascii(resp[offset]);
				if (!compressed)
					ret++;
				offset++;
				if (len > 63) {
					offset = ((len & 0x3f) << 8) | ascii(resp[offset]);
					if (!compressed)
						ret++;
					compressed = true;
				}
				else {
					if (!compressed)
						ret += len;
					if (name.length > 0 && len > 0)
						name += '.';
					name += resp.substr(offset, len);
					offset += len;
				}
			} while (len != 0);

			return {'len':ret, 'name':name};
		}

		function parse_rdata(type, resp, offset, len) {
			var tmp;
			var tmp2;
			var tmp3;
			var tmp4;

			switch(type) {
				case 1:	 // A
					return ascii(resp[offset]) + '.' +
					       ascii(resp[offset + 1]) + '.' +
					       ascii(resp[offset + 2]) + '.' +
					       ascii(resp[offset + 3]);
				case 2:   // NS
					return get_name(resp, offset).name;
				case 5:   // CNAME
					return get_name(resp, offset).name;
				case 6:   // SOA
					tmp = {};
					tmp2 = 0;
					tmp3 = get_name(resp, offset + tmp2);
					tmp.mname = tmp3.name;
					tmp2 += tmp3.len;
					tmp3 = get_name(resp, offset + tmp2);
					tmp.rname = tmp3.name;
					tmp2 += tmp3.len;
					tmp.serial = string_to_int32(resp.substr(offset + tmp2, 4));
					tmp2 += 4;
					tmp.refresh = string_to_int32(resp.substr(offset + tmp2, 4));
					tmp2 += 4;
					tmp.retry = string_to_int32(resp.substr(offset + tmp2, 4));
					tmp2 += 4;
					tmp.export = string_to_int32(resp.substr(offset + tmp2, 4));
					tmp2 += 4;
					tmp.minimum = string_to_int32(resp.substr(offset + tmp2, 4));
					tmp2 += 4;
					return tmp;
				case 11:  // WKS
					tmp = {};
					tmp.address = ascii(resp[offset]) + '.' +
					              ascii(resp[offset + 1]) + '.' +
					              ascii(resp[offset + 2]) + '.' +
					              ascii(resp[offset + 3]);
					tmp.protocol = ascii(resp[offset + 4]);
					tmp2 = 5;
					tmp.ports = [];
					while (tmp2 < len) {
						tmp3 = ascii(resp[offset + tmp2]);
						for (tmp4 = 0; tmp4 < 8; tmp4++) {
							if (tmp3 & (1 << tmp4))
								tmp.ports.push(8 * (tmp2 - 5) + tmp3);
						}
					}
					return tmp;
				case 12:  // PTR
					return get_name(resp, offset).name;
				case 13:  // HINFO
					tmp = get_string(resp, offset);
					return {'cpu':tmp.string, 'os':get_string(resp, offset + tmp.len).string};
				case 15:  // MX
					tmp = {};
					tmp.preference = string_to_int16(resp.substr(offset, 2));
					tmp.exchange = get_name(resp, offset + 2).name;
					return tmp;
				case 16:  // TXT
					tmp = [];
					tmp2 = 0;
					do {
						tmp3 = get_string(resp, offset + tmp2);
						tmp.push(tmp3.string);
						tmp2 += tmp3.len;
					} while (tmp2 < len);
					return tmp;
				case 28:  // AAAA
					return format("%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
					               ascii(resp[offset + 0]),  ascii(resp[offset + 1]),
					               ascii(resp[offset + 2]),  ascii(resp[offset + 3]),
					               ascii(resp[offset + 4]),  ascii(resp[offset + 5]),
					               ascii(resp[offset + 6]),  ascii(resp[offset + 7]), 
					               ascii(resp[offset + 8]),  ascii(resp[offset + 9]),
					               ascii(resp[offset + 10]), ascii(resp[offset + 11]),
					               ascii(resp[offset + 12]), ascii(resp[offset + 13]),
					               ascii(resp[offset + 14]), ascii(resp[offset + 15])
					             ).replace(/(0000:)+/, ':').replace(/(^|:)0{1,3}/g, '$1');
				case 33:  // SRV
					tmp = {};
					tmp.priority = string_to_int16(resp.substr(offset, 2);
					offset += 2;
					tmp.weight = string_to_int16(resp.substr(offset, 2);
					offset += 2;
					tmp.port = string_to_int16(resp.substr(offset, 2);
					offset += 2;
					tmp.target = get_name(resp, offset).name;
					return tmp;
				case 35:  // NAPTR
					tmp = {};
					tmp.order = string_to_int16(resp.substr(offset, 2);
					offset += 2;
					tmp.preference = string_to_int16(resp.substr(offset, 2);
					offset += 2;
					tmp2 = get_string(resp, offset);
					tmp.flags = tmp2.string;
					offset += tmp2.len;
					tmp2 = get_string(resp, offset);
					tmp.services = tmp2.string;
					offset += tmp2.len;
					tmp2 = get_string(resp, offset);
					tmp.regexp = tmp2.string;
					offset += tmp2.len;
					tmp.replacement = get_name(resp, offset).name;
					return tmp;
				case 256: // URI
					tmp = {};
					tmp.priority = string_to_int16(resp.substr(offset, 2);
					offset += 2;
					tmp.weight = string_to_int16(resp.substr(offset, 2);
					offset += 2;
					tmp.target = get_string(resp, offset).string;
					tmp.target = tmp.target.replace(/\\"/g, '"');
					tmp.target = tmp.target.replace(/^"(.*)"$/, '$1');
					return tmp;
				case 3:   // MD
				case 4:   // MF
				case 7:   // MB
				case 8:   // MG
				case 9:   // MR
				case 10:  // NULL
				case 14:  // MINFO
					return {'raw':resp.substr(offset, len)};
			}
		}

		resp = this.recv(10000);
		id = string_to_int16(resp);
		if (this.outstanding[id] === undefined)
			return false;

		q = this.outstanding[id];
		delete this.outstanding[id];

		ret.id = id;
		ret.response = !!(ascii(resp[2]) & 1);
		ret.opcode = (ascii(resp[2]) & 0x1e) >> 1;
		ret.authoritative = !!(ascii(resp[2]) & (1<<5));
		ret.truncation = !!(ascii(resp[2]) & (1<<6));
		ret.recusrion = !!(ascii(resp[2]) & (1<<7));
		ret.reserved = ascii(resp[3]) & 7;
		ret.rcode = ascii(resp[3] & 0xf1) >> 3;

		queries = string_to_int16(resp.substr(4, 2));
		answers = string_to_int16(resp.substr(6, 2));
		nameservers = string_to_int16(resp.substr(8, 2));
		arecords = string_to_int16(resp.substr(10, 2));
		if (answers === 0)
			return false;
		js.clearTimeout(q.timeoutid);
		offset = 12;
		for (i = 0; i < queries; i++) {
			rdata = {};
			tmp = get_name(resp, offset);
			rdata.name = tmp.name;
			offset += tmp.len;
			rdata.type = string_to_int16(resp.substr(offset, 2));
			offset += 2;
			rdata.class = string_to_int16(resp.substr(offset, 2));
			offset += 2;
			ret.queries.push(rdata);
		}

		for (i = 0; i < answers; i++) {
			rdata = {};
			tmp = get_name(resp, offset);
			rdata.name = tmp.name;
			offset += tmp.len;
			rdata.type = string_to_int16(resp.substr(offset, 2));
			offset += 2;
			rdata.class = string_to_int16(resp.substr(offset, 2));
			offset += 2;
			rdata.ttl = string_to_int32(resp.substr(offset, 4));
			offset += 4;
			rdlen = string_to_int16(resp.substr(offset, 2));
			offset += 2;
			rdata.rdata = parse_rdata(rdata.type, resp, offset, rdlen);
			offset += rdlen;
			ret.answers.push(rdata);
		}

		for (i = 0; i < nameservers; i++) {
			rdata = {};
			tmp = get_name(resp, offset);
			rdata.name = tmp.name;
			offset += tmp.len;
			rdata.type = string_to_int16(resp.substr(offset, 2));
			offset += 2;
			rdata.class = string_to_int16(resp.substr(offset, 2));
			offset += 2;
			rdata.ttl = string_to_int32(resp.substr(offset, 4));
			offset += 4;
			rdlen = string_to_int16(resp.substr(offset, 2));
			offset += 2;
			rdata.rdata = parse_rdata(rdata.type, resp, offset, rdlen);
			offset += rdlen;
			ret.nameservers.push(rdata);
		}

		for (i = 0; i < arecords; i++) {
			rdata = {};
			tmp = get_name(resp, offset);
			rdata.name = tmp.name;
			offset += tmp.len;
			rdata.type = string_to_int16(resp.substr(offset, 2));
			offset += 2;
			rdata.class = string_to_int16(resp.substr(offset, 2));
			offset += 2;
			rdata.ttl = string_to_int32(resp.substr(offset, 4));
			offset += 4;
			rdlen = string_to_int16(resp.substr(offset, 2));
			offset += 2;
			rdata.rdata = parse_rdata(rdata.type, resp, offset, rdlen);
			offset += rdlen;
			ret.additional.push(rdata);
		}

		q.callback.call(q.thisObj, ret);
		return true;
	}

	servers.forEach(function(server) {
		var sock = new Socket(SOCK_DGRAM, "dns", server.indexOf(':') >= 0);
		sock.bind();
		sock.connect(server, 53);
		sock.on('read', handle_response);
		sock.outstanding = outstanding;
		this.sockets.push(sock);
	}, this);

	if (this.sockets.length < 1)
		throw('Unable to create any sockets');

	increment_id = function() {
		var ret = nextid;
		do {
			nextid++;
			if (nextid > 65535)
				nextid = 0;
		} while (outstanding[nextid] !== undefined);
		return ret;
	}

}

DNS.types = {
	'A':1,
	'NS':2,
	'MD':3,
	'MF':4,
	'CNAME':5,
	'SOA':6,
	'MB':7,
	'MG':8,
	'MR':9,
	'NULL':10,
	'WKS':11,
	'PTR':12,
	'HINFO':13,
	'MINFO':14,
	'MX':15,
	'TXT':16,
	'AAAA':28,
	'SRV':33,
	'NAPTR':35,
	'URI':256
};

DNS.classes = {
	'IN':1,
	'CS':2,
	'CH':3,
	'HS':4
};

DNS.prototype.query = function(queries, /* queryStr, type, class, */callback, thisObj, recursive, timeout, failures, failed) {
	var id;
	var namebits = {};

	function int16_to_string(id) {
		return ascii((id & 0xff00) >> 8) + ascii(id & 0xff);
	}

	function handle_timeout() {
		this.failed++;

		delete outstanding[this.id];

		if (this.failed > 2)
			this.callback.call(this);
		else
			this.obj.query(this.query, this.callback, this.thisObj, this.recursive, this.timeout, this.failures, this.failed);
	}

	queries.forEach(function(query) {
		if (query.str === undefined)
			query.str = 'example.com';
		if (query.type === undefined)
			query.type = 'AAAA';
		if (DNS.types[query.type] !== undefined)
			query.type = DNS.types[query.type];
		query.type = parseInt(query.type, 10);
		if (isNaN(query.type)) {
			throw new Error('Invalid type');
		}
		if (query.class === undefined)
			query.class = 'IN';
		if (DNS.classes[query.class] !== undefined)
			query.class = DNS.classes[query.class];
		query.class = parseInt(query.class, 10);
		if (isNaN(query.class)) {
			throw new Error('Invalid class');
		}
	});

	if (callback === undefined)
		return;
	if (recursive === undefined)
		recursive = true;
	if (timeout === undefined)
		timeout = 1000;
	if (failures === undefined)
		failures = 1;
	if (failed === undefined)
		failed = 0;

	var query = '';

	// Header
	id = increment_id();
	query = int16_to_string(id);
	query += ascii(recursive ? 1 : 0);
	query += ascii(0);
	query += int16_to_string(queries.length);	// Questions
	query += int16_to_string(0);	// Answers
	query += int16_to_string(0);	// Name Servers
	query += int16_to_string(0);	// Additional Records

	// Queries
	queries.forEach(function(oneQuery) {
		var thisname = '';
		var fields = oneQuery.query.split(/\./).reverse();
		var matched;

		fields.forEach(function(field) {
			if (field.length > 63)
				throw new Error('invalid field in query "'+field+'" (longer than 63 characters)');
			thisname = ascii(field.length) + field + thisname;
			if (namebits[thisname] !== undefined && namebits[thisname] > 0) {
				matched = {'offset':namebits[thisname], 'length':thisname.length};
			}
			else {
				namebits[thisname] = 0 - thisname.length;
			}
		});

		// Now fix up negative namebits...
		Object.keys(namebits).forEach(function(name) {
			if (namebits[name] < 0) {
				namebits[name] = query.length + thisname.length + namebits[name];
			}
		});

		// And use the match
		if (matched !== undefined) {
			thisname = thisname.substr(0, thisname.length - matched.length);
			thisname = thisname + ascii(0xc0 | ((matched.offset & 0x3f) >> 8)) + ascii(matched.offset & 0xff);
		}
		else
			thisname += ascii(0);
		query += thisname;
		query += int16_to_string(oneQuery.type);
		query += int16_to_string(oneQuery.class);
	}, this);

	this.sockets[0].outstanding[id] = {'callback':callback, 'query':queries, 'recursive':recursive, 'timeout':timeout, 'thisObj':thisObj, 'id':id, 'obj':this, 'failed':failed};
	this.sockets[0].outstanding[id].timeoutid = js.setTimeout(handle_timeout, timeout, this.sockets[0].outstanding[id]);

	this.sockets.forEach(function(sock) {
		sock.write(query);
	});
}

DNS.prototype.resolve = function(host, callback, thisObj)
{
	var ctx = {A:{}, AAAA:{}, unique_id:'DNS.prototype.resolve'};
	var final;
	var respA;
	var respAAAA;

	this.sockets.forEach(function(sock) {
		ctx.unique_id += '.'+sock.local_port;
	});

	if (host === undefined)
		throw new Error('No host specified');

	if (thisObj === undefined)
		thisObj = this;

	ctx.callback = callback;
	ctx.thisObj = thisObj;

	ctx.final = js.addEventListener(ctx.unique_id+'.final', function() {
		var ret = [];
		this.AAAA.addrs.forEach(function(addr) {
			ret.push(addr);
		});
		this.A.addrs.forEach(function(addr) {
			ret.push(addr);
		});
		js.removeEventListener(this.final);
		js.removeEventListener(this.respA);
		js.removeEventListener(this.respAAAA);
		this.callback.call(this.thisObj, ret);
	});

	ctx.respA = js.addEventListener(ctx.unique_id+'.respA', function() {
		this.A.done = true;
		if (this.AAAA.done)
			js.dispatchEvent(this.unique_id + '.final', this);
	});

	ctx.respAAAA = js.addEventListener(ctx.unique_id+'.respAAAA', function() {
		this.AAAA.done = true;
		if (this.A.done)
			js.dispatchEvent(ctx.unique_id + '.final', this);
	});

	function handle_response(resp) {
		var rectype;
		switch(resp.queries[0].type) {
			case DNS.types.A:
				rectype = 'A';
				break;
			case DNS.types.AAAA:
				rectype = 'AAAA';
				break;
		};
		if (rectype === undefined)
			return;

		this[rectype].addrs = [];

		resp.answers.forEach(function(ans) {
			if (resp.queries[0].type != ans.type || resp.queries[0].class != ans.class)
				return;
			this[rectype].addrs.push(ans.rdata);
		}, this);
		js.dispatchEvent(this.unique_id + '.resp'+rectype, this);
	}

	this.query([{query:host, type:'AAAA'}], handle_response, ctx);
	this.query([{query:host, type:'A'}], handle_response, ctx);
}

DNS.prototype.resolveTypeClass = function(host, type, class, callback, thisObj)
{
	var ctx = {};
	var final;
	var respA;
	var respAAAA;

	this.sockets.forEach(function(sock) {
		ctx.unique_id += '.'+sock.local_port;
	});

	if (host === undefined)
		throw new Error('No host specified');

	if (type === undefined)
		throw new Error('No type specified');

	if (class === undefined)
		throw new Error('No class specified');

	if (callback === undefined)
		throw new Error('No callback specified');
	ctx.callback = callback;

	if (thisObj === undefined)
		thisObj = this;
	ctx.thisObj = thisObj;

	function handle_response(resp) {
		ret = [];
		if (resp !== undefined && resp.answers !== undefined) {
			resp.answers.forEach(function(ans) {
				if (resp.queries[0].type != ans.type || resp.queries[0].class != ans.class)
					return;
				ret.push(ans.rdata);
			});
		}
		this.callback.call(this.thisObj, ret);
	}

	this.query([{query:host, type:type, class:class}], handle_response, ctx);
}

DNS.prototype.reverse = function(ip, callback, thisObj)
{
	var qstr;
	var m;
	var a;
	var fillpos;

	if (ip === undefined)
		throw new Error('No IP specified');

	if (thisObj === undefined)
		thisObj = this;

	// Sure, this doesn't deal with terrible ipv4-mapped representations.  Suck it.
	m = ip.match(/^(?:::ffff:)?([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})$/i);
	if (m !== null) {
		// IPv4 Address
		if (parseInt(m[1], 10) > 255 || parseInt(m[2], 10) > 255 || parseInt(m[3], 10) > 255 || parseInt(m[4], 10) > 255)
			throw new Error('Malformed IP address '+ip);
		qstr = m[4] + '.' + m[3] + '.' + m[2] + '.' + m[1] + '.in-addr.arpa';
	}
	else {
		a = ip.split(/:/);
		if (a.length < 3 || a.length > 8)
			throw new Error('Malformed IP address '+ip);
		if (ip.search(/^[a-fA-F0-9:]+$/) != 0)
			throw new Error('Malformed IP address '+ip);
		a.forEach(function(piece, idx, arr) {
			if (piece !== '') {
				while (piece.length < 4)
					piece = '0'+piece;
			}
			arr[idx] = piece;
		});
		if (a[0] == '')
			a[0] = '0000';
		if (a[a.length - 1] == '')
			a[a.length] = '0000';
		while (a.length < 8) {
			fillpos = a.indexOf('');
			if (fillpos === -1)
				throw('Unable to expand IPv6 address');
			a.splice(fillpos, 0, '0000');
		}
		fillpos = a.indexOf('');
		if (fillpos != -1)
			a.splice(fillpos, 1, '0000');
		a.reverse();
		qstr = '';
		a.forEach(function(piece) {
			qstr += piece[3] + '.' + piece[2] + '.' + piece[1] + '.' + piece[0] + '.';
		});
		qstr += 'ip6.arpa';
	}

	this.resolveTypeClass(qstr, 'PTR', 'IN', callback, thisObj);
}

DNS.prototype.resolveMX = function(host, callback, thisObj)
{
	var ctx = {callback:callback};
	var qstr;
	var m;
	var a;
	var fillpos;

	if (host === undefined)
		throw new Error('No host specified');

	if (thisObj === undefined)
		thisObj = this;
	ctx.thisObj = thisObj;

	function handler(resp) {
		var ret = [];
		resp.sort(function(a, b) {
			return a.preference - b.preference;
		});
		resp.forEach(function(r) {
			ret.push(r.exchange);
		});
		this.callback.call(this.thisObj, ret);
	}

	this.resolveTypeClass(host, 'MX', 'IN', handler, ctx);
}
