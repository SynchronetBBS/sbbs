if(js.global.get_geoip==undefined)
	js.global.load(js.global, "geoip.js");
if(js.global.get_nicklocation==undefined)
	js.global.load(js.global, "nicklocate.js");

function get_nickcoords(uh, sn, n) {
	var geo=get_nicklocation(uh, sn, n);
	if(!geo)
		geo = {latitude:0, longitude:0};
	var ret=geo.latitude+','+geo.longitude;
	if(ret=='0,0') {
		uh=sn;
		geo=get_geoip(uh);
		ret=geo.latitude+','+geo.longitude;
	}
	return ret;
}

