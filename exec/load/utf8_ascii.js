// $Id: utf8_ascii.js,v 1.4 2019/07/24 22:05:29 rswindell Exp $

require("unicode_cp437.js", 'unicode_cp437');

function utf8_ascii(uni)
{
	return uni.replace(/[\xc0-\xfd][\x80-\xbf]+/g, function(ch) {
		var i;
		var uc = ch.charCodeAt(0);
		var nch;

		for (i=7; i>0; i--) {
			if ((uc & 1<<i) == 0)
				break;
			uc &= ~(1<<i);
		}
		for (i=1; i<ch.length; i++) {
			uc <<= 6;
			uc |= ch.charCodeAt(i) & 0x3f;
		}

		nch = unicode_cp437(uc);
		if (nch.charCodeAt(0) > 0x7f)
			nch = '?';
		return nch;
	});
}
