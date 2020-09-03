// $Id: fidoaddr.js,v 1.3 2020/03/15 02:13:18 rswindell Exp $

// Validate a numeric component of an FTN address
// FSP-1028: Zone/Net/Node/Point must be int 0 - 32767
function is_valid_numeric(n) {
  return (typeof n == 'number' && !isNaN(n) && n >= 0 && n <= 32767);
}

// Should parse anything except a 'node candidate' address
function parse(str)
{
  // FSP-1028: Domain must contain only a-z, 0-9, -, _, and ~
  // Ignores 'implementation note' re; 9 character ASCIIZ string for domain
  const re = /^(\d{1,5}):(\d{1,5})\/(\d{1,5})(\.(\d{1,5}))?(@([a-z0-9~_-]+))?$/;
  const addr = str.match(re);
  if (!addr) return false;
  const ret = {
    zone: parseInt(addr[1]),
    net: parseInt(addr[2]),
    node: parseInt(addr[3]),
    point: (typeof addr[5] == 'undefined' ? addr[5] : parseInt(addr[5])),
    domain: addr[7]
  };
  if (![ret.zone, ret.net, ret.node].every(is_valid_numeric)) return false;
  if (typeof ret.point != 'undefined' && !is_valid_numeric(ret.point)) {
    return false;
  }
  return ret;
}

function is_valid(str)
{
	if(!str || !str.length)
		return false;

	return parse(str) != false;
}

function to_inet(addr)
{
	if(typeof addr == "string")
		addr = parse(addr);
	if(!addr)
		return false;
	if(addr.point)
		return format("p%u.f%u.n%u.z%u", addr.point, addr.node, addr.net, addr.zone);
	return format("f%u.n%u.z%u", addr.node, addr.net, addr.zone);
}

function to_filename(addr)
{
	if(typeof addr == "string")
		addr = parse(addr);
	if(!addr)
		return false;
	return format("%04x%04x", addr.net, addr.node);
}

function to_str(addr)
{
	if(typeof addr == "string")
		return addr;

	if(addr.point)
		return format("%u:%u/%u.%u", addr.zone, addr.net, addr.node, addr.point);
	return format("%u:%u/%u", addr.zone, addr.net, addr.node);
}

this;
