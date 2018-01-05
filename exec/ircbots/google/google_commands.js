Bot_Commands["MAP"] = new Bot_Command(0,false,false);
Bot_Commands["MAP"].usage = "MAP [ <user|location> HILLS=<num> WIDTH=<num> HEIGHT=<num> SCALE=<miles> INFO MODE=(ISLAND|LAKE|BORDER) (SECTION X=<num>|Y=<num>) ]";
Bot_Commands["MAP"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(!cmd[0]) {
		coords=getNickLocation(srv,onick);
		mapDisplay(srv,target);
		return;
	}
	while(cmd.length > 0) {
		var c = cmd.shift().split("=");
		
		switch(c[0].toUpperCase()) {
		case "SCALE":
			if(isNaN(c[1]))
				srv.o(target,"Try again, faggit.");
			else if(c[1] > 4000 || c[1] < 1) 
				srv.o(target,"Fuck yourself.");
			else
				Google.scale = c[1];
			break;
		case "HILLS":
			if(isNaN(c[1]))
				srv.o(target,"Try again, faggit.");
			else if(c[1] > 5000 || c[1] < 1) 
				srv.o(target,"Fuck yourself.");
			else
				map.hills = c[1];
			break;
		case "WIDTH":
			if(isNaN(c[1]))
				srv.o(target,"Try again, faggit.");
			else if(c[1] > 80 || c[1] < 5) 
				srv.o(target,"Fuck yourself.");
			else
				mapWidth = c[1];
			break;
		case "HEIGHT":
			if(isNaN(c[1]))
				srv.o(target,"Try again, faggit.");
			else if(c[1] > 200 || c[1] < 5) 
				srv.o(target,"Fuck yourself.");
			else
				mapHeight = c[1];
			break;
		case "MODE":
			if(!c[1]) {
				srv.o(target,"Try again, faggit.");
				break;
			}
			map.island = false;
			map.lake = false;
			map.border = false;
			
			var modes = c[1].split("|");
			while(modes.length > 0) {
				switch(modes.shift().toUpperCase()) {
				case "ISLAND":
					map.island = true;
					break;
				case "LAKE":
					map.lake = true;
					break;
				case "BORDER":
					map.border = true;
					break;
				case "NONE":
					break;
				}
			}
			break;
		case "MINR":
			if(isNaN(c[1]))
				srv.o(target,"Try again, faggit.");
			else if(c[1] > 1000 || c[1] <= 0) 
				srv.o(target,"Fuck yourself.");
			else {
				if(c[1] > map.maxRadius)
					map.maxRadius = c[1];
				map.minRadius = c[1];
			}
			break;
		case "MAXR":
			if(isNaN(c[1]))
				srv.o(target,"Try again, faggit.");
			else if(c[1] > 1000 || c[1] <= 0) 
				srv.o(target,"Fuck yourself.");
			else {
				if(c[1] < map.minRadius)
					map.minRadius = c[1];
				map.maxRadius = c[1];
			}
			break;
		case "BASE":
			if(isNaN(c[1]))
				srv.o(target,"Try again, faggit.");
			else if(c[1] > 100000 || c[1] <= -100000 || c[1] >= map.peak) 
				srv.o(target,"Fuck yourself");
			else
				map.base = c[1];
			break;
		case "PEAK":
			if(isNaN(c[1]))
				srv.o(target,"Try again, faggit.");
			else if(c[1] > 100000 || c[1] <= -100000 || c[1] <= map.base) 
				srv.o(target,"Fuck yourself.");
			else
				map.peak = c[1];
			break;
		case "RANDOM":
			map.init(mapWidth,mapHeight);
			map.randomize();
			showMap(map,srv,target)
			break;
		case "SECTION":
			if(!cmd[0]) {
				srv.o(target,"Try again, faggit.");
				return;
			}
			var sec = cmd.shift().split("=");
			if(isNaN(sec[1])) {
				srv.o(target,"Fuck yourself.");
				return;
			}
			switch(sec[0].toUpperCase()) {
			case "X":
				var section = map.ySection(sec[1]);
				showSection(map,section,srv,target);
				return;
			case "Y":
				var section = map.xSection(sec[1]);
				showSection(map,section,srv,target);
				return;
			default:
				srv.o(target,"Fuck yourself.");
				return;
			}
			break;
		case "INFO":
			if(coords) 
				showGoogleInfo(srv,target);
			else 
				showGeneratorInfo(srv,target);
			showRangeInfo(srv,target);
			break;
		default:
			if(srv.users[c[0].toUpperCase()]) {
				coords=getNickLocation(srv,c[0]);
			}
			else  {
				coords=Google.getLocationByAddress(cmd.join(" "));
				if(coords==undefined)
					srv.o(target, "Could not locate " + cmd.join(" "));
			}
			mapDisplay(srv,target);
			return;
		}
	}
	return true;
}

