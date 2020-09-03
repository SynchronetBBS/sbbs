// $Id: syncdata.js,v 1.1 2018/01/17 02:45:48 rswindell Exp $	
	
// Find the correct/full internal code for the "syncdata" sub-board:
function find(code)
{
	if(!code)
			code = "syncdata";
	// Find the actual sub-code
	var sub_code;
	for(var s in msg_area.sub) {
		var sub = msg_area.sub[s];
		if(sub.code.substr(-code.length).toLowerCase() == code)
			return sub.code;
	}
	return false;
}

this;