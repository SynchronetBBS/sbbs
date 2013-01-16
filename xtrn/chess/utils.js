var COLOUR = {
	white:1,
	black:-1
};

var COLOUR_STR = {
	'1':'w',
	'-1':'b'
};

function parsePos(pos)
{
	var m=pos.match(/^([a-h])([1-8])$/);

	if(m==null)
		throw("Illegal position '"+pos+"' passed to parsePos()");

	var ret={};
	ret.x = m[1].charCodeAt(0)-96;
	ret.y = parseInt(m[2],10);
	return ret;
}

function makePos(pos)
{
	if(pos.x == null || pos.x < 1 || pos.x > 8)
		throw("Illegal location '"+pos.toSource()+"'");
	if(pos.y == null || pos.y < 1 || pos.y > 8)
		throw("Illegal location '"+pos.toSource()+"'");
	var ret=ascii(96+pos.x);
	ret += ascii(48+pos.y);
	return ret;
}

function parseColour(colour)
{
	if(colour=='w')
		return COLOUR.white;
	if(colour=='b')
		return COLOUR.black;
	throw("Illegal colour '"+colour+"' passed to parseColour");
}

function toward(from, to)
{
	if(from==to)
		return from;
	if(from > to)
		return from-1;
	return from+1;
}

function copyProps(from, to)
{
	for(prop in from) {
		if(from.__lookupGetter__(prop))
			to.__defineGetter__(prop, from.__lookupGetter__(prop));
		else
			to[prop]=from[prop];
	}
}
