// Find the correct/full internal code for the "syncdata" sub-board:
"use strict;"

function find(code)
{
	if(!code)
		code = "syncdata";
	// Find the actual sub-code
	for(var s in msg_area.sub) {
		if(s.substr(-code.length).toLowerCase() == code)
			return s;
	}
	return false;
}

this;
