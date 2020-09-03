function Maidenhead(locator, latitude, longitude, precision)
{
	this.locator = locator;
	this.longitude = longitude;
	this.latitude = latitude;
	this.precision = precision;
	var lop;
	var lap;
	var ucl;
	var stupidhack;
	var i;

	function parselocator(locator) {
		function add_pos(subloc, pos) {
			if (this.positions[pos].range == 10) {
				lop = subloc.charCodeAt(0) - '0'.charCodeAt();
				lap = subloc.charCodeAt(1) - '0'.charCodeAt();
			} else {
				lop = subloc.charCodeAt(0) - 'A'.charCodeAt();
				lap = subloc.charCodeAt(1) - 'A'.charCodeAt();
			}

			if (lap < 0 || lop < 0 || lap >= this.positions[pos].range || lop >= this.positions[pos].range)
				throw('invalid field in locator');
			this.longitude += this.positions[pos].precision * lop;
			this.latitude += this.positions[pos].precision / 2 * lap;
		}

		ucl = locator.toUpperCase();
		if (ucl.length > 18)
			throw('locator too precise');
		if (ucl.length & 1)
			throw('locator length is odd');

		this.latitude = -90.0;
		this.longitude = -180.0;
		this.precision = 0;
		while (ucl.length >= 2) {
			add_pos.call(this, ucl.substr(0, 2), this.precision);
			ucl = ucl.substr(2);
			this.precision += 1;
		}

		this.south_west = {'longitude': this.longitude, 'latitude': this.latitude};
		this.north_west = {'longitude': this.longitude, 'latitude': this.latitude + this.positions[this.precision-1].precision / 2};
		this.south_east = {'longitude': this.longitude + this.positions[this.precision-1].precision, 'latitude': this.latitude};
		this.north_east = {'longitude': this.longitude + this.positions[this.precision-1].precision, 'latitude': this.latitude + this.positions[this.precision-1].precision / 2};
		this.longitude += this.positions[this.precision-1].precision / 2;
		this.latitude += this.positions[this.precision-1].precision / 4;
		this.locator = locator;
	}

	function sub_pos(hack, pos) {
		lop = parseInt(hack.longitude / this.positions[pos].precision, 10);
		lap = parseInt(hack.latitude / (this.positions[pos].precision / 2), 10);
		if (lap < 0 || lop < 0 || lap >= this.positions[pos].range || lop >= this.positions[pos].range)
			throw('invalid latitude or longitude');
		if (this.positions[pos].range == 10) {
			hack.locator = hack.locator + String.fromCharCode('0'.charCodeAt()+lop);
			hack.locator = hack.locator + String.fromCharCode('0'.charCodeAt()+lap);
		} else if (this.positions[pos].range == 18) {
			hack.locator = hack.locator + String.fromCharCode('A'.charCodeAt()+lop);
			hack.locator = hack.locator + String.fromCharCode('A'.charCodeAt()+lap);
		} else {
			hack.locator = hack.locator + String.fromCharCode('a'.charCodeAt()+lop);
			hack.locator = hack.locator + String.fromCharCode('a'.charCodeAt()+lap);
		}
		hack.longitude -= lop * this.positions[pos].precision;
		hack.latitude -= lap * (this.positions[pos].precision / 2);
	}

	if (locator !== undefined) {
		parselocator.call(this, locator);
	} else {
		if (latitude === undefined || longitude === undefined || precision === undefined)
			throw('must specify locator or lat, lon, precision');
		if (latitude < -90 || latitude > 90)
			throw('Invalid latitude');
		if (longitude < -180 || longitude > 180)
			throw('Invalid longitude');
		stupidhack = {'latitude': latitude + 90, 'longitude': longitude + 180, 'locator': ''};

		for (i=0; i<precision; i++)
			sub_pos.call(this, stupidhack, i);
		parselocator(stupidhack.locator);
	}

}

Maidenhead.prototype.positions = [
	{'precision': 20.0, 'range': 18},
	{'precision': 2.0, 'range':10},
	{'precision': 2.0/24, 'range': 24},
	{'precision': 2.0/240, 'range': 10},
	{'precision': 2.0/5760, 'range': 24},
	{'precision': 2.0/57600, 'range': 10},
	{'precision': 2.0/115200, 'range': 10},
	{'precision': 2.0/276480000, 'range': 24},
	{'precision': 2.0/2764800000, 'range': 10}
];

