if(!js.global || js.global.HTTPRequest==undefined)
	js.global.load("http.js");
if(!js.global || js.global.USCallsign==undefined)
	js.global.load("callsign.js");

Bot_Commands["SHOULD"] = new Bot_Command(0, false, false);
Bot_Commands["SHOULD"].command = function (target, onick, ouh, srv, lbl, cmd) {
	// Remove empty cmd args
	for(i=1; i<cmd.length; i++) {
		if(cmd[i].search(/^\s*$/)==0) {
			cmd.splice(i,1);
			i--;
		}
	}

	if(cmd.length == 1)
		return true;

	m=cmd.splice(1).join(" ").match(/^((?:(?:the|a|an|that|this|my|your|his|her|our|some|their|its|every|each|any)\s+)?.*?)\s+(,|.*\s+or\s+.*?)[\.\?]?$/i);

	if(m==null)
		return true;
	var a=m[2].split(/\s+or\s+|,\s*or\s+|\s*,\s*/i);
	var n=m[1];
	switch(n.toUpperCase()) {
	case 'I':
		n='you';
		break;
	case 'YOU':
		n='I';
		break;
	}
	srv.o(target, n+' should '+a[random(a.length)]);

	return true;
}
