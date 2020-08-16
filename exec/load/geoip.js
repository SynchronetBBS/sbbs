// $Id: geoip.js,v 1.13 2019/08/24 04:28:30 echicken Exp $

require('http.js', 'HTTPRequest');
var settings = load('modopts.js', 'geoip');

if (settings && settings.api_key) {
    var geoipAPIKey = settings.api_key;
} else {
	var geoipAPIKey = 'a1ddc4963461ca20bffd54bb926ce74dc1ecbb8a421122cdc3cdfef616f5aad1';	// Enter your API info here!
	log("You should get an API key from http://ipinfodb.com/register.php");
}

function get_geoip(host, countryonly)
{
	var GeoIP;
	var m,i,j;
	var geoip_url;
	var result;
	var tmpobj={};
	var isarray=(typeof(host)=='object' && host instanceof Array);

	if(isarray) {
		for(i in host)
			host[i]=resolve_ip(host[i]);
	}
	else
		host=resolve_ip(host);

	// Get the best URL
	if(countryonly) {
		if(isarray)
			geoip_url='http://api.ipinfodb.com/v3/ip-country/?key='+geoipAPIKey+'&format=json&ip='+encodeURIComponent(host.join(','));
		else
			geoip_url='http://api.ipinfodb.com/v3/ip-country/?key='+geoipAPIKey+'&format=json&ip='+encodeURIComponent(host);
	}
	else {
		if(isarray)
			geoip_url='http://api.ipinfodb.com/v3/ip-city/?key='+geoipAPIKey+'&format=json&ip='+encodeURIComponent(host.join(','));
		else
			geoip_url='http://api.ipinfodb.com/v3/ip-city/?key='+geoipAPIKey+'&format=json&ip='+encodeURIComponent(host);
	}
	for(i=0; i<10; i++) {
		try {
			result='ret='+new HTTPRequest().Get(geoip_url);
			GeoIP=js.eval(result);
			if(GeoIP==undefined)
				continue;
			if(GeoIP.Locations != undefined) {
				if(isarray)
					return GeoIP.Locations;
				if(!GeoIP.Locations[0].countryName)
					continue;
				return GeoIP.Locations[0];
			}
			if(!GeoIP.countryName)
				continue;
			return GeoIP;
		}
		catch(e) {
		}
	}
}
