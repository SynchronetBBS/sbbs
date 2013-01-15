function parsePos(pos)
{
	var m=pos.match(/^([a-h])([1-8])$/);

	if(m==null)
		throw("Illegal position '"+pos+"' passed to parsePos()");

	var ret={};
	ret.x = m[1].charCodeAt(0)-96;
	ret.y = m[2]-'0';
	return ret;
}

function parseColour(colour)
{
	if(colour=='w')
		return false;
	if(colour=='b')
		return true;
	throw("Illegal colour '"+colour+"' passed to parseColour");
}

