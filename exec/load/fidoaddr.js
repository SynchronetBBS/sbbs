// $Id$

function parse(str)
{
	var addr = str.match(/^(\d+):(\d+)\/(\d+)$/);				// 3D
	if(!addr)
		addr = str.match(/^(\d+):(\d+)\/(\d+)\.(\d+)$/);		// 4D
	if(!addr)
		return false;
	for(var i=1; i < addr.length; i++)
		if(parseInt(addr[i], 10) > 0xffff)
			return false;
	return { zone: addr[1], net: addr[2], node: addr[3], point: addr[4] };
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

this;