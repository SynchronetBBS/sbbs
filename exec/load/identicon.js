require('graphic.js', 'Graphic');

function identicon(value)
{
	if (value === null)
		value = user.alias;
	if (value === undefined)
		value = ascii(1)+"<undefined>";

	var hash = md5_calc(value, true);
	var bits = 0;
	var bv = 0;
	var tb;

	function get_bits(cnt) {
		if (cnt === undefined)
			cnt = 1;
		while (bits < cnt) {
			var s = hash[0];
			hash = hash.substr(1);
			bv |= (parseInt(s, 16) << bits);
			bits += 4;
		}
		var ret = bv & ((1 << cnt) - 1);
		bv >>= cnt;
		bits -= cnt;
		return ret;
	}

	var fg = get_bits(3);
	var bg = 7 - fg;
	var br = 8;
	if (fg == 4)
		br = 0;
	var c = fg | (bg << 4) | br;
	var g = new Graphic(10, 6, c);
	const chars = [' ', '\xdb', '\xdc', '\xdb'];
	var ch;
	for (x = 0; x < 5; x++) {
		for (y = 0; y < 6; y++) {
			tb = chars[get_bits(2)];
			g.data[x][y].ch = tb;
			g.data[9-x][y].ch = tb;
		}
	}
	return g;
}

this;
