var COLOUR = {
	white:1,
	black:-1
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
		to[prop]=from[prop];
	}
}
