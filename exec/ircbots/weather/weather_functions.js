if(js.global.get_geoip==undefined)
	js.global.load(js.global, "geoip.js");
if(js.global.get_nicklocation==undefined)
	js.global.load(js.global, "nicklocate.js");

function get_nickcoords(uh, sn, n) {
	var geo=get_nicklocation(uh, sn, n);
	var ret=geo.latitude+','+geo.longitude;
	if(ret=='0,0') {
		userhost=srvhost;
		geo=get_geoip(userhost);
		ret=geo.latitude+','+geo.longitude;
	}
	return ret;
}

