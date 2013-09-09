Bot_Commands["WEATHER"] = new Bot_Command(0,false,false);
Bot_Commands["WEATHER"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var i;
	var lstr;
	var nick=onick;

	// Remove empty cmd args
	for(i=1; i<cmd.length; i++) {
		if(cmd[i].search(/^\s*$/)==0) {
			cmd.splice(i,1);
			i--;
		}
	}

	cmd.shift();
	if(cmd[0])
		nick=cmd[0];
	if(srv.users[nick.toUpperCase()] == undefined) {
		srv.o(target, nick+', who the f^@% is '+nick+"?");
	}
	else {
		lstr=get_nickcoords(srv.users[nick.toUpperCase()].uh, srv.users[nick.toUpperCase()].servername, nick);
		if (!lstr) {
			var usr = new User(system.matchuser(nick));
			if (typeof(usr)=='object')
				lstr = usr.location;
		}
		if(!lstr && cmd[0])
			lstr=cmd.join(' ');
		var query = encodeURIComponent(lstr);

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

	}
	return true;
}

Bot_Commands["FORECAST"] = new Bot_Command(0,false,false);
Bot_Commands["FORECAST"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var i;
	var lstr;
	var nick=onick;

	// Remove empty cmd args
	for(i=1; i<cmd.length; i++) {
		if(cmd[i].search(/^\s*$/)==0) {
			cmd.splice(i,1);
			i--;
		}
	}
	
	cmd.shift();
	if(cmd[0])
		nick=cmd[0];
	lstr=get_nickcoords(srv.users[nick.toUpperCase()].uh, srv.users[nick.toUpperCase()].servername, nick);
	if (!lstr) {
		var usr = new User(system.matchuser(nick));
		if (typeof(usr)=='object')
			lstr = usr.location;
	}
	if(!lstr && cmd[0])
		lstr=cmd.join(' ');
	var query = encodeURIComponent(lstr);

	var location_url = "http://api.wunderground.com/auto/wui/geo/GeoLookupXML/index.xml?query=" + query;
	var forecast_url = "http://api.wunderground.com/auto/wui/geo/ForecastXML/index.xml?query=" + query;
	
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
			var Forecast = new XML((new HTTPRequest().Get(forecast_url)).replace(/<\?.*\?>[\r\n\s]*/,''));

			var str = Location.city + ", " + Location.state + " -";
			
			for(fd in Forecast.txt_forecast.forecastday) {
				var fcast=Forecast.txt_forecast.forecastday[fd];
				str += " " + fcast.title + ": " + fcast.fcttext;
			}
			str += ' (Provided by Weather Underground, Inc.)';

			var m;
			while(m=str.match(/^(.*?\. +)[^\.]+:/)) {
				srv.o(target, m[1]);
				str=str.substr(m[1].length);
			}
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
