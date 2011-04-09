if(js.global.get_geoip==undefined)
	js.global.load(js.global, "geoip.js");

function get_nicklocation(srv,nick)
{
	var ret,geo;

	try {
		var userhost=srv.users[nick.toUpperCase()].uh.replace(/^.*\@/,'');
		// If the hostname is not a FQDN, use the server name and replace the first element...
		if(userhost.indexOf('.')==-1)
			userhost += (srv.users[cmd[1].toUpperCase()].servername.replace(/^[^\.]+\./,'.'));
		geo=get_geoip(userhost);
		ret=geo.latitude+','+geo.longitude;
		if(ret=='0,0') {
			userhost=srv.users[nick.toUpperCase()].servername
			geo=get_geoip(userhost);
			ret=geo.latitude+','+geo.longitude;
		}
		return ret;
	}
	catch(e) {
		log("ERROR: " + e);
	}
}
