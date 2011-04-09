if(js.global.get_geoip==undefined)
	js.global.load(js.global,"geoip.js");

Bot_Commands["WHEREIS"] = new Bot_Command(0,false,false);
Bot_Commands["WHEREIS"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var i;
	var find;
	var lstr;
	var location;

	function get_nicklocation(nick)
	{
		var geo;

		try {
			var userhost=srv.users[nick.toUpperCase()].uh.replace(/^.*\@/,'');
			// If the hostname is not a FQDN, use the server name and replace the first element...
			if(userhost.indexOf('.')==-1)
				userhost += (srv.users[nick.toUpperCase()].servername.replace(/^[^\.]+\./,'.'));
			geo=get_geoip(userhost);
			if(geo.countryName=='Reserved') {
				userhost=srv.users[nick.toUpperCase()].servername
				geo=get_geoip(userhost);
			}
			return geo;
		}
		catch(e) {
			log("Error getting nick location for "+nick+": "+e);
		}
	}

	// Remove empty cmd args
	for(i=1; i<cmd.length; i++) {
		if(cmd[i].search(/^\s*$/)==0) {
			cmd.splice(i,1);
			i--;
		}
	}

	if (!cmd[1])
		find=onick;
	else if(cmd.length==2)
		find=cmd[1];
	else {
		srv.o(target,"Usage: whereis <nick>");
		return true;
	}

	location=get_nicklocation(find);
	if (location) {
		lstr=find+' is ';
		if(location.cityName=='')
			lstr += 'somewhere in ';
		else
			lstr += 'around '+location.cityName+', ';

		if(location.regionName!='')
			lstr += location.regionName+', ';

		if(location.countryName!='')
			lstr += location.countryName;
	}
	else {
		var usr = new User(system.matchuser(cmd[1]));

		if (typeof(usr)=='object' && usr.location)
			lstr = find+' is located at '+usr.location;
	}

	if(!lstr)
		lstr="Unable to locate "+find;

	srv.o(target, lstr);

	return true;
}
