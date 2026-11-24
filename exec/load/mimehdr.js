// Support for RFC2047:
//
//        MIME (Multipurpose Internet Mail Extensions) Part Three:
//              Message Header Extensions for Non-ASCII Text

require("utf8_cp437.js", 'utf8_cp437');

// Returns an array of 'encoded-words'
function decode(hvalue)
{
	var result = [];
	var encoded_word = /^\=\?([a-zA-Z0-9-]+)\?(.)\?([^ ?]*)\?\=/;
	var regex = /(\=\?[a-zA-Z0-9-]+\?.\?[^ ?]*\?\=)(\s*)|\S+\s*/g;
	var word;

	while(hvalue && (word = regex.exec(hvalue)) !== null) {
		var retval = { charset: 'unspecified (US-ASCII)', data: word[0] };

		if(!word[1]) {
			result.push(retval);
			continue;
		}
		var match = word[1].match(encoded_word);

		retval.data = word[1];
		retval.charset = match[1].toLowerCase();
		switch(match[2].toUpperCase()) {
			case 'Q':	// "similar to" Quoted-printable
				retval.data = match[3]
					.replace(/_/g, ' ')
					.replace(/=([0-9A-F][0-9A-F])/gi, function(str, p1) { return(ascii(parseInt(p1,16))); });
				break;
			case 'B':	// Base64
				retval.data = base64_decode(match[3]);
				break;
		}
		result.push(retval);
		// White space is dropped only between two adjacent encoded words
		if(word[2] && !encoded_word.test(hvalue.substring(regex.lastIndex)))
			result.push({ charset: 'unspecified (US-ASCII)', data: word[2] });
	}
	return result;
}

// Translate a MIME-encoded header field value to a CP437 string
function to_cp437(val)
{
	var result = [];
	var words = mimehdr.decode(val);
	for(i in words) {
		var word = words[i];
		word.data = strip_ctrl(word.data);
		switch(word.charset) {
			case 'utf-8':
				if(word.data)
					result.push(utf8_cp437(word.data));
				else
					result.push(word.data);
				break;
			default:
			case 'cp437':
			case 'ibm437':
			case 'us-ascii':
				result.push(word.data);
				break;
		}
	}
	return result.join('');
}

function encode_word(word)
{
	var output = "=?utf-8?q?";
	for(var i in word) {
		var ch = word[i];
		try {
			if(ascii(ch) < 0x80 && ch != '=')
				output += ch;
			else
				output += format("=%02X", ascii(ch));
		} catch(e) {}
	}
	return output + "?=";
}

function encode(val)
{
	if(str_is_ascii(val))
		return val;

	var output = '';
	var words = val.split(/\s+/g);
	for(var i in words) {
		if(output.length)
			output += " ";
		var word = words[i];
		if(str_is_ascii(word)) {
			output += word;
			continue;
		}
		if(!str_is_utf8(word))
			word = utf8_encode(word);
		output += encode_word(word);
	}
	return output;
}

this;