Maidenhead.prototype.distance = function(to)
{
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
	/* Latitude/longitude spherical geodesy tools                         (c) Chris Veness 2002-2017  */
	/*                                                                                   MIT Licence  */
	/* www.movable-type.co.uk/scripts/latlong.html                                                    */
	/* www.movable-type.co.uk/scripts/geodesy/docs/module-latlon-spherical.html                       */
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
	var radius;
	var rlatfrom;
	var rlatto;
	var rlonfrom;
	var rlonto;
	var rlondiff;
	var rlatdiff;
	var a;
	var c;

	if (!(to instanceof Maidenhead))
		throw('argument must be a Maidenhead');
	radius = 6371e3;
	rlatfrom = this.latitude * (Math.PI / 180);
	rlatto = to.latitude * (Math.PI / 180);
	rlonfrom = this.longitude * (Math.PI / 180);
	rlonto = to.longitude * (Math.PI / 180);
	rlondiff = rlonto - rlonfrom;
	rlatdiff = rlatto - rlatfrom;
	a = Math.sin(rlatdiff/2)*Math.sin(rlatdiff/2)+Math.cos(rlatfrom)*Math.cos(rlatto)*Math.sin(rlondiff/2)*Math.sin(rlondiff/2);
	c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a));
	return radius * c;
};

Maidenhead.prototype.bearing = function(to)
{
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
	/* Latitude/longitude spherical geodesy tools                         (c) Chris Veness 2002-2017  */
	/*                                                                                   MIT Licence  */
	/* www.movable-type.co.uk/scripts/latlong.html                                                    */
	/* www.movable-type.co.uk/scripts/geodesy/docs/module-latlon-spherical.html                       */
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
	var rlatfrom;
	var rlatto;
	var rlondiff;
	var y;
	var x;
	var rbear;

	if (!(to instanceof Maidenhead))
		throw('argument must be a Maidenhead');
	rlatfrom = this.latitude * (Math.PI / 180);
	rlatto = to.latitude * (Math.PI / 180);
	rlondiff = to.longitude - this.longitude;
	y = Math.sin(rlondiff) * Math.cos(rlatto);
	x = Math.cos(rlatfrom)*Math.sin(rlatto)-Math.sin(rlatfrom)*Math.cos(rlatto)*Math.cos(rlondiff);
	rbear = Math.atan2(y, x);
	return ((rbear / (Math.PI / 180))+360) % 360;
};

Maidenhead.prototype.vdistance = function(to)
{
	if (!(to instanceof Maidenhead))
		throw('argument must be a Maidenhead');
	try {
		return this.vinverse(to).distance;
	} catch (e) {
		return undefined;
	}
};

Maidenhead.prototype.vbearing = function(to)
{
	if (!(to instanceof Maidenhead))
		throw('argument must be a Maidenhead');
	try {
		return this.vinverse(to).initialBearing;
	} catch (e) {
		return undefined;
	}
};

