// $Id$

if(!js.global || js.global.HTTPRequest==undefined)
	load("http.js");

var geoipAPIKey='a1ddc4963461ca20bffd54bb926ce74dc1ecbb8a421122cdc3cdfef616f5aad1';	// Enter your API info here!

if(geoipAPIKey==undefined) {
	log("You need to get a key from http://ipinfodb.com/register.php");
	exit;
}

function get_geoip(host, countryonly)
{
	var GeoIP;
	var m,i,j;
	var ishost=false;
	var geoip_url;
	var result;
	var tmpobj={};
	var isarray=(typeof(host)=='object' && host instanceof Array);

	/*
	 * Check if this is an IP address or a hostname...
	 * (We'll ignore weird encodings though like 12345.12345)
	 */
	if(!isarray) {
		m=host.match(/^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$/);
		if(m!=null) {
			if(m.length==5) {
				for(i=1; i<5; i++) {
					j=parseInt(m[i]);
					if(j < 0 || j > 255) {
						ishost=true;
						break;
					}
				}	
			}
		}
		else
			ishost=true;
	}

	// Get the best URL
	if(countryonly) {
		if(isarray)
			geoip_url='http://api.ipinfodb.com/v3/ip-country/?key='+geoipAPIKey+'&format=json&ip='+encodeURIComponent(host.join(','));
		else if(ishost)
			geoip_url='http://api.ipinfodb.com/v3/ip-country/?key='+geoipAPIKey+'&format=json&ip='+encodeURIComponent(host);
		else
			geoip_url='http://api.ipinfodb.com/v3/ip-country/?key='+geoipAPIKey+'&format=json&ip='+encodeURIComponent(host);
	}
	else {
		if(isarray)
			geoip_url='http://api.ipinfodb.com/v3/ip-city/?key='+geoipAPIKey+'&format=json&ip='+encodeURIComponent(host.join(','));
		else if(ishost)
			geoip_url='http://api.ipinfodb.com/v3/ip-city/?key='+geoipAPIKey+'&format=json&ip='+encodeURIComponent(host);
		else
			geoip_url='http://api.ipinfodb.com/v3/ip-city/?key='+geoipAPIKey+'&format=json&ip='+encodeURIComponent(host);
	}
	try {
		do {
			result='ret='+new HTTPRequest().Get(geoip_url);
			GeoIP=js.eval(result);
		} while(GeoIP==undefined);
		if(GeoIP.Locations != undefined) {
			if(isarray)
				return GeoIP.Locations;
			return GeoIP.Locations[0];
		}
		return GeoIP;
	}
	catch(e) {}
}