Bot_Commands["DISTANCE"] = new Bot_Command(0,false,false);
Bot_Commands["DISTANCE"].usage = "DISTANCE FROM=<user/location> TO=<user/location> [ AVOID=(tolls|highways) UNITS=(imperial|metric) MODE=(walking|driving|bicycling) ]";
Bot_Commands["DISTANCE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(!cmd[0]) {
		return;
	}
		
	var avoid;
	var from;
	var to;
	var mode;
	var units;
	
	for(var i=0;i<cmd.length;i++) {
		var c = cmd[i].split("=");
		
		switch(c[0].toUpperCase()) {
		case "AVOID":
			if(c[1] == "tolls" || c[1] == "highways")
				avoid = c[1];
			break;
		case "MODE":
			if(c[1] == "driving" || c[1] == "walking" || c[1] == "bicycling")
				mode = c[1];
			break;
		case "UNITS":
			if(c[1] == "imperial" || c[1] == "metric")
				units = c[1];
			break;
		case "FROM":
			var f = c[1];
			for(var j=i+1;j<cmd.length;j++) {
				var k = cmd[j].split("=")[0].toUpperCase();
				if(k == "AVOID" || k == "MODE" || k == "UNITS" || k == "TO")
					break;
				f += "+" + cmd[j];
			}
			if(f.length > 0)
				from = f;
			break;
		case "TO":
			var t = c[1];
			for(var j=i+1;j<cmd.length;j++) {
				var k = cmd[j].split("=")[0].toUpperCase();
				if(k == "AVOID" || k == "MODE" || k == "UNITS" || k == "FROM")
					break;
				t += "+" + cmd[j];
			}
			if(t.length > 0)
				to = t;
			break;
		}
	}
	
	/* if no starting point has been specified, use user location */
	if(from == undefined) {
		var loc = getNickLocation(srv,onick);
		from = loc.latitude + "," + loc.longitude;
	}
	
	if(to == undefined) {
		srv.o(target,"destination not specified. usage: distance from=<location> to=<location> mode=<walking|bicycling|driving> avoid=<tolls|highways> units=<imperial|metric>");
		return;
	}

	if(srv.users[to.toUpperCase()]) {
		var loc = getNickLocation(srv,to);
		to = loc.latitude + "," + loc.longitude;
	}

	if(srv.users[from.toUpperCase()]) {
		var loc = getNickLocation(srv,from);
		from = loc.latitude + "," + loc.longitude;
	}

	var d = Google.getDistance(from,to,avoid,mode,units);
	if(d.distance) {
		srv.o(target,"Distance: " + d.distance.text + " Duration: " + d.duration.text);
	}
}

Bot_Commands["DIRECTIONS"] = new Bot_Command(0,false,false);
Bot_Commands["DIRECTIONS"].usage = "DIRECTIONS FROM=<user/location> TO=<user/location> [ AVOID=(tolls|highways) UNITS=(imperial|metric) MODE=(walking|driving|bicycling) WAYPOINT=<user/location> ]";
Bot_Commands["DIRECTIONS"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(!cmd[0]) {
		return;
	}
		
	var waypoint;
	var avoid;
	var from;
	var to;
	var mode;
	var units;
	
	for(var i=0;i<cmd.length;i++) {
		var c = cmd[i].split("=");
		
		switch(c[0].toUpperCase()) {
		case "AVOID":
			if(c[1] == "tolls" || c[1] == "highways")
				avoid = c[1];
			break;
		case "MODE":
			if(c[1] == "driving" || c[1] == "walking" || c[1] == "bicycling")
				mode = c[1];
			break;
		case "UNITS":
			if(c[1] == "imperial" || c[1] == "metric")
				units = c[1];
			break;
		case "FROM":
			var f = c[1];
			for(var j=i+1;j<cmd.length;j++) {
				var k = cmd[j].split("=")[0].toUpperCase();
				if(k == "AVOID" || k == "MODE" || k == "UNITS" || k == "TO")
					break;
				f += "+" + cmd[j];
			}
			if(f.length > 0)
				from = f;
			break;
		case "WAYPOINT":
			var w = c[1];
			for(var j=i+1;j<cmd.length;j++) {
				var k = cmd[j].split("=")[0].toUpperCase();
				if(k == "AVOID" || k == "MODE" || k == "UNITS" || k == "FROM" || k == "TO")
					break;
				w += "+" + cmd[j];
			}
			if(w.length > 0)
				waypoint = w;
			break;
		case "TO":
			var t = c[1];
			for(var j=i+1;j<cmd.length;j++) {
				var k = cmd[j].split("=")[0].toUpperCase();
				if(k == "AVOID" || k == "MODE" || k == "UNITS" || k == "FROM" || k == "WAYPOINT")
					break;
				t += "+" + cmd[j];
			}
			if(t.length > 0)
				to = t;
			break;
		}
	}
	
	/* if no starting point has been specified, use user location */
	if(from == undefined) {
		var loc = getNickLocation(srv,onick);
		from = loc.latitude + "," + loc.longitude;
	}
	
	if(to == undefined) {
		srv.o(target,"destination not specified. usage: directions from=<location> to=<location> mode=<walking|bicycling|driving> avoid=<tolls|highways> units=<imperial|metric>");
		return;
	}

	if(srv.users[to.toUpperCase()]) {
		var loc = getNickLocation(srv,to);
		to = loc.latitude + "," + loc.longitude;
	}
	
	if(srv.users[from.toUpperCase()]) {
		var loc = getNickLocation(srv,from);
		from = loc.latitude + "," + loc.longitude;
	}
	
	var d = Google.getDirections(from,to,avoid,mode,units,waypoint);
	if(!d || !d.legs) {
		srv.o(target,"No routes found.");
		return;
	}
		
	for(var l=0;l<d.legs.length;l++) {
		var leg = d.legs[l];
		srv.o(target,format("Leg %d: %s - %s",l+1,leg.distance.text,leg.duration.text));
		for(var s=0;s<leg.steps.length;s++) {
			var step = leg.steps[s];
			var inst = step.html_instructions.replace(/<[^>]*>/g,' ').replace(/\s+/g,' ');
			var dist = step.distance.text;
			var dur = step.duration.text;
			srv.o(target,format("%s(%s - %s)",inst,dist,dur));
		}
	}
}