Maidenhead.prototype.vinverse = function(to)
{
	// Adapted from: https://www.movable-type.co.uk/scripts/latlong-vincenty.html
	// using the WGS-84 ellipsoid.
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
	/* Vincenty Direct and Inverse Solution of Geodesics on the Ellipsoid (c) Chris Veness 2002-2017  */
	/*                                                                                   MIT Licence  */
	/* www.movable-type.co.uk/scripts/latlong-vincenty.html                                           */
	/* www.movable-type.co.uk/scripts/geodesy/docs/module-latlon-vincenty.html                        */
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
	var p1, p2;
	var rlat1, rlat2;
	var a, b, f;
	var L;
	var tanU1, cosU1, sinU1, tanU2, cosU2, sinU2;
	var sinsig, cossig, sig, cosSqalpg, cos2sigM;
	var rlon, iterations, antimeridian;
	var sinrlon, cosrlon;
	var sinSqsig, sinalph, cidenheaosSqalph, cosSqalph, C, rlonprime, iterationCheck;
	var uSq, A, B, delatsig, s, alph1, alph2, ret, rlon1, rlon2;

	if (!(to instanceof Maidenhead))
		throw('argument must be a Maidenhead');
	p1 = {'lat': this.latitude, 'lon': this.longitude};
	p2 = {'lat': to.latitude, 'lon': to.longitude};
	if (p1.lon == -180.0)
		p1.lon = 180.0;
	rlat1 = p1.lat * (Math.PI / 180);
	rlon1 = p1.lon * (Math.PI / 180);
	rlat2 = p2.lat * (Math.PI / 180);
	rlon2 = p2.lon * (Math.PI / 180);

	// TODO: Get these values...
	a = 6378137.0;
	b = 6356752.3142;
	f = 1.0/298.257223563;

	L = rlon2 - rlon1;
	tanU1 = (1.0-f) * Math.tan(rlat1);
	cosU1 = 1.0 / Math.sqrt((1.0 + tanU1*tanU1));
	sinU1 = tanU1 * cosU1;
	tanU2 = (1.0-f) * Math.tan(rlat2);
	cosU2 = 1.0 / Math.sqrt((1.0 + tanU2*tanU2));
	sinU2 = tanU2 * cosU2;

	sinsig=0.0;
	cossig=0.0;
	sig=0.0;
	cosSqalph=0.0;
	cos2sigM=0.0;

	rlon = L;
	iterations = 0.0;
	antimeridian = Math.abs(L) > Math.PI;

	for (;;) {
		sinrlon = Math.sin(rlon);
		cosrlon = Math.cos(rlon);
		sinSqsig = (cosU2*sinrlon) * (cosU2*sinrlon) + (cosU1*sinU2-sinU1*cosU2*cosrlon) * (cosU1*sinU2-sinU1*cosU2*cosrlon);
		if (sinSqsig == 0)
			break; // co-incident points
		sinsig = Math.sqrt(sinSqsig);
		cossig = sinU1*sinU2 + cosU1*cosU2*cosrlon;
		sig = Math.atan2(sinsig, cossig);
		sinalph = cosU1 * cosU2 * sinrlon / sinsig;
		cidenheaosSqalph = 1 - sinalph*sinalph;
		if (cosSqalph != 0)
			cos2sigM = cossig - 2.0*sinU1*sinU2/cosSqalph;
		else
			cos2sigM = 0.0; // equatorial
		C = f/16.0*cosSqalph*(4.0+f*(4.0-3.0*cosSqalph));
		rlonprime = rlon;
		rlon = L + (1.0-C) * f * sinalph * (sig + C*sinsig*(cos2sigM+C*cossig*(-1.0+2.0*cos2sigM*cos2sigM)));
		if (antimeridian)
			iterationCheck = Math.abs(rlon)-Math.PI;
		else
			iterationCheck = Math.abs(rlon);
		if (iterationCheck > Math.PI)
			throw('rlon > pi');
		iterations += 1;
		if (iterations >= 1000)
			throw('Formula failed to converge');
		if (Math.abs(rlon-rlonprime) <= 1e-12)
			break;
	}

	uSq = cosSqalph * (a*a - b*b) / (b*b);
	A = 1.0 + uSq/16384.0*(4096.0+uSq*(-768.0+uSq*(320.0-175.0*uSq)));
	B = uSq/1024.0 * (256.0+uSq*(-128.0+uSq*(74.0-47.0*uSq)));
	delatsig = B*sinsig*(cos2sigM+B/4.0*(cossig*(-1.0+2.0*cos2sigM*cos2sigM)-
		B/6.0*cos2sigM*(-3.0+4.0*sinsig*sinsig)*(-3.0+4.0*cos2sigM*cos2sigM)));

	s = b*A*(sig-delatsig);

	alph1 = Math.atan2(cosU2*sinrlon,  cosU1*sinU2-sinU1*cosU2*cosrlon);
	alph2 = Math.atan2(cosU1*sinrlon, -sinU1*cosU2+cosU1*sinU2*cosrlon);

	alph1 = (alph1 + 2.0*Math.PI) % (2.0*Math.PI); // normalise to 0..360
	alph2 = (alph2 + 2.0*Math.PI) % (2.0*Math.PI); // normalise to 0..360

	ret = { 'distance': s, 'iterations': iterations};
	if (s === 0) {
		ret.initialBearing = undefined;
		ret.finalBearing = undefined;
	} else {
		ret.initialBearing = alph1 / (Math.PI / 180);
		ret.finalBearing = alph2 / (Math.PI / 180);
	}

	return ret;
};
