// $Id$

// Support for RFC2047:
//
//        MIME (Multipurpose Internet Mail Extensions) Part Three:
//              Message Header Extensions for Non-ASCII Text

// Returns an array of 'encoded-words' 
function decode(hvalue)
{
	var result = [];
	
	var list = hvalue.split(/\s+/);
	for(var i in list) {
		var str = list[i];
		var retval = { charset: 'unspecified (US-ASCII)', data: str };

		var match = str.match(/^\=\?([a-zA-Z0-9-]+)\?(.)\?([^ ?]+)\?\=$/);
		if(!match) {
			result.push(retval);
			continue;
		}
		
		retval.charset = match[1].toLowerCase();
		switch(match[2].toLowerCase()) {
			case 'q':	// Quoted-printable
				retval.data = match[3].replace(/=([0-9A-F][0-9A-F])/g, function(str, p1) { return(ascii(parseInt(p1,16))); });
				break;
			case 'b':	// Base64
				retval.data = base64_decode(match[3]);
				break;
		}
		result.push(retval);
	}
	return result;
}

this;