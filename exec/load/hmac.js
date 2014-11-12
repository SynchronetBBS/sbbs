load("sha1.js");

function hmac_sha1(key, message)
{
	function hex2bin(hexstr) {
		var i;
		var ret='';

		if(hexstr.length % 2)
			hexstr += '0';
		for (i=0; i<hexstr.length; i+=2)
			ret += ascii(parseInt(hexstr.substr(i, 2), 16));
		return ret;
	}

	var okey = '';
	var ikey = '';
	var i;
	var tmp;

	if (key.length > 64)
		key = hex2bin(Sha1.hash(key, false));
	while (key.length < 64)
		key += '\x00';

	for (i=0; i<key.length; i++) {
		tmp = ascii(key.substr(i, 1));
		okey += ascii(tmp ^ 0x5c);
		ikey += ascii(tmp ^ 0x36);
	}

	tmp = hex2bin(Sha1.hash(ikey + message, false));
	return hex2bin(Sha1.hash(okey + tmp, false));
}

function hmac_md5(key, message)
{
	function hex2bin(hexstr) {
		var i;
		var ret='';

		if(hexstr.length % 2)
			hexstr += '0';
		for (i=0; i<hexstr.length; i+=2)
			ret += ascii(parseInt(hexstr.substr(i, 2), 16));
		return ret;
	}

	var okey = '';
	var ikey = '';
	var i;
	var tmp;

	if (key.length > 64)
		key = hex2bin(md5_calc(key, true));
	while (key.length < 64)
		key += '\x00';

	for (i=0; i<key.length; i++) {
		tmp = ascii(key.substr(i, 1));
		okey += ascii(tmp ^ 0x5c);
		ikey += ascii(tmp ^ 0x36);
	}

	tmp = hex2bin(md5_calc(ikey + message, true));
	return hex2bin(md5_calc(okey + tmp, true));
}
