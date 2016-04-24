require("unicode_cp437.js", 'unicode_cp437');

function utf8_cp437(uni)
{
	return uni.replace(/[\xc0-\xfd][\x80-\xbf]+/g, function(ch) {
		var i;
		var uc = ch.charCodeAt(0);
		for (i=7; i>0; i--) {
			if ((uc & 1<<i) == 0)
				break;
			uc &= ~(1<<i);
		}
		for (i=1; i<ch.length; i++) {
			uc <<= 6;
			uc |= ch.charCodeAt(i) & 0x3f;
		}

		return unicode_cp437(uc);
	});
}
