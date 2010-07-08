Bot_Commands["WEATHER"] = new Bot_Command(0,false,false);
Bot_Commands["WEATHER"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var i;
	var lstr;

	// Remove empty cmd args
	for(i=1; i<cmd.length; i++) {
		if(cmd[i].search(/^\s*$/)==0) {
			cmd.splice(i,1);
			i--;
		}
	}

	if (!cmd[1]) {
		var GeoIP;
		var geoip_url='http://ipinfodb.com/ip_query2.php?ip='+ouh.replace(/^.*\@/,'')+'&timezone=false';

		try {
			GeoIP=new XML((new HTTPRequest().Get(geoip_url)).replace(/<\?.*\?>[\r\n\s]*/,''));
			if(GeoIP.Location[0].Latitude.length()==1 && GeoIP.Location[0].Longitude.length()==1)
				lstr=GeoIP.Location[0].Latitude+','+GeoIP.Location[0].Longitude;
		}
		catch (e) {};
	}
	if(cmd.length==2) {
		try {
			if(typeof(srv.users[cmd[1].toUpperCase()]) == 'object') {
				var userhost=srv.users[cmd[1].toUpperCase()].uh.replace(/^.*\@/,'');
				// If the hostname is not a FQDN, use the server name and replace the first element...
				if(userhost.indexOf('.')==-1)
					userhost += (srv.users[cmd[1].toUpperCase()].servername.replace(/^[^\.]+\./,'.'));

				var GeoIP;
				var geoip_url='http://ipinfodb.com/ip_query2.php?ip='+userhost+'&timezone=false';

				GeoIP=new XML((new HTTPRequest().Get(geoip_url)).replace(/<\?.*\?>[\r\n\s]*/,''));
				if(GeoIP.Location[0].Latitude.length()==1 && GeoIP.Location[0].Longitude.length()==1)
					lstr=GeoIP.Location[0].Latitude+','+GeoIP.Location[0].Longitude;
			}
		}
		catch (e) {
			srv.o(target, "Caught e: " + e);	
		};
	}

	var query = "";

	cmd.shift();
	if (!lstr) {
		var usr = new User(system.matchuser(cmd[1]));

		if (typeof(usr)=='object')
			lstr = usr.location;
	}
	if (!lstr)
		lstr = cmd.join(' ');
	query = encodeURIComponent(lstr);

	var weather_url = "http://api.wunderground.com/auto/wui/geo/WXCurrentObXML/index.xml?query=" + query;
	var location_url = "http://api.wunderground.com/auto/wui/geo/GeoLookupXML/index.xml?query=" + query;

	try {
		var Location = new XML((new HTTPRequest().Get(location_url)).replace(/<\?.*\?>[\r\n\s]*/,''));
	}
	catch(e) {
		srv.o(target, "Unable to resolve location "+lstr);
		return true;
	}

	switch(Location.location.length()) {
		case 0:
			if(Location.nearby_weather_stations.length()==0) {
				srv.o(target, "Unable to locate "+lstr);
				break;
			}
			// Fall-through
		case 1:
			var Weather = new XML((new HTTPRequest().Get(weather_url)).replace(/<\?.*\?>[\r\n\s]*/,''));

			var str = Weather.display_location.full;
			str += " - " + Weather.weather;
			str += ", "+ Weather.temp_c+" degrees ("+(parseInt(Weather.temp_c)+273)+"K, "+Weather.temp_f+"F)";
			str += " Wind "+Weather.wind_string;
			if(Weather.display_location.city != Weather.observation_location.city)
				str += " (Observed at: " + Weather.observation_location.city + ")";
			str += ' (Provided by Weather Underground, Inc.)';
			srv.o(target, str);
			break;
		default:
			srv.o(target, "Multiple matches for "+lstr);
			if(Location.location.length() > 7)
				srv.o(target, lstr+" matches "+Location.location.length()+" places... listing the first 7.");
			var outstr = "";
			var outcnt = 0;
			for (i in Location.location) {
				if (outcnt)
					outstr += "; ";
				outstr += Location.location[i].name;
				outcnt++;
				if (outcnt>=7)
					break;
			}
			srv.o(target, lstr+': '+outstr+'.');
			break;
	}

	return true;
}
