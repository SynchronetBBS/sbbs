js.global.load(js.global,"googleapis.js");
js.global.load(js.global,"mapgenerator.js");
js.global.load(js.global,"ircansi.js");
js.global.load(js.global,"geoip.js");

var coords = undefined;
var mapWidth = 30;
var mapHeight = 15;

var map = new Map(mapWidth,mapHeight);

function getNickLocation(srv,nick) {
	
	var userhost = getUserHost(srv,nick);
	var location = get_geoip(userhost);
	
	if(location == undefined) {
		location=getHostLocation(userhost);
	}
	if(location == undefined) {
		userhost=srv.users[nick.toUpperCase()].servername
		location=getHostLocation(userhost);
	}
	if(location == undefined) {
		var usr = new User(system.matchuser(nick));
		if(typeof(usr)=='object')
			location = Google.getLocationByAddress(usr.location);
	}
	
	return location;
}

function getHostLocation(host) {
	var location=Google.getLocationByHost(host);
	if(location == undefined)
		location = get_geoip(host);
	return location;
}

function mapDisplay(srv,target) {
	if(coords == undefined || isNaN(coords.latitude) || isNaN(coords.longitude))
		srv.o(target, "geoIP, it's motherfucked.");
	else {
		map.init(mapWidth,mapHeight);
		Google.getElevationMap(coords,mapWidth,mapHeight,map);
		showMap(map,srv,target);
	}			
}

function showGeneratorInfo(srv,target) {
	var mode = (map.island?"island ":"") + (map.lake?"lake ":"") + (map.border?"border":"");
	srv.o(target,"mode: " + (mode.length>0?mode:"normal") + " hills: " + map.hills);
	srv.o(target,"min/max radius: " + map.minRadius + "/" + map.maxRadius + " base/peak: " + map.base + "/" + map.peak);
}

function showGoogleInfo(srv,target) {
	srv.o(target,"latitude: " + coords.latitude.toFixed(4) + " longitude: " + coords.longitude.toFixed(4));
	srv.o(target,"scale: " + Google.scale + " width: " + mapWidth + " height: " + mapHeight);
}

function showRangeInfo(srv,target) {
	/* only display range data if it exists */
	if(map.range > 0) {
		srv.o(target,"min (m): " + map.min.toFixed(4) + " max (m): " + map.max.toFixed(4)); 
		srv.o(target,"mean (m): " + map.mean.toFixed(4) + " range (m): " + map.range.toFixed(4));
	}
}

function getUserHost(srv,nick) {
	var userhost=srv.users[nick.toUpperCase()].uh.replace(/^.*\@/,'');
	// If the hostname is not a FQDN, use the server name and replace the first element...
	if(userhost.indexOf('.')==-1)
		userhost += (srv.users[nick.toUpperCase()].servername.replace(/^[^\.]+\./,'.'));
	return userhost;
}

