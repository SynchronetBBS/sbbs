if(js.global.get_geoip==undefined)
	js.global.load(js.global,"geoip.js");
if(js.global.get_nicklocation==undefined)
	js.global.load(js.global, "nicklocate.js");

Bot_Commands["WHEREIS"] = new Bot_Command(0,false,false);
Bot_Commands["WHEREIS"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var i;
	var find;
	var lstr;
	var location;

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

	if(srv.users[find.toUpperCase()] == undefined) {
		lstr=find+', who the f^@% is '+find+"?";
	}
	else {
		location=get_nicklocation(srv.users[find.toUpperCase()].uh, srv.users[find.toUpperCase()].servername, find);
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
	}

	srv.o(target, lstr);

	return true;
}
