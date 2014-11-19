function encode_integer(val)
{
	var ret ='';

	val = parseInt(val, 10);
	if (val < 0)
		val = Math.pow(2, 32) + val;
	while(val) {
		ret = ascii(val & 0xff)+ret;
		// We can't shift here or it will convert to 32-bit signed first.
		val = Math.floor(val/256);
	}
	while (ret.length < 2)
		ret = ascii(0)+ret;
	if (ret.length == 3)
		ret = ascii(0)+ret;
	if (ret.length == 1 || ret.length == 0 || ret.length == 3 || ret.length > 4)
		rage_quit("Invalid int length!");
	return ret;
}

function encode_long(val)
{
	var ret= encode_integer(val);

	while (ret.length < 4)
		ret = ascii(0)+ret;
	return ret;
}

function encode_short(val)
{
	var ret = encode_integer(val);

	if (ret.length > 2)
		rage_quit("Integer too long!");
	return ret;
}

function address_to_bin(addr)
{
	var m;
	var i, j;
	var ret = '';

	m = addr.match(/^([0-9]+)\.([0-9]+)\.([0-9]+)\.([0-9]+)$/);
	if (m != null) {
		for (i=1; i<=4; i++)
			ret += ascii(parseInt(m[i], 10));
		return ret;
	}
	else {
		ret = '\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00';

		var blocks = addr.split(/:/);
		for (i = 0; i<blocks.length; i++) {
			if (blocks[i]=='')
				break;
			while(blocks[i].length < 4)
				blocks[i] = '0'+blocks[i];
			ret = ret.substr(0, i*2)+ascii(parseInt(blocks[i].substr(0,2), 16))+ascii(parseInt(blocks[i].substr(2, 2), 16)) + ret.substr(i*2 + 2);
		}
		if (i != 8) {
			// Work backward now...
			for (j = blocks.length - 1; j >=0; j--) {
				if (blocks[j]=='')
					break;
				while(blocks[j].length < 4)
					blocks[j] = '0'+blocks[j];
				ret = ret.substr(0, (8 - (blocks.length - j))*2)+ascii(parseInt(blocks[j].substr(0,2), 16))+ascii(parseInt(blocks[j].substr(2, 2), 16)) + ret.substr((8 - (blocks.length - j))*2 + 2);
			}
		}
		return ret;
	}
}

function time_to_timestamp(time)
{
	var ret ='';
	var d = new Date(time*1000);
	var y = new Date(time*1000);

	y.setMonth(0);
	y.setDate(1);
	y.setHours(0);
	y.setMinutes(0);
	y.setSeconds(0);
	y.setMilliseconds(0);
	ret = encode_short(d.getYear()+1900);
	ret += encode_short(0);
	ret += encode_long((d.valueOf()-y.valueOf())/1000);
	return ret;
}

function decode_integer(val)
{
	var ret = 0;

	while(val.length) {
		ret <<= 8;
		ret += ascii(val.substr(0,1));
		val = val.substr(1);
	}
	return ret;
}
